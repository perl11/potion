//
// table.c
// the central table type, based on khash
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "internal.h"
#include "potion.h"
#include "khash.h"
#include "table.h"

#define PN_SET_TUPLE(t) (((PN)t)|PN_TUPLE_FLAG)
#define PN_GET_TUPLE(t) ((struct PNTuple *)(((PN)t)^PN_TUPLE_FLAG))

inline PN potion_tuple_new(Potion *P, PN value) {
  struct PNTuple *t = PN_OBJ_ALLOC(struct PNTuple, PN_TTUPLE, sizeof(PN));
  t->len = 1;
  t->set[0] = value;
  return PN_SET_TUPLE(t);
}

inline PN potion_tuple_push(Potion *P, PN tuple, PN value) {
  struct PNTuple *t;
  if (tuple == PN_EMPTY)
    return potion_tuple_new(P, value);
  t = PN_GET_TUPLE(tuple);
  t = (struct PNTuple *)realloc((char *)t, sizeof(struct PNTuple) +
                                (sizeof(PN) * ++t->len));
  // be sure new pointer is registered with GC
  t->set[t->len-1] = value;
  return PN_SET_TUPLE(t);
}

PN potion_table_new(Potion *P, PN closure, PN self) {
  // struct PNTable *t = PN_OBJ_ALLOC(struct PNTable, PN_TTABLE, 0);
  return PN_EMPTY;
}

PN potion_table_inspect(Potion *P, PN cl, PN self) {
  struct PNTable *t = (struct PNTable *)self;
  kh_PN_t *h = &t->kh;
  unsigned k;
  for (k = kh_begin(h); k != kh_end(h); ++k)
    if (kh_exist(h, k)) {
      potion_send(kh_value(h, k), PN_inspect);
      printf("\n");
    }
  return PN_NIL;
}

PN potion_tuple_inspect(Potion *P, PN cl, PN self) {
  struct PNTuple *t = PN_GET_TUPLE(self);
  printf("(");
  if (self != PN_EMPTY) {
    unsigned long i;
    for (i = 0; i < t->len; i++) {
      if (i > 0) printf(", ");
      potion_send(t->set[i], PN_inspect);
    }
  }
  printf(")");
  return PN_NIL;
}

void potion_table_init(Potion *P) {
  PN tbl_vt = PN_VTABLE(PN_TTABLE);
  PN tpl_vt = PN_VTABLE(PN_TTUPLE);
  potion_method(tbl_vt, "new", potion_table_new, 0);
  potion_method(tbl_vt, "inspect", potion_table_inspect, 0);
  potion_method(tpl_vt, "inspect", potion_tuple_inspect, 0);
}
