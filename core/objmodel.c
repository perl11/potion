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
  PNType type;
  PN parent;
  kh_PN_t kh[0];
};

unsigned long potion_vt_id = PN_TUSER;

PN potion_closure_new(Potion *P, imp_t meth, PN sig, PN data) {
  struct PNClosure *c = PN_BOOT_OBJ_ALLOC(struct PNClosure, PN_TCLOSURE, 0);
  c->method = meth;
  c->sig = sig;
  c->data = data;
  return (PN)c;
}

PN potion_closure_inspect(Potion *P, PN cl, PN self, PN len) {
  printf("#<closure>");
  return PN_NIL;
}

PN potion_allocate(Potion *P, PN closure, PN self, PN len) {
  struct PNVtable *vt = (struct PNVtable *)self;
  struct PNObject *o = PN_ALLOC2(struct PNObject, PN_INT(len));
  PN_GB(o->gb, NULL, 0);
  o->vt = vt->type;
  return (PN)o;
}

PN potion_type_new(Potion *P, PNType t, PN self) {
  struct PNVtable *vt = PN_CALLOC(struct PNVtable, sizeof(kh_PN_t));
  PN_GB(vt->gb, NULL, 0);
  vt->vt = PN_TVTABLE;
  vt->type = t;
  vt->parent = self;
  PN_VTABLE(t) = (PN)vt;
  return (PN)vt;
}

PN potion_proto_method(Potion *P, PN cl, PN self, PN args) {
  return potion_vm(P, PN_CLOSURE(cl)->data, args, PN_EMPTY);
}

PN potion_getter_method(Potion *P, PN cl, PN self) {
  return PN_CLOSURE(cl)->data;
}

PN potion_def_method(Potion *P, PN closure, PN self, PN key, PN method) {
  int ret;
  struct PNVtable *vt = (struct PNVtable *)self;
  unsigned k = kh_put(PN, vt->kh, key, &ret);
  if (!PN_IS_CLOSURE(method)) {
    if (PN_IS_PROTO(method))
      method = potion_closure_new(P, (imp_t)potion_proto_method, PN_NIL, method);
    else
      method = potion_closure_new(P, (imp_t)potion_getter_method, PN_EMPTY, method);
  }
  return kh_value(vt->kh, k) = method;
}

PN potion_lookup(Potion *P, PN closure, PN self, PN key) {
  int ret;
  struct PNVtable *vt = (struct PNVtable *)self;
  unsigned k = kh_put(PN, vt->kh, key, &ret);
  if (ret) return PN_NIL;
  return kh_value(vt->kh, k);
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

void potion_object_init(Potion *P) {
  PN clo_vt = PN_VTABLE(PN_TCLOSURE);
  potion_method(clo_vt, "inspect", potion_closure_inspect, 0);
}

void potion_lobby_init(Potion *P) {
  P->lobby = PN_VTABLE(PN_TLOBBY);
}
