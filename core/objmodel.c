//
// objmodel.c
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

PN potion_closure_new(Potion *P, imp_t meth, PN val) {
  struct PNClosure *c = (struct PNClosure *)
    potion_allocate(P, 0, PN_VTABLE(PN_TCLOSURE),
      PN_NUM(sizeof(struct PNClosure)-sizeof(struct PNObject)));
  c->method = meth;
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

PN potion_def_method(Potion *P, PN closure, PN self, PN key, PN method) {
  int i;
  struct PNVtable *vt = (struct PNVtable *)self;
  for (i = 0; i < vt->tally; ++i)
    if (key == vt->p[i].key)
      return vt->p[i].value = method;
  if (vt->tally == vt->size) {
    vt->size *= 2;
    PN_REALLOC_N(vt->p, struct PNPairs, vt->size);
  }
  vt->p[vt->tally].key = key;
  vt->p[vt->tally].value = method;
  vt->tally++;
  return method;
}

PN potion_lookup(Potion *P, PN closure, PN self, PN key) {
  int i;
  struct PNVtable *vt = (struct PNVtable *)self;
  for (i = 0; i < vt->tally; ++i)
    if (key == vt->p[i].key)
      return vt->p[i].value;
  return PN_NIL;
}

PN potion_lookup_str(PN self, char *str) {
  int i;
  struct PNVtable *vt = (struct PNVtable *)self;
  for (i = 0; i < vt->tally; ++i)
    if (!strcmp(str, PN_STR_PTR(vt->p[i].key)))
      return vt->p[i].key;
  return PN_NIL;
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
  if (!closure)
    fprintf(stderr, "lookup failed %lu %s\n", vt, PN_STR_PTR(msg));
  return closure;
}
