// Include Qt headers first, to avoid weirdness that the perl headers cause
#include <QtCore/QAbstractItemModel>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMetaMethod>
#include <QtCore/QMetaObject>
#include <QtCore/QModelIndex>
#include <QtCore/QRegExp>
#include <QtGui/QPainter>
#include <QtGui/QPaintEngine>
#include <QtGui/QPalette>
#include <QtGui/QIcon>
#include <QtGui/QBitmap>
#include <QtGui/QCursor>
#include <QtGui/QSizePolicy>
#include <QtGui/QKeySequence>
#include <QtGui/QTextLength>
#include <QtGui/QTextFormat>

// Perl headers
extern "C" {
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"
}

// Now my own headers
#include "smoke.h"
#include "Qt.h"
#include "binding.h"
#include "smokeperl.h"
#include "marshall_types.h" // Method call classes
#include "handlers.h" // for install_handlers function

// Standard smoke variables
extern Q_DECL_EXPORT Smoke* qt_Smoke;
extern Q_DECL_EXPORT void init_qt_Smoke();

// We only have one binding.
PerlQt::Binding binding;

// Global variables
SV* sv_this = 0;
SV* sv_qapp = 0;
HV* pointer_map = 0;
int do_debug = 0;

// There's a comment in QtRuby about possible memory leaks with these.
// Method caches, to avoid expensive lookups
QHash<QByteArray, Smoke::Index *> methcache;

smokeperl_object * 
alloc_smokeperl_object(bool allocated, Smoke * smoke, int classId, void * ptr) {
    smokeperl_object * o = new smokeperl_object;
	o->classId = classId;
	o->smoke = smoke;
	o->ptr = ptr;
	o->allocated = allocated;
    return o;
}

SV* allocSmokePerlSV ( void* ptr, SmokeType type ) {
    // The hash
    HV* hv = newHV();
    // The hash reference to return
    SV* var = newRV_noinc((SV*)hv);

    if ( type.classId() > 0 ) {
        // What package should I bless as?
        char *retpackage = binding.className(type.classId());
        // Bless the sv to that package.
        sv_bless( var, gv_stashpv(retpackage, TRUE) );
    }

    // Now we need to associate the pointer to the returned
    // value with the sv.
    smokeperl_object o;
    o.smoke = qt_Smoke;
    o.classId = type.classId();
    o.ptr = ptr;

    if(type.isStack())
        o.allocated = true;
    else
        o.allocated = false;

    // For this, we need a magic wand.  This is what actually
    // stores 'o' into our hash.
    sv_magic((SV*)hv, 0, '~', (char*)&o, sizeof(o));

    // Associate our vtbl_smoke with our sv, so that
    // smokeperl_free is called for us when the sv is destroyed
    MAGIC* mg = mg_find((SV*)hv, '~');
    mg->mg_virtual = &vtbl_smoke;

    // Store this into the ptr map for reference from virtual
    // function calls.
    if( SmokeClass( type ).hasVirtual() )
        mapPointer(var, &o, pointer_map, o.classId, 0);

    // We're done with our local var
    return var;
}

#ifdef DEBUG
void catRV( SV *r, SV *sv );
void catSV( SV *r, SV *sv );
void catAV( SV *r, AV *av );

void catRV( SV *r, SV *sv ) {
    smokeperl_object *o = sv_obj_info(sv);
    if(o)
        // Got a cxx type.
        sv_catpvf(r, "(%s*)0x%p",o->smoke->className(o->classId), o->ptr);
    else if (SvTYPE(SvRV(sv)) == SVt_PVMG)
        // Got a blessed hash
        sv_catpvf(r, "%s(%s)", HvNAME(SvSTASH(SvRV(sv))), SvPV_nolen(SvRV(sv)));
    else if (SvTYPE(SvRV(sv)) == SVt_PVAV) {
        // got an array ref
        catAV( r, (AV*)SvRV(sv) );
    }
    else
        sv_catsv(r, sv);
}

void catAV( SV *r, AV *av ) {
    long count = av_len( av ) + 1;
    sv_catpv(r, "[");
    for( long i = 0; i < count; ++i ) {
        if(i) sv_catpv(r, ", ");
        SV** item = av_fetch( av, i, 0 );
        if( !item )
            continue;
        else if(SvROK(*item))
            catRV(r, *item);
        else
            catSV(r, *item);
    }
    sv_catpv(r, "]");
}

void catSV( SV *r, SV *sv ) {
    bool isString = SvPOK(sv);
    STRLEN len;
    char *s = SvPV(sv, len);
    if(isString) sv_catpv(r, "'");
    sv_catpvn(r, s, len > 10 ? 10 : len);
    if(len > 10) sv_catpv(r, "...");
    if(isString) sv_catpv(r, "'");
}

// Args: SV** sp: the stack pointer containing the args to display
//       int n: the number of args
// Returns: An SV* containing a formatted string describing the arguments on
//          the stack
SV* catArguments(SV** sp, int n) {
    SV* r = newSVpv("", 0);
    for(int i = 0; i < n; i++) {
        if(i) sv_catpv(r, ", ");
        if(!SvOK(sp[i])) {
            // Not a valid sv, print undef
            sv_catpv(r, "undef");
        }
        else if(SvROK(sp[i])) {
            catRV(r, sp[i]);
        }
        else {
            catSV(r, sp[i]);
        }
    }
    return r;
}

#endif

const char* get_SVt(SV* sv) {
    const char* r;
    if(!SvOK(sv))
        r = "u";
    else if(SvIOK(sv))
        r = "i";
    else if(SvNOK(sv))
        r = "n";
    else if(SvPOK(sv))
        r = "s";
    else if(SvROK(sv)) {
        smokeperl_object *o = sv_obj_info(sv);
        if(!o) {
            switch (SvTYPE(SvRV(sv))) {
                case SVt_PVAV:
                    r = "a";
                    break;
                case SVt_PVMG: {
                    const char * classname = HvNAME(SvSTASH(SvRV(sv)));
                    if ( !strcmp( classname, "Qt::String" ) ) {
                        r = classname;
                    }
                    else {
                        // All enums are blessed scalars.
                        r = "e";
                    }
                    break;
                }
                default:
                    r = "r";
            }
        }
        else
            r = o->smoke->className(o->classId);
    }
    else
        r = "U";
    return r;
}

