#ifndef MARSHALL_BASETYPES_H
#define MARSHALL_BASETYPES_H

template <class T> T* smoke_ptr(Marshall *m) { return (T*) m->item().s_voidp; }

template<> bool* smoke_ptr<bool>(Marshall *m) { return &m->item().s_bool; }
template<> int* smoke_ptr<int>(Marshall *m) { return &m->item().s_int; }
template<> unsigned int* smoke_ptr<unsigned int>(Marshall *m) { return &m->item().s_uint; }
template<> double* smoke_ptr<double>(Marshall *m) { return &m->item().s_double; }

template <class T> T perl_to_primitive(SV*);
template <class T> SV* primitive_to_perl(T);

template <class T>
static void marshall_from_perl(Marshall *m) {
    SV* var;
    if(SvROK(m->var()))
        var = SvRV(m->var());
    else 
        var = m->var();
    (*smoke_ptr<T>(m)) = perl_to_primitive<T>(var);
}

template <class T>
static void marshall_to_perl(Marshall *m) {
    sv_setsv_mg(m->var(), primitive_to_perl<T>( *smoke_ptr<T>(m) ));
}

#include "marshall_primitives.h"

#endif //MARSHALL_BASETYPES_H
