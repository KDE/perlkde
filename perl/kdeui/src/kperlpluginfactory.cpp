/***************************************************************************
 *   Copyright (C) 2005,2006,2007 by Siraj Razick <siraj@kdemail.net>      *
 *   Copyright (C) 2007 by Riccardo Iaconelli <ruphy@fsfe.org>             *
 *   Copyright (C) 2007 by Matthias Kretz <kretz@kde.org>                  *
 *   Copyright (C) 2008 by Richard Dale <richard.j.dale@gmail.com>         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#include <QString>
#include <QDir>
#include <QFileInfo>

#include <KStandardDirs>
#include <klibloader.h>
#include <kdebug.h>
#include <plasma/applet.h>

#include <smoke.h>

// Perl headers
extern "C" {
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
}

// Copy these to avoid linking directly to the bindings
struct smokeperl_object {
    bool allocated;
    void* smoke;
    int classId;
    void* ptr;
};
inline smokeperl_object* sv_obj_info(SV* sv) { // ptr on success, null on fail
    SV *obj = SvRV(sv);
    MAGIC *mg = mg_find(obj, '~');
    if(!mg ){//|| mg->mg_virtual != &vtbl_smoke) {
        // FIXME: die or something?
        fprintf( stderr, "No ~ magic\n" );
        return 0;
    }
    smokeperl_object *o = (smokeperl_object*)mg->mg_ptr;
    return o;
}

// Add the ability for the embedded interpreter to dynamically load modules
// Recreate with perl -MExtUtils::Embed -e xsinit -- -o xsinit.c -std
EXTERN_C void xs_init (pTHX);

EXTERN_C void boot_DynaLoader (pTHX_ CV* cv);

EXTERN_C void
xs_init(pTHX)
{
	char *file = __FILE__;
	dXSUB_SYS;

	/* DynaLoader is a special case */
	newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}

//
// This function was borrowed from the ruby code, which was borrowed from the
// kross code. It puts out an error message and possibly at some point in the
// future a stacktrace on stderr for the current exception.
//
bool
show_exception_message()
{
    PerlInterpreter* my_perl = PL_curinterp;
    if( SvTRUE(ERRSV) ) {
        STRLEN n_a;
        fprintf( stderr, "Error in Perl plugin: $@: %s\n", SvPVx(ERRSV, n_a));
        return true;
    }
    return false;
}

/*
static VALUE plugin_class = Qnil;

static VALUE
create_plugin_instance(VALUE av)
{
    return rb_funcall(plugin_class, rb_intern("new"), 2, Qnil, av);
}
*/

class KPerlPluginFactory : public KPluginFactory
{
    public:
        KPerlPluginFactory();
        //~KPerlPluginFactory();

    protected:
        virtual QObject *create(const char *iface, QWidget *parentWidget, QObject *parent, const QVariantList &args, const QString &keyword);

    private:
        static QByteArray camelize(QByteArray name);
};

static PerlInterpreter *my_perl;

K_EXPORT_PLUGIN(KPerlPluginFactory)
K_EXPORT_PLUGIN_VERSION(PLASMA_VERSION)

KPerlPluginFactory::KPerlPluginFactory()
    : KPluginFactory() // no useful KComponentData object for now
{
}

//KPerlPluginFactory::~KPerlPluginFactory()
//{
    //perl_destruct(my_perl);
    //perl_free(my_perl);
    //PERL_SYS_TERM();
//}

QByteArray KPerlPluginFactory::camelize(QByteArray name)
{
    // Convert foo_bar_baz to FooBarBaz
    QByteArray camelCaseName = name.left(1).toUpper();
    for (int i = 1; i < name.size(); i++) {
        if (name[i] == '_' || name[i] == '-') {
            i++;
            if (i < name.size()) {
                 camelCaseName += name.mid(i, 1).toUpper();
            }
        } else {
             camelCaseName += name[i];
        }
    }

    return camelCaseName;
}

