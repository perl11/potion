//
// table.c
// the central table type, based on khash
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

PN potion_table_inspect(Potion *P, PN cl, PN self) {
  struct PNTable *t = (struct PNTable *)self;
  PN out = potion_byte_str(P, "(");
  kh_PN_t *h = t->kh;
  unsigned k;
  for (k = kh_begin(h); k != kh_end(h); ++k)
    if (kh_exist(h, k)) {
      potion_bytes_obj_inspect(P, out, kh_value(h, k));
      pn_printf(P, out, "\n");
    }
  pn_printf(P, out, ")");
  return out;
}

PN potion_table_length(Potion *P, PN cl, PN self) {
  struct PNTable *t = (struct PNTable *)self;
  return PN_NUM(kh_end(t->kh));
}

PN potion_table__link(Potion *P, PN cl, PN self, PN link) {
  struct PNTable *t = (struct PNTable *)self;
  kh_PN_t *h = t->kh;
  unsigned k;
  for (k = kh_begin(h); k != kh_end(h); ++k)
    if (kh_exist(h, k))
      PN_LINK(kh_value(h, k));
  return link;
}

#define NEW_TUPLE(t, size, ptr) \
  struct PNTuple *t = PN_OBJ_ALLOC(struct PNTuple, PN_TTUPLE, 0); \
  t->len = size; \
  t->set = ptr

PN potion_tuple_empty(Potion *P) {
  NEW_TUPLE(t, 0, NULL);
  return PN_SET_TUPLE(t);
}

PN potion_tuple_with_size(Potion *P, PN_SIZE size) {
  NEW_TUPLE(t, size, PN_ALLOC_N(PN, size));
  return PN_SET_TUPLE(t);
}

PN potion_tuple_new(Potion *P, PN value) {
  NEW_TUPLE(t, 1, PN_ALLOC_N(PN, 1));
  t->set[0] = value;
  return PN_SET_TUPLE(t);
}

PN potion_tuple_push(Potion *P, PN tuple, PN value) {
  struct PNTuple *t = PN_GET_TUPLE(tuple);
  if (t->set == NULL)
    t->set = PN_ALLOC_N(PN, ++t->len);
  else
    PN_REALLOC_N(t->set, PN, ++t->len);
  t->set[t->len-1] = value;
  return tuple;
}

PN_SIZE potion_tuple_find(Potion *P, PN tuple, PN value) {
  PN_TUPLE_EACH(tuple, i, v, {
    if (v == value) return i;
  });
  return -1;
}

PN_SIZE potion_tuple_put(Potion *P, PN tuple, PN value) {
  PN_SIZE idx = potion_tuple_find(P, tuple, value);
  if (idx != -1) return idx;

  potion_tuple_push(P, tuple, value);
  return PN_TUPLE_LEN(tuple) - 1;
}

PN potion_tuple_at(Potion *P, PN cl, PN self, PN index) {
  long i = PN_INT(index), len = PN_TUPLE_LEN(self);
  if (i < 0) i += len;
  if (i >= len) return PN_NIL;
  return PN_TUPLE_AT(self, i);
}

PN potion_tuple_inspect(Potion *P, PN cl, PN self) {
  PN out = potion_byte_str(P, "(");
  PN_TUPLE_EACH(self, i, v, {
    if (i > 0) pn_printf(P, out, ", ");
    potion_bytes_obj_inspect(P, out, v);
  });
  pn_printf(P, out, ")");
  return out;
}

PN potion_tuple_print(Potion *P, PN cl, PN self) {
  PN_TUPLE_EACH(self, i, v, {
    potion_send(v, PN_print);
  });
  return PN_NIL;
}

PN potion_tuple_length(Potion *P, PN cl, PN self) {
  return PN_NUM(PN_TUPLE_LEN(self));
}

PN potion_tuple__link(Potion *P, PN cl, PN self, PN link) {
  PN_TUPLE_EACH(self, i, v, {
    PN_LINK(v);
  });
  return link;
}

void potion_table_init(Potion *P) {
  PN tbl_vt = PN_VTABLE(PN_TTABLE);
  PN tpl_vt = PN_VTABLE(PN_TTUPLE);
  potion_method(tbl_vt, "inspect", potion_table_inspect, 0);
  potion_method(tbl_vt, "length", potion_table_length, 0);
  potion_method(tpl_vt, "~link", potion_table__link, 0);
  potion_method(tpl_vt, "at", potion_tuple_at, "index=N");
  potion_method(tpl_vt, "inspect", potion_tuple_inspect, 0);
  potion_method(tpl_vt, "length", potion_tuple_length, 0);
  potion_method(tpl_vt, "print", potion_tuple_print, 0);
  potion_method(tpl_vt, "~link", potion_tuple__link, 0);
}