// The length of the QList returned from this will always be one more than the
// number of arguments that the signal call takes.  The first spot is the type
// of the return value of the signal.
// For custom signals, the first value will always be xmoc_void, because we
// don't populate a return type for custom signals.
QList<MocArgument*> getMocArguments(Smoke* smoke, const char * typeName, QList<QByteArray> methodTypes) {
    static QRegExp * rx = 0;
	if (rx == 0) {
		rx = new QRegExp("^(bool|int|uint|long|ulong|double|char\\*|QString)&?$");
	}
	methodTypes.prepend(QByteArray(typeName));
	QList<MocArgument*> result;

	foreach (QByteArray name, methodTypes) {
		MocArgument *arg = new MocArgument;
		Smoke::Index typeId = 0;

		if (name.isEmpty()) {
			arg->argType = xmoc_void;
			result.append(arg);
		} else {
			name.replace("const ", "");
			QString staticType = (rx->indexIn(name) != -1 ? rx->cap(1) : "ptr");
			if (staticType == "ptr") {
				arg->argType = xmoc_ptr;
				QByteArray targetType = name;
				typeId = smoke->idType(targetType.constData());
				if (typeId == 0 && !name.contains('*')) {
					if (!name.contains("&")) {
						targetType += "&";
					}
					typeId = smoke->idType(targetType.constData());
				}

                // This shouldn't be necessary because the type of the slot arg
                // should always be in the smoke module of the slot being
                // invoked. However, that isn't true for a dataUpdated() slot
                // in a PlasmaScripting::Applet
                /*
				if (typeId == 0) {
					QHash<Smoke*, PerlQtModule>::const_iterator it;
					for (it = perlqt_modules.constBegin(); it != perlqt_modules.constEnd(); ++it) {
						smoke = it.key();
						targetType = name;
						typeId = smoke->idType(targetType.constData());
						if (typeId != 0) {
							break;
						}
	
						if (typeId == 0 && !name.contains('*')) {
							if (!name.contains("&")) {
								targetType += "&";
							}

							typeId = smoke->idType(targetType.constData());
	
							if (typeId != 0) {
								break;
							}
						}
					}
				}	
                */		
			} else if (staticType == "bool") {
				arg->argType = xmoc_bool;
				smoke = qt_Smoke;
				typeId = smoke->idType(name.constData());
			} else if (staticType == "int") {
				arg->argType = xmoc_int;
				smoke = qt_Smoke;
				typeId = smoke->idType(name.constData());
			} else if (staticType == "uint") {
				arg->argType = xmoc_uint;
				smoke = qt_Smoke;
				typeId = smoke->idType(name.constData());
			} else if (staticType == "long") {
				arg->argType = xmoc_long;
				smoke = qt_Smoke;
				typeId = smoke->idType(name.constData());
			} else if (staticType == "ulong") {
				arg->argType = xmoc_ulong;
				smoke = qt_Smoke;
				typeId = smoke->idType(name.constData());
			} else if (staticType == "double") {
				arg->argType = xmoc_double;
				smoke = qt_Smoke;
				typeId = smoke->idType(name.constData());
			} else if (staticType == "char*") {
				arg->argType = xmoc_charstar;
				smoke = qt_Smoke;
				typeId = smoke->idType(name.constData());
			} else if (staticType == "QString") {
				arg->argType = xmoc_QString;
				name += "*";
				smoke = qt_Smoke;
				typeId = smoke->idType(name.constData());
			}

			if (typeId == 0) {
				croak("Cannot handle '%s' as slot argument\n", name.constData());
				return result;
			}

			arg->st.set(smoke, typeId);
			result.append(arg);
		}
	}

	return result;
}

// The pointer map gives us the relationship between an arbitrary c++ pointer
// and a perl SV.  If you have a virtual function call, you only start with a
// c++ pointer.  This reference allows you to trace back to a perl package, and
// find a subroutine in that package to call.
SV* getPointerObject(void* ptr) {
    HV *hv = pointer_map;
    SV *keysv = newSViv((IV)ptr);
    STRLEN len;
    char *key = SvPV(keysv, len);
    // Look to see in the pointer_map for a ptr->perlSV reference
    SV **svp = hv_fetch(hv, key, len, 0);
    // Nothing found, exit out
    if(!svp){
        SvREFCNT_dec(keysv);
        return 0;
    }
    // Corrupt entry, not sure how this would happen
    if(!SvOK(*svp)){
        hv_delete(hv, key, len, G_DISCARD);
        SvREFCNT_dec(keysv);
        return 0;
    }
    SvREFCNT_dec(keysv);
    return *svp;
}

int isDerivedFrom(Smoke *smoke, Smoke::Index classId, Smoke::Index baseId, int cnt) {
    if(classId == baseId)
        return cnt;
    cnt++;
    for(Smoke::Index *p = smoke->inheritanceList + smoke->classes[classId].parents;
        *p;
        p++)
    {
        if(isDerivedFrom(smoke, *p, baseId, cnt) != -1)
            return cnt;
    }
    return -1;
}

int isDerivedFrom(Smoke *smoke, const char *className, const char *baseClassName, int cnt) {
    if(!smoke || !className || !baseClassName)
        return -1;
    Smoke::Index idClass = smoke->idClass(className).index;
    Smoke::Index idBase = smoke->idClass(baseClassName).index;
    return isDerivedFrom(smoke, idClass, idBase, cnt);
}

// Enter keys: integer memory address of a cxxptr, values: associated perl sv
// into pointer_map hash
// Recurse to store it also as casted to its parent classes, which could (and
// does) have different memory addresses
void mapPointer(SV *obj, smokeperl_object *o, HV *hv, Smoke::Index classId, void *lastptr) {
    void *ptr = o->smoke->cast(o->ptr, o->classId, classId);
    // This ends the recursion
    if(ptr != lastptr) {
        lastptr = ptr;
        SV *keysv = newSViv((IV)ptr);
        STRLEN len;
        char *key = SvPV(keysv, len);
        SV *rv = newSVsv(obj);
        sv_rvweaken(rv); // weak reference! See weaken docs in Scalar::Util
        hv_store(hv, key, len, rv, 0);
        SvREFCNT_dec(keysv);
    }
    for(Smoke::Index *i = o->smoke->inheritanceList + o->smoke->classes[classId].parents; *i; i++) {
        mapPointer(obj, o, hv, *i, lastptr);
    }
}

// Given the perl package, look up the smoke classid
// Depends on the classcache_ext hash being defined, which gets set in the
// init_class function in Qt::_internal
Smoke::Index package_classId( const char *package ) {
    // Get the cache hash
    HV* classcache_ext = get_hv( "Qt::_internal::package2classId", false );
    U32 klen = strlen( package );
    SV** classcache = hv_fetch( classcache_ext, package, klen, 0 );
    Smoke::Index item = 0;
    if( classcache ) {
        item = SvIV( *classcache );
    }
    if( item ){
        return item;
    }

    // Get the ISA array, nisa is a temp string to build package::ISA
    char nisa[strlen(package)+6];
    sprintf( nisa, "%s::ISA", package );
    AV* isa = get_av( nisa, true );

    // Loop over the ISA array
    for( int i = 0; i <= av_len( isa ); i++ ) {
        // Get the value of the current index into @isa
        SV** np = av_fetch( isa, i, 0 ); // np = 'new package'?
        if( np ) {
            // Recurse until we find a match
            Smoke::Index ix = package_classId( SvPV_nolen( *np ) );
            if( ix ) {
                ;// Cache the result - to do, does it depend on cache?
                return ix;
            }
        }
    }
    // Found nothing, so
    return (Smoke::Index) 0;
}

