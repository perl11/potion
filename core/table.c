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

PN potion_table_string(Potion *P, PNv cl, PNv self) {
  struct PNTable * volatile t = (struct PNTable *)self;
  PNv out = potion_byte_str(P, "(");
  unsigned k, i = 0;
  for (k = kh_begin(t->kh); k != kh_end(t->kh); ++k)
    if (kh_exist(t->kh, k)) {
      if (i++ > 0) pn_printf(P, out, ", ");
      potion_bytes_obj_string(P, out, kh_key(t->kh, k));
      pn_printf(P, out, "=");
      potion_bytes_obj_string(P, out, kh_value(t->kh, k));
    }
  pn_printf(P, out, ")");
  return out;
}

PN potion_table_cast(Potion *P, PNv self) {
  if (PN_IS_TUPLE(self)) {
    int ret; unsigned k;
    kh__PN_t *kh = PN_CALLOC(kh__PN_t, 0);
    struct PNTuple * volatile t = PN_GET_TUPLE(self);
    PN_TUPLE_EACH(t, i, v, {
      k = kh_put(_PN, kh, PN_NUM(i), &ret);
      kh_value(kh, k) = v;
    });
    SYS_FREE(t->set);
    t->vt = PN_TTABLE;
    ((struct PNTable *)self)->kh = kh;
  }
  return self;
}

PN potion_table_at(Potion *P, PN cl, PN self, PN key) {
  int ret;
  struct PNTable *t = (struct PNTable *)self;
  unsigned k = kh_put(_PN, t->kh, key, &ret);
  if (ret) return PN_NIL;
  return kh_value(t->kh, k);
}

PN potion_table_put(Potion *P, PN cl, PN self, PN key, PN value) {
  int ret;
  struct PNTable *t = (struct PNTable *)self;
  unsigned k = kh_put(_PN, t->kh, key, &ret);
  kh_value(t->kh, k) = value;
  return self;
}

PN potion_table_remove(Potion *P, PN cl, PN self, PN key) {
  int ret;
  struct PNTable *t = (struct PNTable *)self;
  unsigned k = kh_put(_PN, t->kh, key, &ret);
	if (!ret) kh_del(_PN, t->kh, k);
  return self;
}

PN potion_table_set(Potion *P, PN self, PN key, PN value) {
  return potion_table_put(P, PN_NIL, potion_table_cast(P, self), key, value);
}

PN potion_table_length(Potion *P, PN cl, PN self) {
  struct PNTable *t = (struct PNTable *)self;
  return PN_NUM(kh_size(t->kh));
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
  NEW_TUPLE(t, size, SYS_CALLOC_N(PN, size));
  return PN_SET_TUPLE(t);
}

PN potion_tuple_new(Potion *P, PN value) {
  NEW_TUPLE(t, 1, SYS_ALLOC_N(PN, 1));
  t->set[0] = value;
  return PN_SET_TUPLE(t);
}

PN potion_tuple_push(Potion *P, PN tuple, PN value) {
  struct PNTuple *t = PN_GET_TUPLE(tuple);
  if (t->set == NULL)
    t->set = SYS_ALLOC_N(PN, ++t->len);
  else
    SYS_REALLOC_N(t->set, PN, ++t->len);
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

PN potion_tuple_clone(Potion *P, PN cl, PN self) {
  struct PNTuple *t1 = PN_GET_TUPLE(self);
  NEW_TUPLE(t2, t1->len, SYS_ALLOC_N(PN, t1->len));
  PN_MEMCPY_N(t2->set, t1->set, PN, t1->len);
  return PN_SET_TUPLE(t2);
}

PN potion_tuple_join(Potion *P, PN cl, PN self, PN sep) {
  PN out = potion_byte_str(P, "");
  PN_TUPLE_EACH(self, i, v, {
    if (sep) potion_bytes_append(P, PN_NIL, out, sep);
    potion_bytes_obj_string(P, out, v);
  });
  return out;
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

PN potion_lobby_list(Potion *P, PN cl, PN self, PN size) {
  return potion_tuple_with_size(P, PN_INT(size));
}

void potion_table_init(Potion *P) {
  PN tbl_vt = PN_VTABLE(PN_TTABLE);
  PN tpl_vt = PN_VTABLE(PN_TTUPLE);
  potion_type_func(tbl_vt, (PN_F)potion_table_at);
  potion_method(tbl_vt, "at", potion_table_at, "index=o");
  potion_method(tbl_vt, "length", potion_table_length, 0);
  potion_method(tbl_vt, "put", potion_table_put, "index=o,value=o");
  potion_method(tbl_vt, "remove", potion_table_remove, "index=o");
  potion_method(tbl_vt, "string", potion_table_string, 0);
  potion_type_func(tpl_vt, (PN_F)potion_tuple_at);
  potion_method(tpl_vt, "at", potion_tuple_at, "index=N");
  potion_method(tpl_vt, "clone", potion_tuple_clone, 0);
  potion_method(tpl_vt, "join", potion_tuple_join, 0);
  potion_method(tpl_vt, "length", potion_tuple_length, 0);
  potion_method(tpl_vt, "print", potion_tuple_print, 0);
  potion_method(tpl_vt, "put", potion_tuple_put, "index=o,value=o");
  // TODO: add Tuple remove
  potion_method(tpl_vt, "string", potion_tuple_string, 0);
  potion_method(P->lobby, "list", potion_lobby_list, 0);
}