QObject *KPerlPluginFactory::create(const char *iface, QWidget *parentWidget, QObject *parent, const QVariantList &args, const QString &keyword)
{
    QString path = KStandardDirs::locate("data", keyword);

    if (path.isEmpty()) {
        kWarning() << "Perl script" << keyword << "missing";
        return 0;
    }

    QFileInfo program(path);
    // Start up our interpreter
    if( !my_perl ) {
        // kWarning() << "Launcing new perl";
        my_perl = perl_alloc();
        perl_construct(my_perl);

        // Build argc and argv to pass to perl.  Supply a -I to add the applet's
        // path to @INC.
        int argc = 4;
        QByteArray includepath( "-I" );
        includepath.append( QFile::encodeName(program.path()).data() );
        char *argv[] = {
            "kperlpluginfactory",
            includepath.data(),
            "-e", "0" };

        perl_parse(my_perl, xs_init, argc, argv, (char **)NULL);

        // Set a variable that we can check against to see if we're running in
        // embedded perl.  This will be used during virtual method calls, to
        // put G_EVAL on the call to perl to prevent perl from dying and
        // killing the process that is loading the perl plugin.
        sv_setsv( get_sv("Qt4::_internal::isEmbedded", TRUE), &PL_sv_yes );
    }
    PERL_SET_CONTEXT(PL_curinterp);

    // Load the specified in an eval to trap the error
    QString moduleName = program.baseName().replace(QRegExp("\\.pm$"), "").toLatin1();
    QString requireModule = QString( "eval{ require %1 } " )
        .arg( moduleName );
    eval_pv( requireModule.toLatin1().data(), TRUE );
    bool badStatus = show_exception_message();
    if( badStatus ) {
        return 0;
    }
    // Now run ModuleName->import
    QString importModule = QString( "eval{ %1->import } " )
        .arg( moduleName );
    eval_pv( importModule.toLatin1().data(), TRUE );
    badStatus = show_exception_message();
    if( badStatus ) {
        return 0;
    }

    // Get ready to call perl
    dSP; ENTER; SAVETMPS; PUSHMARK(SP);

    AV* argsAV = newAV();
    SV* argsAVref = newRV_noinc( (SV*)argsAV );
    for (int i = 0; i < args.size(); ++i) {
        if (args.at(i).type() == QVariant::String) {
            av_push( argsAV, sv_2mortal(newSVpv(args.at(i).toByteArray().data(), args.at(i).toByteArray().size())));
        } else if (args.at(i).type() == QVariant::Int) {
            av_push( argsAV, sv_2mortal(newSViv(args.at(i).toInt())) );
        } else if (args.at(i).type() == QVariant::Bool) {
            av_push( argsAV, sv_2mortal(newSVsv(args.at(i).toBool() ? &PL_sv_yes : &PL_sv_no )) );
        }
    }

    // The first argument is parent.  We're not going to give the consturctor a
    // parent, because we'd have to create a valid smokeperl_object for our C++
    // parent.  We can't do that without linking to libsmokeqtcore.  The
    // constructor will get an array ref of args though.
    XPUSHs( &PL_sv_undef );
    XPUSHs( argsAVref );
    PUTBACK;

    int count = call_pv(moduleName.toLatin1().data(), G_SCALAR | G_EVAL);
    badStatus = show_exception_message();
    if( badStatus ) {
        return 0;
    }

    SPAGAIN;

    if ( count != 1 ) {
        kWarning() << "Invalid return count from perl";
        return 0;
    }

    SV* perlretval = POPs;

    PUTBACK;
    FREETMPS;
    LEAVE;

    smokeperl_object *o = sv_obj_info(perlretval);
    if( !o || !o->ptr ) {
        kDebug() << "KPerlPluginFactory: Did not get a valid object returned "
            "from" << moduleName << "constructor";
        return 0;
    }
    QObject* createdInstance = reinterpret_cast<QObject *>(o->ptr);
    createdInstance->setParent(parent);

    PUTBACK;
    FREETMPS;
    LEAVE;

    return createdInstance;
}