#ifdef DEBUG
// Args: Smoke::Index id: a smoke method id to print
// Returns: an SV* containing a formatted method signature string
SV* prettyPrintMethod(Smoke::Index id) {
    SV* r = newSVpv("", 0);
    Smoke::Method& meth = qt_Smoke->methods[id];
    const char* tname = qt_Smoke->types[meth.ret].name;
    if(meth.flags & Smoke::mf_static) sv_catpv(r, "static ");
    sv_catpvf(r, "%s ", (tname ? tname:"void"));
    sv_catpvf(r, "%s::%s(", qt_Smoke->classes[meth.classId].className, qt_Smoke->methodNames[meth.name]);
    for(int i = 0; i < meth.numArgs; i++) {
        if(i) sv_catpv(r, ", ");
        tname = qt_Smoke->types[qt_Smoke->argumentList[meth.args+i]].name;
        sv_catpv(r, (tname ? tname:"void"));
    }
    sv_catpv(r, ")");
    if(meth.flags & Smoke::mf_const) sv_catpv(r, " const");
    return r;
}
#endif

const char* resolve_classname( smokeperl_object* o ) {
    return binding.className( o->classId );
}

SV* set_obj_info(const char * className, smokeperl_object * o) {
    // The hash
    HV* hv = newHV();
    // The hash reference to return
    SV* var = newRV_noinc((SV*)hv);

    // Bless the sv to that package.
    sv_bless( var, gv_stashpv(className, TRUE) );

    // For this, we need a magic wand.  This is what actually
    // stores 'o' into our hash.
    sv_magic((SV*)hv, 0, '~', (char*)o, sizeof(*o));

    // Associate our vtbl_smoke with our sv, so that
    // smokeperl_free is called for us when the sv's refcount goes to 0
    MAGIC* mg = mg_find((SV*)hv, '~');
    mg->mg_virtual = &vtbl_smoke;

    // We're done with our local var
    return var;
}

// Returns the memory address of the cxxptr stored within a given sv.
void* sv_to_ptr(SV* sv) {
    smokeperl_object* o = sv_obj_info(sv);
    return o ? o->ptr : 0;
}

// Remove the values entered in pointer_map hash, called from
// PerlQt::Binding::deleted when the destructor of a smoke object is called
void unmapPointer( smokeperl_object* o, Smoke::Index classId, void* lastptr) {
    HV* hv = pointer_map;
    void* ptr = o->smoke->cast( o->ptr, o->classId, classId );
    if( ptr != lastptr) { //recurse
        lastptr = ptr;
        SV* keysv = newSViv((IV)ptr);
        STRLEN len;
        char* key = SvPV(keysv, len);
        if(hv_exists(hv, key, len))
            hv_delete(hv, key, len, G_DISCARD);
        SvREFCNT_dec(keysv);
    }
    // Do the same for all parent classes
    for(Smoke::Index *i = o->smoke->inheritanceList + o->smoke->classes[classId].parents; *i; i++) {
        unmapPointer(o, *i, lastptr);
    }
}

XS(XS_qabstract_item_model_rowcount) {
    dXSARGS;
    if (items == 1) {
        smokeperl_object *o = sv_obj_info(ST(0));
        QAbstractItemModel * model = (QAbstractItemModel *) o->ptr;
		XSRETURN_IV(model->rowCount());
	}
	else if (items == 2) {
        smokeperl_object *o = sv_obj_info(ST(0));
        QAbstractItemModel * model = (QAbstractItemModel *) o->ptr;
		smokeperl_object * mi = sv_obj_info(ST(1));
		QModelIndex * modelIndex = (QModelIndex *) mi->ptr;
		XSRETURN_IV(model->rowCount(*modelIndex));
	}
    else {
        croak("%s", "Invalid argument list to Qt::AbstractItemModel::rowCount");
    }
}

XS(XS_qabstract_item_model_columncount) {
    dXSARGS;
    if (items == 1) {
        smokeperl_object *o = sv_obj_info(ST(0));
        QAbstractItemModel * model = (QAbstractItemModel *) o->ptr;
		XSRETURN_IV(model->columnCount());
	}
	else if (items == 2) {
        smokeperl_object *o = sv_obj_info(ST(0));
        QAbstractItemModel * model = (QAbstractItemModel *) o->ptr;
		smokeperl_object * mi = sv_obj_info(ST(1));
		QModelIndex * modelIndex = (QModelIndex *) mi->ptr;
		XSRETURN_IV(model->columnCount(*modelIndex));
	}
    else {
        croak("%s", "Invalid argument list to Qt::AbstractItemModel::columnCount");
    }
}

XS(XS_qabstract_item_model_data) {
    dXSARGS;
    smokeperl_object * o = sv_obj_info(ST(0));
	QAbstractItemModel * model = (QAbstractItemModel *) o->ptr;
    smokeperl_object * mi = sv_obj_info(ST(1));
	QModelIndex * modelIndex = (QModelIndex *) mi->ptr;
	QVariant value;
	if (items == 2) {
		value = model->data(*modelIndex);
	} else if (items == 3) {
		value = model->data(*modelIndex, SvIV(SvRV(ST(2))));
	} else {
		croak("%s", "Invalid argument list to Qt::AbstractItemModel::data");
	}

    SV* retval = allocSmokePerlSV(
        new QVariant(value),
        SmokeType( o->smoke, o->smoke->idType("QVariant") )
    );

    ST(0) = sv_2mortal( retval );
    XSRETURN(1);
}

XS(XS_qabstract_item_model_setdata) {
    dXSARGS;
    if ( items < 1 || items > 4 ) {
        croak("%s\n", "Invalid argument list to Qt::AbstractItemModel::setData");
    }
    smokeperl_object *o = sv_obj_info(ST(0));
	QAbstractItemModel * model = (QAbstractItemModel *) o->ptr;
    smokeperl_object * mi = sv_obj_info(ST(1));
	QModelIndex * modelIndex = (QModelIndex *) mi->ptr;
    smokeperl_object * v = sv_obj_info(ST(2));
	QVariant * variant = (QVariant *) v->ptr;

	if ( items == 3 ) {
		if ( model->setData(*modelIndex, *variant) ) {
            XSRETURN_YES;
        }
        else {
            XSRETURN_NO;
        }
	}
    else if ( items == 4 ) {
        printf( "Calling setData with displayrole %d\n", SvIV(SvRV(ST(3))) );
        if ( model->setData( *modelIndex, *variant, SvIV(SvRV(ST(3))) ) ) {
            XSRETURN_YES;
        }
        else {
            XSRETURN_NO;
        }
    }
}

XS(XS_qabstract_item_model_flags) {
    dXSARGS;
    smokeperl_object *o = sv_obj_info(ST(0));
	QAbstractItemModel * model = (QAbstractItemModel *) o->ptr;
    smokeperl_object * mi = sv_obj_info(ST(1));
	const QModelIndex * modelIndex = (const QModelIndex *) mi->ptr;
	XSRETURN_IV((IV)model->flags(*modelIndex));
}

