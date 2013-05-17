//
// p2.h - extend the API for p2/perl
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

#include "table.h"

/// a namespace is a hash of dynamic symbolnames
///
struct PNNamespace {
  PN_OBJECT_HEADER;  ///< PNType vt; PNUniq uniq
  PN_TABLE_HEADER;   ///< PN_SIZE n_buckets, size, n_occupied, upper_bound
  char table[0];
};

#define PN_TNAMESPACE PN_TUSER+1
PN potion_nstuple_set(Potion *P, PN name);
PN potion_nstuple_push(Potion *P, PN name);
PN potion_nstuple_pop(Potion *P);
PN potion_pkg(Potion *P, PN cl, PN self);
PN potion_pkg_create(Potion *P, PN pkg);
PN potion_pkg_put(Potion *P, PN name, PN value);
PN potion_pkg_at(Potion *P, PN cl, PN self, PN key);
PN potion_sym_at(Potion *P, PN name); //not yet
PN potion_namespace_create(Potion *P, PN cl, PN self, PN pkg);
PN potion_namespace_put(Potion *P, PN cl, PN self, PN name, PN value);
PN potion_namespace_at(Potion *P, PN cl, PN self, PN key);
void potion_p2_init(Potion *);
#endif

//
// additional p2 functions
//

PN p2_source_load(Potion *P, PN cl, PN buf);
PN p2_parse(Potion *, PN, char*);
PN p2_run(Potion *, PN, int);
PN p2_eval(Potion *, PN, int);
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
