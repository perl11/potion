//
// objmodel.h
// much of this is based on the work of ian piumarta
// <http://www.piumarta.com/pepsi/objmodel.pdf>
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include "internal.h"
#include "potion.h"

unsigned long potion_vt_id = PN_TUSER;

PN potion_closure_new(imp_t meth, PN val) {
  struct PNClosure *c = PN_ALLOC(struct PNClosure);
  PN_GB(c->gb, NULL, 0);
  c->vt = PN_TCLOSURE;
  c->method = meth;
  c->value = val;
  return (PN)c;
}

PN potion_bind(PN rcv, PN msg) {
  return PN_NIL;
}