XS(XS_qabstract_item_model_insertrows) {
    dXSARGS;
    smokeperl_object *o = sv_obj_info(ST(0));
	QAbstractItemModel * model = (QAbstractItemModel *) o->ptr;

	if (items == 3) {
        //bool insertRows( int row, int count )
		if (model->insertRows(SvIV(ST(1)), SvIV(ST(2)))) {
            XSRETURN_YES;
        }
        else {
            XSRETURN_NO;
        }
	}
	else if (items == 4) {
        //bool insertRows( int row, int count, const QModelIndex & parent = QModelIndex() )
    	smokeperl_object * mi = sv_obj_info(ST(3));
		const QModelIndex * modelIndex = (const QModelIndex *) mi->ptr;
		if (model->insertRows(SvIV(ST(1)), SvIV(ST(2)), *modelIndex)) {
            XSRETURN_YES;
        }
        else {
            XSRETURN_NO;
        }
	}

	croak("%s", "Invalid argument list to Qt::AbstractItemModel::insertRows");
}

XS(XS_qabstract_item_model_insertcolumns) {
    dXSARGS;
    smokeperl_object *o = sv_obj_info(ST(0));
	QAbstractItemModel * model = (QAbstractItemModel *) o->ptr;

	if (items == 3) {
        //bool insertColumns( int column, int count )
		if (model->insertColumns(SvIV(ST(1)), SvIV(ST(2)))) {
            XSRETURN_YES;
        }
        else {
            XSRETURN_NO;
        }
	}
	else if (items == 4) {
        //bool insertColumns( int column, int count, const QModelIndex & parent = QModelIndex() )
    	smokeperl_object * mi = sv_obj_info(ST(3));
		const QModelIndex * modelIndex = (const QModelIndex *) mi->ptr;
		if (model->insertColumns(SvIV(ST(1)), SvIV(ST(2)), *modelIndex)) {
            XSRETURN_YES;
        }
        else {
            XSRETURN_NO;
        }
	}

	croak("%s", "Invalid argument list to Qt::AbstractItemModel::insertColumns");
}

XS(XS_qabstract_item_model_removerows) {
    dXSARGS;
    smokeperl_object *o = sv_obj_info(ST(0));
	QAbstractItemModel * model = (QAbstractItemModel *) o->ptr;

	if (items == 3) {
        //bool removeRows( int row, int count )
		if (model->removeRows(SvIV(ST(1)), SvIV(ST(2)))) {
            XSRETURN_YES;
        }
        else {
            XSRETURN_NO;
        }
	}
	else if (items == 4) {
        //bool removeRows( int row, int count, const QModelIndex & parent = QModelIndex() )
    	smokeperl_object * mi = sv_obj_info(ST(3));
		const QModelIndex * modelIndex = (const QModelIndex *) mi->ptr;
		if (model->removeRows(SvIV(ST(1)), SvIV(ST(2)), *modelIndex)) {
            XSRETURN_YES;
        }
        else {
            XSRETURN_NO;
        }
	}

	croak("%s", "Invalid argument list to Qt::AbstractItemModel::removeRows");
}

XS(XS_qabstract_item_model_removecolumns) {
    dXSARGS;
    smokeperl_object *o = sv_obj_info(ST(0));
	QAbstractItemModel * model = (QAbstractItemModel *) o->ptr;

	if (items == 3) {
        //bool removeColumns( int column, int count )
		if (model->removeColumns(SvIV(ST(1)), SvIV(ST(2)))) {
            XSRETURN_YES;
        }
        else {
            XSRETURN_NO;
        }
	}
	else if (items == 4) {
        //bool removeColumns( int column, int count, const QModelIndex & parent = QModelIndex() )
    	smokeperl_object * mi = sv_obj_info(ST(3));
		const QModelIndex * modelIndex = (const QModelIndex *) mi->ptr;
		if (model->removeColumns(SvIV(ST(1)), SvIV(ST(2)), *modelIndex)) {
            XSRETURN_YES;
        }
        else {
            XSRETURN_NO;
        }
	}

	croak("%s", "Invalid argument list to Qt::AbstractItemModel::removeColumns");
}

