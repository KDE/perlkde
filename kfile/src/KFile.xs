/***************************************************************************
                          KFile.xs  -  KFile perl extension
                             -------------------
    begin                : 11-14-2010
    copyright            : (C) 2010 by Chris Burel
    email                : chrisburel@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QHash>
#include <QList>

// Perl headers
extern "C" {
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"
}

#include <kfile_smoke.h>

#include <smokeperl.h>
#include <handlers.h>

extern QList<Smoke*> smokeList;
extern SV* sv_this;

const char*
resolve_classname_kfile(smokeperl_object * o)
{
    return perlqt_modules[o->smoke].binding->className(o->classId);
}

extern TypeHandler KFile_handlers[];

static PerlQt4::Binding bindingkfile;

MODULE = KFile            PACKAGE = KFile::_internal

PROTOTYPES: DISABLE

SV*
getClassList()
    CODE:
        AV* classList = newAV();
        for (int i = 1; i < kfile_Smoke->numClasses; i++) {
            if (kfile_Smoke->classes[i].className && !kfile_Smoke->classes[i].external)
                av_push(classList, newSVpv(kfile_Smoke->classes[i].className, 0));
        }
        RETVAL = newRV_noinc((SV*)classList);
    OUTPUT:
        RETVAL

SV*
getEnumList()
    CODE:
        AV *av = newAV();
        for(int i = 1; i < kfile_Smoke->numTypes; i++) {
            Smoke::Type curType = kfile_Smoke->types[i];
            if( (curType.flags & Smoke::tf_elem) == Smoke::t_enum )
                av_push(av, newSVpv(curType.name, 0));
        }
        RETVAL = newRV_noinc((SV*)av);
    OUTPUT:
        RETVAL

MODULE = KFile            PACKAGE = KFile

PROTOTYPES: ENABLE

BOOT:
    init_kfile_Smoke();
    smokeList << kfile_Smoke;

    bindingkfile = PerlQt4::Binding(kfile_Smoke);

    PerlQt4Module module = { "PerlKFile", resolve_classname_kfile, 0, &bindingkfile  };
    perlqt_modules[kfile_Smoke] = module;

    install_handlers(KFile_handlers);
