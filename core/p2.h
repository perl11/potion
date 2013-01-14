//
// p2.h
//
// (c) 2008 why the lucky stiff, the freelance professor
// (c) 2013 cPanel, written by rurban
//
#ifndef P2_H
#define P2_H

#define P2_VERSION  "0.0"
#define P2_MINOR    0
#define P2_MAJOR    0
#define P2_SIG      "p\02\10n"
#define P2_VMID     0x79

#include "potion.h"

//
// additional p2 functions
//

PN p2_parse(Potion *, PN);
PN p2_vm_proto(Potion *, PN, PN, ...);
PN p2_vm_class(Potion *, PN, PN);
PN p2_vm(Potion *, PN, PN, PN, PN_SIZE, PN * volatile);
PN p2_eval(Potion *, PN, int);
PN p2_run(Potion *, PN, int);
PN_F p2_jit_proto(Potion *, PN, PN);

#endif