XS(XS_qvariant_value) {
    dXSARGS;
	void * sv_ptr = 0;
	SV *retval = &PL_sv_undef;

    smokeperl_object *o = sv_obj_info(ST(1));
	if (o == 0 || o->ptr == 0) {
		ST(0) = retval;
        XSRETURN(1);
	}

	QVariant * variant = (QVariant*) o->ptr;

	// If the QVariant contains a user type, don't bother to look at the Perl class argument
	if (variant->type() >= QVariant::UserType) {
#ifdef QT_QTDBUS 
    if (qstrcmp(variant->typeName(), "QDBusObjectPath") == 0) {
        QString s = qVariantValue<QDBusObjectPath>(*variant).path();
        ST(0) = sv_2mortal( newSVpv( s.toLatin1(), s.size() ) );
        XSRETURN(1);
    } else if (qstrcmp(variant->typeName(), "QDBusSignature") == 0) {
        QString s = qVariantValue<QDBusSignature>(*variant).signature();
        ST(0) = sv_2mortal( newSVpv( s.toLatin1(), s.size() ) );
        XSRETURN(1);
    }
#endif

    void *value_ptr = QMetaType::construct(QMetaType::type(variant->typeName()), (void *) variant->constData());
    Smoke::ModuleIndex mi = o->smoke->findClass("QVariant");
    Smoke::Index typeId = mi.smoke->idType(mi.smoke->classes[mi.index].className);
    retval = allocSmokePerlSV(
        value_ptr,
        SmokeType( mi.smoke, typeId )
    );
    sv_bless(retval, gv_stashpv( binding.className(mi.index), TRUE ) );
    ST(0) = sv_2mortal(retval);
    XSRETURN(1);
	}

	const char * classname = SvPV_nolen(ST(0));
    Smoke::ModuleIndex * sv_class_id = new Smoke::ModuleIndex;
    sv_class_id->index = package_classId(classname);
    sv_class_id->smoke = qt_Smoke;

	if (sv_class_id == 0) {
		ST(0) = retval;
        XSRETURN(1);
	}

	if (qstrcmp(classname, "Qt::Pixmap") == 0) {
		QPixmap v = qVariantValue<QPixmap>(*variant);
		sv_ptr = (void *) new QPixmap(v);
	} else if (qstrcmp(classname, "Qt::Font") == 0) {
		QFont v = qVariantValue<QFont>(*variant);
		sv_ptr = (void *) new QFont(v);
	} else if (qstrcmp(classname, "Qt::Brush") == 0) {
		QBrush v = qVariantValue<QBrush>(*variant);
		sv_ptr = (void *) new QBrush(v);
	} else if (qstrcmp(classname, "Qt::Color") == 0) {
		QColor v = qVariantValue<QColor>(*variant);
		sv_ptr = (void *) new QColor(v);
	} else if (qstrcmp(classname, "Qt::Palette") == 0) {
		QPalette v = qVariantValue<QPalette>(*variant);
		sv_ptr = (void *) new QPalette(v);
	} else if (qstrcmp(classname, "Qt::Icon") == 0) {
		QIcon v = qVariantValue<QIcon>(*variant);
		sv_ptr = (void *) new QIcon(v);
	} else if (qstrcmp(classname, "Qt::Image") == 0) {
		QImage v = qVariantValue<QImage>(*variant);
		sv_ptr = (void *) new QImage(v);
	} else if (qstrcmp(classname, "Qt::Polygon") == 0) {
		QPolygon v = qVariantValue<QPolygon>(*variant);
		sv_ptr = (void *) new QPolygon(v);
	} else if (qstrcmp(classname, "Qt::Region") == 0) {
		QRegion v = qVariantValue<QRegion>(*variant);
		sv_ptr = (void *) new QRegion(v);
	} else if (qstrcmp(classname, "Qt::Bitmap") == 0) {
		QBitmap v = qVariantValue<QBitmap>(*variant);
		sv_ptr = (void *) new QBitmap(v);
	} else if (qstrcmp(classname, "Qt::Cursor") == 0) {
		QCursor v = qVariantValue<QCursor>(*variant);
		sv_ptr = (void *) new QCursor(v);
	} else if (qstrcmp(classname, "Qt::SizePolicy") == 0) {
		QSizePolicy v = qVariantValue<QSizePolicy>(*variant);
		sv_ptr = (void *) new QSizePolicy(v);
	} else if (qstrcmp(classname, "Qt::KeySequence") == 0) {
		QKeySequence v = qVariantValue<QKeySequence>(*variant);
		sv_ptr = (void *) new QKeySequence(v);
	} else if (qstrcmp(classname, "Qt::Pen") == 0) {
		QPen v = qVariantValue<QPen>(*variant);
		sv_ptr = (void *) new QPen(v);
	} else if (qstrcmp(classname, "Qt::TextLength") == 0) {
		QTextLength v = qVariantValue<QTextLength>(*variant);
		sv_ptr = (void *) new QTextLength(v);
	} else if (qstrcmp(classname, "Qt::TextFormat") == 0) {
		QTextFormat v = qVariantValue<QTextFormat>(*variant);
		sv_ptr = (void *) new QTextFormat(v);
	} else if (qstrcmp(classname, "Qt::Variant") == 0) {
		sv_ptr = (void *) new QVariant(*((QVariant *) variant->constData()));
	} else {
		// Assume the value of the Qt::Variant can be obtained
		// with a call such as Qt::Variant.toPoint()
        /*
		QByteArray toValueMethodName(classname);
		if (toValueMethodName.startsWith("Qt::")) {
			toValueMethodName.remove(0, strlen("Qt::"));
		}
		toValueMethodName.prepend("to");
		return rb_funcall(variant_value, rb_intern(toValueMethodName), 1, variant_value);
        */
	}

	retval = allocSmokePerlSV(sv_ptr, SmokeType( sv_class_id->smoke,
         sv_class_id->smoke->idType(sv_class_id->smoke->className(sv_class_id->index))));

    delete sv_class_id;

	ST(0) = sv_2mortal(retval);
    XSRETURN(1);
}

XS(XS_qvariant_from_value) {
    dXSARGS;
    if (items == 2) {
        Smoke::ModuleIndex nameId = qt_Smoke->NullModuleIndex;
        smokeperl_object *o = sv_obj_info(ST(0));
        if (o) {
            nameId = qt_Smoke->idMethodName("QVariant#");
        } else if (SvTYPE(ST(0)) == SVt_PVAV) {
            nameId = qt_Smoke->idMethodName("QVariant?");
        } else {
            nameId = qt_Smoke->idMethodName("QVariant$");
        }

        Smoke::ModuleIndex meth = qt_Smoke->findMethod(qt_Smoke->idClass("QVariant"), nameId);
        Smoke::Index i = meth.smoke->methodMaps[meth.index].method;
        i = -i;		// turn into ambiguousMethodList index
        while (meth.smoke->ambiguousMethodList[i] != 0) {
            if ( qstrcmp( meth.smoke->types[meth.smoke->argumentList[meth.smoke->methods[meth.smoke->ambiguousMethodList[i]].args]].name,
                        HvNAME(ST(1)) ) == 0 )
            {
                Smoke::Index methodId = meth.smoke->ambiguousMethodList[i];
                PerlQt::MethodCall c(qt_Smoke, methodId, o, SP, 0);
                c.next();
                ST(0) = sv_2mortal(c.var());
                XSRETURN(1);
            }

            i++;
        }
    }

    const char * classname = HvNAME(SvSTASH(SvRV(ST(0))));
    smokeperl_object *o = sv_obj_info(ST(0));
    if (o == 0 || o->ptr == 0) {
        // Assume the Qt::Variant can be created with a
        // Qt::Variant.new(obj) call
        fprintf( stderr, "Arguments to qVariantFromValue cannot be null or undef.\n" );
        XSRETURN_UNDEF;
        //if (qstrcmp(classname, "Qt::Enum") == 0) {
            //return rb_funcall(qvariant_class, rb_intern("new"), 1, rb_funcall(ST(0), rb_intern("to_i"), 0));
        //} else {
            //return rb_funcall(qvariant_class, rb_intern("new"), 1, ST(0));
        //}
    }

    QVariant * v = 0;

    if (qstrcmp(classname, " Qt::Pixmap") == 0) {
        v = new QVariant(qVariantFromValue(*(QPixmap*) o->ptr));
    } else if (qstrcmp(classname, " Qt::Font") == 0) {
        v = new QVariant(qVariantFromValue(*(QFont*) o->ptr));
    } else if (qstrcmp(classname, " Qt::Brush") == 0) {
        v = new QVariant(qVariantFromValue(*(QBrush*) o->ptr));
    } else if (qstrcmp(classname, " Qt::Color") == 0) {
        v = new QVariant(qVariantFromValue(*(QColor*) o->ptr));
    } else if (qstrcmp(classname, " Qt::Palette") == 0) {
        v = new QVariant(qVariantFromValue(*(QPalette*) o->ptr));
    } else if (qstrcmp(classname, " Qt::Icon") == 0) {
        v = new QVariant(qVariantFromValue(*(QIcon*) o->ptr));
    } else if (qstrcmp(classname, " Qt::Image") == 0) {
        v = new QVariant(qVariantFromValue(*(QImage*) o->ptr));
    } else if (qstrcmp(classname, " Qt::Polygon") == 0) {
        v = new QVariant(qVariantFromValue(*(QPolygon*) o->ptr));
    } else if (qstrcmp(classname, " Qt::Region") == 0) {
        v = new QVariant(qVariantFromValue(*(QRegion*) o->ptr));
    } else if (qstrcmp(classname, " Qt::Bitmap") == 0) {
        v = new QVariant(qVariantFromValue(*(QBitmap*) o->ptr));
    } else if (qstrcmp(classname, " Qt::Cursor") == 0) {
        v = new QVariant(qVariantFromValue(*(QCursor*) o->ptr));
    } else if (qstrcmp(classname, " Qt::SizePolicy") == 0) {
        v = new QVariant(qVariantFromValue(*(QSizePolicy*) o->ptr));
    } else if (qstrcmp(classname, " Qt::KeySequence") == 0) {
        v = new QVariant(qVariantFromValue(*(QKeySequence*) o->ptr));
    } else if (qstrcmp(classname, " Qt::Pen") == 0) {
        v = new QVariant(qVariantFromValue(*(QPen*) o->ptr));
    } else if (qstrcmp(classname, " Qt::TextLength") == 0) {
        v = new QVariant(qVariantFromValue(*(QTextLength*) o->ptr));
    } else if (qstrcmp(classname, " Qt::TextFormat") == 0) {
        v = new QVariant(qVariantFromValue(*(QTextFormat*) o->ptr));
    } else if (QVariant::nameToType(o->smoke->classes[o->classId].className) >= QVariant::UserType) {
        v = new QVariant(QMetaType::type(o->smoke->classes[o->classId].className), o->ptr);
    } else {
        // Assume the Qt::Variant can be created with a
        // Qt::Variant.new(obj) call
        fprintf( stderr, "Cannot handle type %s in qVariantToValue", classname );
        XSRETURN_UNDEF;
        //return rb_funcall(qvariant_class, rb_intern("new"), 1, ST(0));
    }

    SV *retval = allocSmokePerlSV(v, SmokeType( qt_Smoke, qt_Smoke->idType("QVariant") ) );

    ST(0) = sv_2mortal(retval);
    XSRETURN(1);
}

