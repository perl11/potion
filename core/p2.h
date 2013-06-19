//
// p2.h
//
// (c) 2008 why the lucky stiff, the freelance professor
// (c) 2013 by perl11 org
//
#ifndef P2_H
#define P2_H

#define P2_VERSION  "0.0"
#define P2_MINOR    0
#define P2_MAJOR    0
#define P2_SIG      "p\02\10n"
#define P2_VMID     0x79

#include "potion.h"

#ifdef P2
#  undef NIL_NAME
#  define NIL_NAME "undef" // nil => undef
#  undef POTION_SIG // do not mix compiled potion with p2 bytecode,
                    // though it should be compatible for now
#  define POTION_SIG P2_SIG

extern PN PN_use;

#endif

//
// additional p2 functions
//

PN p2_source_load(Potion *P, PN cl, PN buf);
PN p2_parse(Potion *, PN, char*);
PN p2_run(Potion *, PN, int);
PN p2_eval(Potion *, PN);
PN p2_sig(Potion *, char *);
PN p2_load(Potion *, PN, PN, PN);

// signature syntax is different.
// Internally for PN_FUNC we still use the better strongly-typed potion
// sigs. i.e "name=S,block=&"
// p2_sig should be really p5_sig or p6_sig
#define P2_FUNC(f, s)   potion_closure_new(P, (PN_F)f, p2_sig(P, s), 0)

#ifdef P2
PN potion_any_is_defined(Potion *, PN, PN);

#define potion_load_code   p2_load_code
#define potion_load        p2_load
#define potion_source_load p2_source_load
#define potion_parse       p2_parse
#define potion_run         p2_run
#define potion_eval        p2_eval
#endif

#endif
