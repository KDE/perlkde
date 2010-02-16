#ifndef HANDLERS_H
#define HANDLERS_H

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"

#include "marshall.h"
#include "smokehelp.h"
#include "smokeperl.h"

struct TypeHandler {
    const char* name;
    Marshall::HandlerFn fn;
};

// SV destruction methods
int smokeperl_free(pTHX_ SV* sv, MAGIC* mg);
void invoke_dtor(smokeperl_object* o);

// The magic virtual table that tells sv's to call smokeperl_free when they're
// destroyed
extern struct mgvtbl vtbl_smoke;

template <class T> static void marshall_it(Marshall* m);

void marshall_basetype(Marshall* m);
void marshall_QString(Marshall* m);
void marshall_QStringList(Marshall* m);
void marshall_unknown(Marshall *m);
void marshall_void(Marshall* m);

extern HV* type_handlers;
extern TypeHandler Qt_handlers[];
void install_handlers(TypeHandler* h);

Marshall::HandlerFn getMarshallFn(const SmokeType& type);

#define UNTESTED_HANDLER(name) fprintf( stderr, "The handler %s has no test case.\n", name );

#endif // HANDLERS_H