XS(XS_AUTOLOAD) {
    dXSARGS;
    PERL_SET_CONTEXT(PL_curinterp);
    // Figure out which package and method is being called, based on the
    // autoload variable
    SV* autoload = get_sv( "Qt::AutoLoad::AUTOLOAD", TRUE );
    char* package = SvPV_nolen( autoload );
    char* methodname = 0;
    // Splits off the method name from the package
    for( char* s = package; *s; s++ ) {
        if( *s == ':') methodname = s;
    }
    // No method to call was passed, so error out
    if( !methodname ) XSRETURN_NO;
    // Erases the first character off the method, killing the ':', and truncate
    // the value of method off package.
    *( methodname++ - 1 ) = 0;

    int withObject = ( *package == ' ' ) ? 1 : 0;
    int isSuper = 0;
    if( withObject ) {
        ++package;
        if( *package == ' ' ) {
            isSuper = 1;
            ++package;
            char* super = new char[strlen(package) + 8];
            strcpy( super, package );
            strcat( super, "::SUPER" );
            package = super;
        }
    }

#ifdef DEBUG
    if( do_debug && ( do_debug & qtdb_autoload ) ) {
        fprintf(stderr, "In XS Autoload for %s::%s()", package, methodname);
        if((do_debug & qtdb_verbose) && (withObject || isSuper)) {
            smokeperl_object *o = sv_obj_info(withObject ? ST(0) : sv_this);
            if(o)
                fprintf(stderr, " - this: (%s)%p\n", o->smoke->classes[o->classId].className, o->ptr);
            else
                fprintf(stderr, " - this: (unknown)(nil)\n");
        }
        else {
            fprintf(stderr, "\n");
        }
    }
#endif

    // For anything we do here where withObject is true, sv_this should be set
    // to the first argument on the stack, since that's where perl puts it.
    // Wherever we return, be sure to restore sv_this.
    SV* old_this = 0;
    if( withObject && !isSuper ) {
        old_this = sv_this;
        sv_this = newSVsv(ST(0));
    }
        
    // See if we need to call a perl method
    HV* stash = gv_stashpv( package, TRUE );
    GV* gv = gv_fetchmethod_autoload( stash, methodname, 0 );

    if(gv) {
        // Found a perl method
#ifdef DEBUG
        if(do_debug && (do_debug & qtdb_autoload))
            fprintf(stderr, "\t%s::%s found in Perl stash\n", package, methodname);
#endif            

        // Call the found method
        ENTER;
        SAVETMPS;
        PUSHMARK(SP - items + withObject);
        // What context are we calling this subroutine in?
        I32 gimme = GIMME_V;
        // Make the call, save number of returned values
        int count = call_sv((SV*)GvCV(gv), gimme);
        // Get the return value
        SPAGAIN;
        SP -= count;
        if (withObject)
            for (int i=0; i<count; i++)
                ST(i) = ST(i+1);
        PUTBACK;

        // Clean up
        if(withObject && !isSuper){
            SvREFCNT_dec(sv_this);
            sv_this = old_this;
        }
        else if(isSuper)
            delete[] package;

        // Error out if necessary
        if(SvTRUE(ERRSV))
            croak("%s", SvPV_nolen(ERRSV));

        if (gimme == G_VOID)
            XSRETURN_UNDEF;
        else
            XSRETURN(count);
    }
    else if( !strcmp( methodname, "DESTROY" ) ) {
        smokeperl_object* o = sv_obj_info(sv_this);

        // Check to see that o exists (has a smokeperl_object in sv_this), has
        // a valid pointer, and (is allocated or has an entry in the pointer
        // map).  If all of that's true, or we're in global destruction, we
        // don't really care what happens.
        if( PL_dirty ) {
            // This block will be repeated a lot to clean stuff up.
            if( isSuper ){
                delete[] package;
            }
            if( withObject && !isSuper ) {
                // Restore sv_this
                SvREFCNT_dec(sv_this);
                sv_this = old_this;
            }
            XSRETURN_YES;
        }
        if( !(o && o->ptr && (o->allocated || getPointerObject(o->ptr))) ) {
            // This block will be repeated a lot to clean stuff up.
            if( isSuper )
                delete[] package;
            if( withObject && !isSuper ) {
                // Restore sv_this
                SvREFCNT_dec(sv_this);
                sv_this = old_this;
            }
            XSRETURN_YES;
        }

        // Check to see if a delete of this object has been tried before, by
        // seeing if the object's hash has the "has been hidden" key
        static const char* key = "has been hidden";
        U32 klen = 15;
        SV** svp = 0;
        if( SvROK(sv_this) && SvTYPE(SvRV(sv_this)) == SVt_PVHV ) {
            HV* hv = (HV*)SvRV(sv_this);
            svp = hv_fetch( hv, key, klen, 0);
        }
        if(svp) {
            // Found "has been hidden", so don't do anything, just clean up 
            if( isSuper )
                delete[] package;
            if( withObject && !isSuper ) {
                // Restore sv_this
                SvREFCNT_dec(sv_this);
                sv_this = old_this;
            }
            XSRETURN_YES;
        }

#ifdef DEBUG
        // The following perl call seems to stomp on the package name, let's copy it
        char* packagecpy = new char[strlen(package)+1];
        strcpy( packagecpy, package );
#endif

        // Call the ON_DESTROY method, that stores a reference (increasing the
        // refcnt) if necessary
        if( !stash )
            stash = gv_stashpv(package, TRUE);
        gv = gv_fetchmethod_autoload(stash, "ON_DESTROY", 0);
        int retval = 0;
        if( gv ) {
            PUSHMARK(SP);
            int count = call_sv((SV*)GvCV(gv), G_SCALAR|G_NOARGS);
            SPAGAIN;
            if (count != 1) {
                if( withObject && !isSuper ) {
                    // Restore sv_this
                    SvREFCNT_dec(sv_this);
                    sv_this = old_this;
                }
                croak( "Corrupt ON_DESTROY return value: Got %d value(s), expected 1\n", count );
            }
            retval = POPi;
            PUTBACK;
        }

#ifdef DEBUG
        if( do_debug && retval && (do_debug & qtdb_gc) )
            fprintf(stderr, "Increasing refcount in DESTROY for %s=%p (still has a parent)\n", packagecpy, o->ptr);
        delete[] packagecpy;
#endif

        // Now clean up
        if( withObject && !isSuper ) {
            SvREFCNT_dec(sv_this);
            sv_this = old_this;
        }
        if( isSuper )
            delete[] package;
    }
    else {
        // We're calling a c++ method

        // Get the classId (eventually converting SUPER to the right Qt class)
        Smoke::Index classId = package_classId( package );
        char* classname = (char*) qt_Smoke->className( classId );
        Smoke::Index methodId = 0;
        // We may call a perl sub to find the methodId.  This will overwrite
        // the current SP pointer, so save a copy
        SV** savestack = new SV*[items+1];

        // The deal with SP - items + 1: SP is a stack.  Arguments get pushed
        // onto the stack.  Therefore, the position of the stack pointer when
        // our sub gets it is set to the last argument on the stack.  To get
        // the position of the first argument, you subtract the # of arguments,
        // aka items.  +1 because it's an array.
        Copy( SP - items + 1 + withObject, savestack, items + withObject, SV* );

        // Look in the cache; if this method was called before with the same
        // arguments, we already know the methodId
        // The key to the methodcache looks like this:
        // class      method     arg types
        // QPopupMenu;insertItem;s;QApplication;s
        int lclassname = strlen(classname);
        int lmethodname = strlen(methodname);
        char mcid[256];
        strncpy(mcid, classname, lclassname);
        char *ptr = mcid + lclassname;
        *(ptr++) = ';'; //Set the current position to ; then increment
        strncpy(ptr, methodname, lmethodname);
        ptr += lmethodname;

        // that gives us the first 2 parts of the methcache key, now for the
        // args
        for(int i = withObject; i < items; i++) {
            *(ptr++) = ';';
            const char *type = get_SVt(ST(i));
            int typelen = strlen(type);
            strncpy( ptr, type, typelen );
            ptr += typelen;
        }
        *ptr = 0; // Don't forget to null-terminate the string

        // See if it's cached
        Smoke::Index* rcid = methcache.value(mcid);
        if(rcid) {
            // Got a hit
            methodId = *rcid;
        }
        else {
            // Call do_autoload to get the methodId
            ENTER;
            SAVETMPS;
            PUSHMARK( SP - items + withObject );
            EXTEND( SP, 3 );
            PUSHs(sv_2mortal(newSViv((IV)classId)));
            PUSHs(sv_2mortal(newSVpv(methodname, 0)));
            PUSHs(sv_2mortal(newSVpv(classname, 0)));
            PUTBACK;
            int count = call_pv( "Qt::_internal::do_autoload", G_ARRAY|G_EVAL );
            SPAGAIN;
            // See if do_autoload die'd
            if (SvTRUE(ERRSV)) {
                if( withObject && !isSuper) {
                    SvREFCNT_dec(sv_this);
                    sv_this = old_this;
                }
                else if(isSuper)
                    delete[] package;
                delete[] savestack;
                croak("%s", SvPV_nolen(ERRSV));
            }

            if (count != 2) {
                // Error, clean up our crap
                if( withObject && !isSuper) {
                    SvREFCNT_dec(sv_this);
                    sv_this = old_this;
                }
                else if(isSuper)
                    delete[] package;

                // Error out
                fprintf( stderr, "How'd I get here?\n" );
                croak( "Corrupt do_autoload return value: Got %d value(s), expected 2\n", count );
            }

            int cacheLookup = POPi;
            methodId = POPi;
            PUTBACK;
            FREETMPS;
            LEAVE;

            // Save our lookup
            if ( cacheLookup ) 
                methcache.insert(mcid, new Smoke::Index(methodId));
        }

        static smokeperl_object nothis = { 0, 0, 0, false };
        smokeperl_object* call_this = 0;
        if ( withObject ) {
            call_this = sv_obj_info( sv_this );
        }
        else {
            call_this = &nothis;
        }

#ifdef DEBUG
        if(do_debug && (do_debug & qtdb_calls)) {
            fprintf(stderr, "Calling method\t%s\t%s\n", methodname, SvPV_nolen(sv_2mortal(prettyPrintMethod(methodId))));
            if(do_debug & qtdb_verbose) {
                fprintf(stderr, "with arguments (%s)\n", SvPV_nolen(sv_2mortal(catArguments(savestack, items - withObject))));
            }
        }
#endif

        PerlQt::MethodCall call( qt_Smoke,
                         methodId,
                         call_this,
                         savestack,
                         items  - withObject );
        call.next();

        // The savestack will only be a copy created with new[] if we called a
        // perl method.  If we did, the savestack pointer will differ from the
        // SP pointer, because the perl method changed it.
        if( savestack )
            delete[] savestack;

        if( withObject && !isSuper) {
            SvREFCNT_dec(sv_this);
            sv_this = old_this;
        }
        else if( isSuper )
            delete[] package;

        SV* retval = call.var();

        // Put the return value onto perl's stack
        ST(0) = sv_2mortal(retval);
        XSRETURN(1);
    }
}

