//
// objmodel.c
// much of this is based on the work of ian piumarta
// <http://www.piumarta.com/pepsi/objmodel.pdf>
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "potion.h"
#include "internal.h"
#include "khash.h"
#include "table.h"

struct PNVtable {
  PN_OBJECT_HEADER
  kh_PN_t kh;
  PNType type;
  PN parent;
};

unsigned long potion_vt_id = PN_TUSER;

PN potion_closure_new(Potion *P, imp_t meth, PN sig, PN val) {
  struct PNClosure *c = PN_BOOT_OBJ_ALLOC(struct PNClosure, PN_TCLOSURE, 0);
  c->method = meth;
  c->sig = sig;
  c->value = val;
  return (PN)c;
}

PN potion_allocate(Potion *P, PN closure, PN self, PN len) {
  struct PNVtable *vt = (struct PNVtable *)self;
  struct PNObject *o = PN_ALLOC2(struct PNObject, PN_INT(len));
  PN_GB(o->gb, NULL, 0);
  o->vt = vt->type;
  return (PN)o;
}

PN potion_type_new(Potion *P, PNType t, PN self) {
  struct PNVtable *vt = PN_ALLOC(struct PNVtable);
  PN_GB(vt->gb, NULL, 0);
  vt->vt = PN_TVTABLE;
  vt->type = t;
  vt->parent = self;
  PN_VTABLE(t) = (PN)vt;
  return (PN)vt;
}

PN potion_def_method(Potion *P, PN closure, PN self, PN key, PN method) {
  int ret;
  struct PNVtable *vt = (struct PNVtable *)self;
  unsigned k = kh_put(PN, &vt->kh, key, &ret);
  return kh_value(&vt->kh, k) = method;
}

PN potion_lookup(Potion *P, PN closure, PN self, PN key) {
  int ret;
  struct PNVtable *vt = (struct PNVtable *)self;
  unsigned k = kh_put(PN, &vt->kh, key, &ret);
  if (ret) return PN_NIL;
  return kh_value(&vt->kh, k);
}

PN potion_bind(Potion *P, PN rcv, PN msg) {
  PN closure, vt;
  PNType t = PN_TYPE(rcv);
  if (t >= P->typen) return PN_NIL;
  vt = PN_VTABLE(t);
  // TODO: enable mcache -- need to hash the (t,msg) tuple
  closure = ((msg == PN_lookup) && (t == PN_TVTABLE))
    ? potion_lookup(P, 0, vt, msg)
    : potion_send(vt, PN_lookup, msg);
  // TODO: replace with `forward` method call
  if (!closure)
    fprintf(stderr, "lookup failed %lu %s\n", vt, PN_STR_PTR(msg));
  return closure;
}

void potion_lobby_init(Potion *P) {
  struct PNTable *t = PN_BOOT_OBJ_ALLOC(struct PNTable, PN_TTABLE, 0);
  P->lobby = (PN)t;
}
