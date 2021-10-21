#ifndef ERFA_WRAP
#define ERFA_WRAP

#ifdef ERFAHDEF
#error Please include erfa_wrap.h instead of erfa.h
#endif

#include "erfa.h"

// Define macros adding const cast to some functions to avoid compilation
// warnings
#define eraPpp(a, b, apb) eraPpp((void*)(a), (void*)(b), apb)
#define eraPmp(a, b, amb) eraPmp((void*)(a), (void*)(b), amb)
#define eraRxp(r, p, rp) eraRxp((void*)(r), (void*)(p), rp)
#define eraCpv(pv, c) eraCpv((void*)(pv), c)
#define eraPvmpv(a, b, amb) eraPvmpv((void*)(a), (void*)(b), amb)
#define eraPvppv(a, b, apb) eraPvppv((void*)(a), (void*)(b), apb)
#define eraLdsun(p, e, em, p1) eraLdsun((void*)(p), (void*)(e), em, p1)
#define eraAb(pnat, v, s, bm1, ppr) eraAb((void*)(pnat), (void*)(v), (s), \
    (bm1), ppr)
#define eraPvu(dt, pv, upv) eraPvu((dt), (void*)(pv), upv)

#endif