XS(XS_qt_metacall){
    dXSARGS;
    PERL_UNUSED_VAR(items);
    PERL_SET_CONTEXT(PL_curinterp);

    // Get my arguments off the stack
    QObject* sv_this_ptr = (QObject*)sv_obj_info(sv_this)->ptr;
    // This is an enum value, so it's stored as a scalar reference.
    QMetaObject::Call _c = (QMetaObject::Call)SvIV(SvRV(ST(0)));
    int _id = (int)SvIV(ST(1));
    void** _a = (void**)sv_obj_info(ST(2))->ptr;

    // Assume the target slot is a C++ one
    smokeperl_object* o = sv_obj_info(sv_this);
    Smoke::ModuleIndex nameId = o->smoke->idMethodName("qt_metacall$$?");
    Smoke::ModuleIndex classIdx = { o->smoke, o->classId };
    Smoke::ModuleIndex meth = nameId.smoke->findMethod(classIdx, nameId);
    if (meth.index > 0) {
        Smoke::Method &m = meth.smoke->methods[meth.smoke->methodMaps[meth.index].method];
        Smoke::ClassFn fn = meth.smoke->classes[m.classId].classFn;
        Smoke::StackItem i[4];
        i[1].s_enum = _c;
        i[2].s_int = _id;
        i[3].s_voidp = _a;
        (*fn)(m.method, o->ptr, i);
        int ret = i[0].s_int;
        if (ret < 0) {
            ST(0) = sv_2mortal(newSViv(ret));
            XSRETURN(1);
        }
    } else {
        // Should never happen..
        croak( "Cannot find %s::qt_metacall() method\n", 
               o->smoke->classes[o->classId].className );
    }

    // Get the current metaobject with a virtual call
    const QMetaObject* metaobject = sv_this_ptr->metaObject();

    // get method/property count
    int count = 0;
    if (_c == QMetaObject::InvokeMetaMethod) {
        count = metaobject->methodCount();
    } else {
        count = metaobject->propertyCount();
    }

    if (_c == QMetaObject::InvokeMetaMethod) {
        QMetaMethod method = metaobject->method(_id);

        // Signals are easy, just activate the meta object
        // This code gets called when a cxx signal is connected to a signal
        // defined in a perl package
        if (method.methodType() == QMetaMethod::Signal) {
#ifdef DEBUG
            if(do_debug && (do_debug & qtdb_signals))
                fprintf( stderr, "In signal for %s::%s\n", metaobject->className(), method.signature() );
#endif
            metaobject->activate(sv_this_ptr, metaobject, 0, _a);
            // +1.  Id is 0 based, count is 1 based
            ST(0) = sv_2mortal(newSViv(_id - count + 1));
            XSRETURN(1);
        }
        else if (method.methodType() == QMetaMethod::Slot) {

            // Get the smoke to type id relationship args
            QList<MocArgument*> mocArgs = getMocArguments(o->smoke, method.typeName(), method.parameterTypes());

            // Find the name of the method being called
            QString name(method.signature());
            static QRegExp* rx = 0;
            if (rx == 0) {
                rx = new QRegExp("\\(.*");
            }
            name.replace(*rx, "");

            PerlQt::InvokeSlot slot( sv_this, name.toLatin1().data(), mocArgs, _a );
            slot.next();
        }
    }

    // This should return -1 when we're the one that handled the call
    ST(0) = sv_2mortal(newSViv(_id - count));
    XSRETURN(1);
}

