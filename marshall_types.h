// This file contains the class declarations of the various method call
// classes.

#ifndef MARSHALL_TYPES_H
#define MARSHALL_TYPES_H

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"

#include "smoke.h"

#include "marshall.h"
#include "smokeperl.h"
#include "Qt.h"
#include "handlers.h"

namespace PerlQt {

class Q_DECL_EXPORT MethodReturnValueBase : public Marshall {
public:
    MethodReturnValueBase(Smoke *smoke, Smoke::Index methodIndex, Smoke::Stack stack);
    const Smoke::Method &method();
    Smoke::StackItem &item();
    Smoke *smoke();
    SmokeType type();
    void next();
    bool cleanup();
    void unsupported();
    SV* var();
protected:
    Smoke *_smoke;
    Smoke::Index _methodIndex;
    Smoke::Stack _stack;
    SV *_retval;
};

class Q_DECL_EXPORT VirtualMethodReturnValue : public MethodReturnValueBase {
public:
    VirtualMethodReturnValue(Smoke *smoke, Smoke::Index meth, Smoke::Stack stack, SV* retval);
    Marshall::Action action();
};

class Q_DECL_EXPORT MethodReturnValue : public MethodReturnValueBase {
public:
    MethodReturnValue(Smoke *smoke, Smoke::Index meth, Smoke::Stack stack);
    Marshall::Action action();
};

class Q_DECL_EXPORT MethodCallBase : public Marshall {
public:
    MethodCallBase(Smoke *smoke, Smoke::Index method);
    MethodCallBase(Smoke *smoke, Smoke::Index method, Smoke::Stack stack);
    Smoke *smoke();
    SmokeType type();
    Smoke::StackItem &item();
    const Smoke::Method &method();
    virtual int items() = 0;
    virtual void callMethod() = 0;
    void next();
    void unsupported();

protected:
    Smoke *_smoke;
    Smoke::Index _method;
    Smoke::Stack _stack;
    int _cur;
    Smoke::Index *_args;
    bool _called;
    SV **_sp;
    virtual const char* classname();
};

class Q_DECL_EXPORT VirtualMethodCall : public MethodCallBase {
public:
    VirtualMethodCall(Smoke *smoke, Smoke::Index meth, Smoke::Stack stack, SV *obj, GV *gv);
    ~VirtualMethodCall();
    Marshall::Action action();
    SV *var();
    int items();
    void callMethod();
    bool cleanup();

private:
    GV *_gv;
    SV *_savethis;
};

class Q_DECL_EXPORT MethodCall : public MethodCallBase {
public:
    MethodCall(Smoke *smoke, Smoke::Index methodIndex, smokeperl_object *call_this, SV **sp, int items);
    ~MethodCall();
    Marshall::Action action();
    SV *var();

    inline void callMethod() {
        Smoke::Method *method = _smoke->methods + _method;
        Smoke::ClassFn fn = _smoke->classes[method->classId].classFn;

        void *ptr = _smoke->cast(
            _this->ptr,
            _this->classId,
            _smoke->methods[_method].classId
        );

        // Call the method
        (*fn)(method->method, ptr, _stack);

        // Tell the method call what binding to use
        if (method->flags & Smoke::mf_ctor) {
            Smoke::StackItem s[2];
            s[1].s_voidp = perlqt_modules[_smoke].binding;
            (*fn)(0, _stack[0].s_voidp, s);
        }

        // Marshall the return value
        MethodReturnValue callreturn( _smoke, _method, _stack );

        // Save the result
        _retval = callreturn.var();
    }

    int items(); // What's this?
    bool cleanup();

private:
    smokeperl_object *_this;
    SV **_sp;
    int _items;
    SV *_retval;
    const char *classname();
};
} // End namespace PerlQt

#endif
