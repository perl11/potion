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

PN potion_closure_new(Potion *P, PN_F meth, PN sig, unsigned int extra) {
  struct PNClosure *c = PN_BOOT_OBJ_ALLOC(struct PNClosure, PN_TCLOSURE, extra * sizeof(PN));
  c->method = meth;
  c->sig = sig;
  c->extra = extra;
  return (PN)c;
}

PN potion_closure_inspect(Potion *P, PN cl, PN self, PN len) {
  printf("#<closure>");
  return PN_NIL;
}

PN potion_allocate(Potion *P, PN closure, PN self, PN len) {
  struct PNVtable *vt = (struct PNVtable *)self;
  struct PNObject *o = PN_ALLOC2(struct PNObject, PN_INT(len));
  PN_GB(o);
  o->vt = vt->type;
  return (PN)o;
}

void potion_release(Potion *P, PN obj) {
  struct PNGarbage *ptr;
  if (!PN_IS_REF(obj)) return;
  ptr = (struct PNGarbage *)(obj & PN_REF_MASK);
  if (--ptr->next < 1) PN_FREE(ptr);
}

void potion_destroy(Potion *P) {
  potion_release(P, (PN)P);
}

PN potion_type_new(Potion *P, PNType t, PN self) {
  struct PNVtable *vt = PN_CALLOC(struct PNVtable, sizeof(kh_PN_t));
  PN_GB(vt);
  vt->vt = PN_TVTABLE;
  vt->type = t;
  vt->parent = self;
  PN_VTABLE(t) = (PN)vt;
  return (PN)vt;
}

PN potion_proto_method(Potion *P, PN cl, PN self, PN args) {
  return potion_vm(P, PN_CLOSURE(cl)->data[0], args, 0, NULL);
}

PN potion_getter_method(Potion *P, PN cl, PN self) {
  return PN_CLOSURE(cl)->data[0];
}

PN potion_def_method(Potion *P, PN closure, PN self, PN key, PN method) {
  int ret;
  PN cl;
  struct PNVtable *vt = (struct PNVtable *)self;
  unsigned k = kh_put(PN, vt->kh, key, &ret);
  if (!PN_IS_CLOSURE(method)) {
    if (PN_IS_PROTO(method))
      cl = potion_closure_new(P, (PN_F)potion_proto_method, PN_NIL, 1);
    else
      cl = potion_closure_new(P, (PN_F)potion_getter_method, PN_EMPTY, 1);
    PN_CLOSURE(cl)->data[0] = method;
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

PN potion_ref(Potion *P, PN data) {
  if (PN_IS_REF(data)) return data;
  struct PNWeakRef *ref = PN_BOOT_OBJ_ALLOC(struct PNWeakRef, PN_TWEAK, 0);
  ref->data = data;
  return PN_SET_REF(ref);
}

PN potion_ref_inspect(Potion *P, PN cl, PN self, PN len) {
  printf("#<ref>");
  return PN_NIL;
}

void potion_object_init(Potion *P) {
  PN clo_vt = PN_VTABLE(PN_TCLOSURE);
  PN ref_vt = PN_VTABLE(PN_TWEAK);
  potion_method(clo_vt, "inspect", potion_closure_inspect, 0);
  potion_method(ref_vt, "inspect", potion_ref_inspect, 0);
}

void potion_lobby_init(Potion *P) {
  P->lobby = PN_VTABLE(PN_TLOBBY);
}