XS(XS_signal){
    dXSARGS;

    smokeperl_object *o = sv_obj_info(sv_this);
    QObject *qobj = (QObject*)o->smoke->cast( o->ptr, o->classId, o->smoke->idClass("QObject").index );
    if(qobj->signalsBlocked()) XSRETURN_UNDEF;

    // Each xs method has an implied cv argument that holds the info for the
    // called subroutine.  Use it to determine the name of the signal being
    // called.
    GV* gv = CvGV(cv);
    QLatin1String signalname( GvNAME(gv) );
#ifdef DEBUG
    if(do_debug && (do_debug & qtdb_signals)){
        char* package = HvNAME( GvSTASH(gv) );
        fprintf( stderr, "In signal call %s::%s\n", package, GvNAME(gv) );
        if(do_debug & qtdb_verbose) {
            fprintf(stderr, "with arguments (%s) ", SvPV_nolen(sv_2mortal(catArguments(SP - items + 1, items ))));
            // See cop.h in the perl src for more info on Control ops
            fprintf(stderr, "called at line %lu in %s\n", CopLINE(PL_curcop), GvNAME(CopFILEGV(PL_curcop))+2 );
        }
    }
#endif

    // Get the current metaobject with a virtual call
    const QMetaObject* metaobject = qobj->metaObject();

    // Find the method's meta id.  This loop is easier than building the method
    // signature to send to indexOfMethod, but makes it impossible to make 2
    // signals with the same name but different signatures (arguments).
    int index = -1;
    for (index = metaobject->methodCount() - 1; index > -1; --index) {
		if (metaobject->method(index).methodType() == QMetaMethod::Signal) {
			QString name(metaobject->method(index).signature());
            static QRegExp * rx = 0;
			if (rx == 0) {
				rx = new QRegExp("\\(.*");
			}
			name.replace(*rx, "");

			if (name == signalname) {
				break;
			}
		}
    }

	if (index == -1) {
		XSRETURN_UNDEF;
	}
    QMetaMethod method = metaobject->method(index);
    QList<MocArgument*> args = getMocArguments(o->smoke, method.typeName(), method.parameterTypes());

    SV* retval = sv_2mortal(newSV(0));

    // Our args here:
    // qobj: Whoever is emitting the signal, cast to a QObject*
    // index: The index of the current signal in QMetaObject's array of sig/slots
    // items: The number of arguments we are calling with
    // args: A QList, whose length is items + 1, that tell us how to convert the args to ones Qt likes
    // SP: ...not sure if this is correct.  If items=0, we'll pass sp+1, which
    // should be out of bounds.  But it doesn't matter, since the signal won't
    // do anything with those.
    // retval: Will (at some point, maybe) get populated with the return value from the signal.
    PerlQt::EmitSignal signal(qobj, index, items, args, SP - items + 1, retval);
    signal.next();

    // TODO: Handle signal return value
}

XS(XS_super) {
    dXSARGS;
    PERL_UNUSED_VAR(items);
    SV **svp = 0;
    // Only return the _INTERNAL_STATIC_ hashref if we're in a package that has
    // called SUPER::NEW()
    if(SvROK(sv_this) && SvTYPE(SvRV(sv_this)) == SVt_PVHV){
		// Figure out who's calling us
        HV *stash = (HV*)SvSTASH(SvRV(sv_this));
        if(*HvNAME(stash) == ' ') // if withObject, look for a diff stash
            stash = gv_stashpv(HvNAME(stash) + 1, TRUE);
        if(!stash) XSRETURN_UNDEF;
        
        // Get the _INTERNAL_STATIC_ hash
        svp = hv_fetch(stash, "_INTERNAL_STATIC_", 17, 0);
        if(!svp) XSRETURN_UNDEF;
        
        // Get the hash value from that hash key, remember it's a hash of
        // hashes
        stash = GvHV((GV*)*svp);
        if(!stash) XSRETURN_UNDEF;

        // Now get the value of the 'SUPER' key in the retrieved
        // _INTERNAL_STATIC_ hash
        svp = hv_fetch(stash, "SUPER", 5, 0);
        if(svp) {
            ST(0) = *svp;
            XSRETURN(1);
        }
    }
}

XS(XS_this) {
    dXSARGS;
    PERL_UNUSED_VAR(items);
    ST(0) = sv_this;
    XSRETURN(1);
}
