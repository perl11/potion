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

PN potion_table_string(Potion *P, PN cl, PN self) {
  struct PNTable *t = (struct PNTable *)self;
  PN out = potion_byte_str(P, "(");
  kh_PN_t *h = t->kh;
  unsigned k, i = 0;
  for (k = kh_begin(h); k != kh_end(h); ++k)
    if (kh_exist(h, k)) {
      if (i++ > 0) pn_printf(P, out, ", ");
      potion_bytes_obj_string(P, out, kh_key(h, k));
      pn_printf(P, out, "=");
      potion_bytes_obj_string(P, out, kh_value(h, k));
    }
  pn_printf(P, out, ")");
  return out;
}

PN potion_table_cast(Potion *P, PN self) {
  if (PN_IS_TUPLE(self)) {
    int ret; unsigned k;
    kh_PN_t *kh = PN_CALLOC(kh_PN_t, 0);
    struct PNTuple *t = PN_GET_TUPLE(self);
    PN_TUPLE_EACH(t, i, v, {
      k = kh_put(PN, kh, PN_NUM(i), &ret);
      kh_value(kh, k) = v;
    });
    PN_FREE(t->set);
    t->vt = PN_TTABLE;
    ((struct PNTable *)self)->kh = kh;
  }
  return self;
}

PN potion_table_at(Potion *P, PN cl, PN self, PN key) {
  int ret;
  struct PNTable *t = (struct PNTable *)self;
  unsigned k = kh_put(PN, t->kh, key, &ret);
  if (ret) return PN_NIL;
  return kh_value(t->kh, k);
}

PN potion_table_put(Potion *P, PN cl, PN self, PN key, PN value) {
  int ret;
  struct PNTable *t = (struct PNTable *)self;
  unsigned k = kh_put(PN, t->kh, key, &ret);
  kh_value(t->kh, k) = value;
  return self;
}

PN potion_table_remove(Potion *P, PN cl, PN self, PN key) {
  int ret;
  struct PNTable *t = (struct PNTable *)self;
  unsigned k = kh_put(PN, t->kh, key, &ret);
	if (!ret) kh_del(PN, t->kh, k);
  return self;
}

PN potion_table_set(Potion *P, PN self, PN key, PN value) {
  return potion_table_put(P, PN_NIL, potion_table_cast(P, self), key, value);
}

PN potion_table_length(Potion *P, PN cl, PN self) {
  struct PNTable *t = (struct PNTable *)self;
  return PN_NUM(kh_size(t->kh));
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

PN_SIZE potion_tuple_push_unless(Potion *P, PN tuple, PN value) {
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

PN potion_tuple_string(Potion *P, PN cl, PN self) {
  PN out = potion_byte_str(P, "(");
  PN_TUPLE_EACH(self, i, v, {
    if (i > 0) pn_printf(P, out, ", ");
    potion_bytes_obj_string(P, out, v);
  });
  pn_printf(P, out, ")");
  return out;
}

PN potion_tuple_put(Potion *P, PN cl, PN self, PN key, PN value) {
  if (PN_IS_NUM(key)) {
    long i = PN_INT(key), len = PN_TUPLE_LEN(self);
    if (i <= len) {
      PN_TUPLE_AT(self, i) = value;
      return self;
    }
  }
  return potion_table_put(P, PN_NIL, potion_table_cast(P, self), key, value);
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
  potion_method(tbl_vt, "at", potion_table_at, "index=o");
  potion_method(tbl_vt, "length", potion_table_length, 0);
  potion_method(tbl_vt, "put", potion_table_put, "index=o,value=o");
  potion_method(tbl_vt, "remove", potion_table_remove, "index=o");
  potion_method(tbl_vt, "string", potion_table_string, 0);
  potion_method(tpl_vt, "~link", potion_table__link, 0);
  potion_method(tpl_vt, "at", potion_tuple_at, "index=N");
  potion_method(tpl_vt, "length", potion_tuple_length, 0);
  potion_method(tpl_vt, "print", potion_tuple_print, 0);
  potion_method(tpl_vt, "put", potion_tuple_put, "index=o,value=o");
  // TODO: add Tuple remove
  potion_method(tpl_vt, "string", potion_tuple_string, 0);
  potion_method(tpl_vt, "~link", potion_tuple__link, 0);
}
