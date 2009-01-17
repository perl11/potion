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
  kh_str_t kh[0];
};

PN potion_closure_new(Potion *P, PN_F meth, PN sig, PN_SIZE extra) {
  PN_SIZE i;
  struct PNClosure *c = PN_BOOT_OBJ_ALLOC(struct PNClosure, PN_TCLOSURE, extra * sizeof(PN));
  c->method = meth;
  c->sig = sig;
  c->extra = extra;
  for (i = 0; i < c->extra; i++)
    c->data[i] = PN_NIL;
  return (PN)c;
}

PN potion_closure_string(Potion *P, PN cl, PN self, PN len) {
  return potion_byte_str(P, "#<closure>");
}

PN potion_closure__link(Potion *P, PN cl, PN self, PN link) {
  PN_SIZE i;
  struct PNClosure *c = PN_CLOSURE(self);
  // TODO: link function code if allocated by the jit
  PN_LINK(c->sig);
  for (i = 0; i < c->extra; i++)
    PN_LINK(c->data[i]);
  return link;
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
  if (!PN_IS_PTR(obj)) return;
  ptr = (struct PNGarbage *)(obj & PN_REF_MASK);
  if (--ptr->next < 1) PN_FREE(ptr);
}

void potion_destroy(Potion *P) {
  potion_release(P, (PN)P);
}

PN potion_type_new(Potion *P, PNType t, PN self) {
  struct PNVtable *vt = PN_CALLOC(struct PNVtable, sizeof(kh_str_t));
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
  unsigned k = kh_put(str, vt->kh, ((struct PNString *)key)->hash, &ret);
  if (!PN_IS_CLOSURE(method)) {
    if (PN_IS_PROTO(method))
      cl = potion_closure_new(P, (PN_F)potion_proto_method, PN_NIL, 1);
    else
      cl = potion_closure_new(P, (PN_F)potion_getter_method, PN_NIL, 1);
    PN_CLOSURE(cl)->data[0] = method;
    method = cl;
  }
  return kh_value(vt->kh, k) = method;
}

PN potion_lookup(Potion *P, PN closure, PN self, PN key) {
  struct PNVtable *vt = (struct PNVtable *)self;
  unsigned k = kh_get(str, vt->kh, ((struct PNString *)key)->hash);
  if (k != kh_end(vt->kh)) return kh_value(vt->kh, k);
  return PN_NIL;
}

PN potion_bind(Potion *P, PN rcv, PN msg) {
  PN closure = PN_NIL, vt;
  PNType t = PN_TYPE(rcv);
  if (t >= P->typen) return PN_NIL;
  vt = PN_VTABLE(t);
  // TODO: enable mcache -- need to hash the (t,msg) tuple
  while (vt) {
    closure = ((msg == PN_lookup) && (t == PN_TVTABLE))
      ? potion_lookup(P, 0, vt, msg)
      : potion_send(vt, PN_lookup, msg);
    if (closure) break;
    vt = ((struct PNVtable *)vt)->parent; 
  }
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

PN potion_ref_string(Potion *P, PN cl, PN self, PN len) {
  return potion_byte_str(P, "#<ref>");
}

PN potion_ref__link(Potion *P, PN cl, PN self, PN link) {
  struct PNWeakRef *r = PN_GET_REF(self);
  PN_LINK(r->data);
  return link;
}

PN potion_object_string(Potion *P, PN cl, PN self, PN len) {
  return potion_byte_str(P, "#<object>");
}

PN potion_object_forward(Potion *P, PN cl, PN self, PN method) {
  printf("#<object>");
  return PN_NIL;
}

PN potion_object_send(Potion *P, PN cl, PN self, PN method) {
  return potion_send_dyn(self, method);
}

PN potion_object__link(Potion *P, PN cl, PN self, PN link) {
  return link;
}

PN potion_lobby_self(Potion *P, PN cl, PN self) {
  return self;
}

PN potion_lobby_kind(Potion *P, PN cl, PN self) {
  PNType t = PN_TYPE(self);
  if (t >= P->typen) return PN_NIL; // TODO: error
  return PN_VTABLE(t);
}

void potion_object_init(Potion *P) {
  PN clo_vt = PN_VTABLE(PN_TCLOSURE);
  PN ref_vt = PN_VTABLE(PN_TWEAK);
  PN obj_vt = PN_VTABLE(PN_TOBJECT);
  potion_method(clo_vt, "string", potion_closure_string, 0);
  potion_method(clo_vt, "~link", potion_closure__link, 0);
  potion_method(ref_vt, "string", potion_ref_string, 0);
  potion_method(ref_vt, "~link", potion_ref__link, 0);
  potion_method(obj_vt, "forward", potion_object_forward, 0);
  potion_method(obj_vt, "send", potion_object_send, 0);
  potion_method(obj_vt, "string", potion_object_string, 0);
  potion_method(obj_vt, "~link", potion_object__link, 0);
}

void potion_lobby_init(Potion *P) {
  potion_send(P->lobby, PN_def, potion_str(P, "Lobby"),    P->lobby);
  potion_send(P->lobby, PN_def, potion_str(P, "Mixin"),    PN_VTABLE(PN_TVTABLE));
  potion_send(P->lobby, PN_def, potion_str(P, "Object"),   PN_VTABLE(PN_TOBJECT));
  potion_send(P->lobby, PN_def, potion_str(P, "NilKind"),  PN_VTABLE(PN_TNIL));
  potion_send(P->lobby, PN_def, potion_str(P, "Number"),   PN_VTABLE(PN_TNUMBER));
  potion_send(P->lobby, PN_def, potion_str(P, "Boolean"),  PN_VTABLE(PN_TBOOLEAN));
  potion_send(P->lobby, PN_def, potion_str(P, "String"),   PN_VTABLE(PN_TSTRING));
  potion_send(P->lobby, PN_def, potion_str(P, "Table"),    PN_VTABLE(PN_TTABLE));
  potion_send(P->lobby, PN_def, potion_str(P, "Closure"),  PN_VTABLE(PN_TCLOSURE));
  potion_send(P->lobby, PN_def, potion_str(P, "Tuple"),    PN_VTABLE(PN_TTUPLE));
  potion_send(P->lobby, PN_def, potion_str(P, "Potion"),   PN_VTABLE(PN_TSTATE));
  potion_send(P->lobby, PN_def, potion_str(P, "Source"),   PN_VTABLE(PN_TSOURCE));
  potion_send(P->lobby, PN_def, potion_str(P, "Bytes"),    PN_VTABLE(PN_TBYTES));
  potion_send(P->lobby, PN_def, potion_str(P, "Compiled"), PN_VTABLE(PN_TPROTO));
  potion_send(P->lobby, PN_def, potion_str(P, "Ref"),      PN_VTABLE(PN_TWEAK));

  potion_method(P->lobby, "self", potion_lobby_self, 0);
  potion_method(P->lobby, "kind", potion_lobby_kind, 0);
}
