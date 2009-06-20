//
// table.c
// the central table type, based on khash
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include "potion.h"
#include "internal.h"
#include "khash.h"
#include "table.h"

PN potion_table_string(Potion *P, PN cl, PN self) {
  vPN(Table) t = (struct PNTable *)potion_fwd(self);
  PN out = potion_byte_str(P, "(");
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

PN potion_table_empty(Potion *P) {
  return (PN)PN_ALLOC_N(PN_TTABLE, struct PNTable, sizeof(kh_PN_t));
}

PN potion_table_cast(Potion *P, PN self) {
  if (PN_IS_TUPLE(self)) {
    int ret; unsigned k;
    vPN(Table) t = PN_ALLOC_N(PN_TTABLE, struct PNTable, sizeof(kh_PN_t));
    PN_TUPLE_EACH(self, i, v, {
      k = kh_put(PN, t->kh, PN_NUM(i), &ret);
      kh_value(t->kh, k) = v;
    });
    ((struct PNFwd *)self)->fwd = POTION_FWD;
    ((struct PNFwd *)self)->siz = potion_type_size(P, (const struct PNObject *)self);
    ((struct PNFwd *)self)->ptr = (PN)t;
    PN_TOUCH(self);
    self = (PN)t;
  }
  return self;
}

PN potion_table_at(Potion *P, PN cl, PN self, PN key) {
  vPN(Table) t = (struct PNTable *)potion_fwd(self);
  unsigned k = kh_get(PN, t->kh, key);
  if (k != kh_end(t->kh)) return kh_value(t->kh, k);
  return PN_NIL;
}

PN potion_table_put(Potion *P, PN cl, PN self, PN key, PN value) {
  int ret;
  vPN(Table) t = (struct PNTable *)potion_fwd(self);
  unsigned k = kh_put(PN, t->kh, key, &ret);
  kh_value(t->kh, k) = value;
  PN_TOUCH(self);
  return self;
}

PN potion_table_remove(Potion *P, PN cl, PN self, PN key) {
  int ret;
  vPN(Table) t = (struct PNTable *)potion_fwd(self);
  unsigned k = kh_put(PN, t->kh, key, &ret);
	if (!ret) kh_del(PN, t->kh, k);
  return self;
}

PN potion_table_set(Potion *P, PN self, PN key, PN value) {
  self = potion_fwd(self);
  return potion_table_put(P, PN_NIL, potion_table_cast(P, self), key, value);
}

PN potion_table_length(Potion *P, PN cl, PN self) {
  self = potion_fwd(self);
  vPN(Table) t = (struct PNTable *)self;
  return PN_NUM(kh_size(t->kh));
}

#define NEW_TUPLE(t, size) \
  vPN(Tuple) t = PN_ALLOC_N(PN_TTUPLE, struct PNTuple, size * sizeof(PN)); \
  t->len = size

PN potion_tuple_empty(Potion *P) {
  NEW_TUPLE(t, 0);
  return (PN)t;
}

PN potion_tuple_with_size(Potion *P, unsigned long size) {
  NEW_TUPLE(t, size);
  return (PN)t;
}

PN potion_tuple_new(Potion *P, PN value) {
  NEW_TUPLE(t, 1);
  t->set[0] = value;
  return (PN)t;
}

PN potion_tuple_push(Potion *P, PN tuple, PN value) {
  vPN(Tuple) t = PN_GET_TUPLE(tuple);
  PN_REALLOC(t, PN_TTUPLE, struct PNTuple, sizeof(PN) * (t->len + 1));
  t->set[t->len] = value;
  t->len++;
  PN_TOUCH(tuple);
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
  vPN(Tuple) t1 = PN_GET_TUPLE(self);
  NEW_TUPLE(t2, t1->len);
  PN_MEMCPY_N(t2->set, t1->set, PN, t1->len);
  return (PN)t2;
}

PN potion_tuple_each(Potion *P, PN cl, PN self, PN nil, PN block) {
  PN_TUPLE_EACH(self, i, v, {
    PN_CLOSURE(block)->method(P, block, v);
  });
  return self;
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
  int licks = 0;
  PN out = potion_byte_str(P, "(");
  PN_TUPLE_EACH(self, i, v, {
    if (i > 0) pn_printf(P, out, ", ");
    if (PN_TYPE(v) == PN_TLICK) licks++;
    potion_bytes_obj_string(P, out, v);
  });

  licks = (licks > 0 && licks == PN_TUPLE_LEN(self));
  if (licks) PN_STR_PTR(out)[0] = '[';
  pn_printf(P, out, licks ? "]" : ")");
  return out;
}

PN potion_tuple_put(Potion *P, PN cl, PN self, PN key, PN value) {
  if (PN_IS_NUM(key)) {
    long i = PN_INT(key), len = PN_TUPLE_LEN(self);
    if (i <= len) {
      PN_TUPLE_AT(self, i) = value;
      PN_TOUCH(self);
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

long potion_tuple_binary_search(PN self, PN x) {
  struct PNTuple *t = PN_GET_TUPLE(self);
  long i = 0, j = t->len - 1;
  while (i <= j) {
    long m = (i + j) / 2;
    if (t->set[m] == x)
      return m;
    else if (t->set[m] > x)
      j = m - 1;
    else
      i = m + 1;
  }
  return -1;
}

// TODO: replace with bsearch from libc and using object comparison
PN potion_tuple_bsearch(Potion *P, PN cl, PN self, PN x) {
  long idx = potion_tuple_binary_search(self, x);
  return idx == -1 ? PN_NIL : PN_NUM(idx);
}

void potion_tuple_ins_sort(PN self) {
  struct PNTuple *t = PN_GET_TUPLE(self);
  unsigned long i, j, tmp;
  for (i = 1; i < t->len; i++) {
    j = i;
    while (j > 0 && t->set[j - 1] > t->set[j]) {
      tmp = t->set[j];
      t->set[j] = t->set[j - 1];
      t->set[j - 1] = tmp;
      j--;
    }
  }
}

PN potion_tuple_nsort(Potion *P, PN cl, PN self) {
  potion_tuple_ins_sort(self);
  return self;
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
  potion_method(tpl_vt, "bsearch", potion_tuple_bsearch, "value=N");
  potion_method(tpl_vt, "each", potion_tuple_each, "index=N");
  potion_method(tpl_vt, "clone", potion_tuple_clone, 0);
  potion_method(tpl_vt, "join", potion_tuple_join, 0);
  potion_method(tpl_vt, "length", potion_tuple_length, 0);
  potion_method(tpl_vt, "print", potion_tuple_print, 0);
  potion_method(tpl_vt, "put", potion_tuple_put, "index=o,value=o");
  // TODO: add Tuple remove
  potion_method(tpl_vt, "nsort", potion_tuple_nsort, 0);
  potion_method(tpl_vt, "string", potion_tuple_string, 0);
  potion_method(P->lobby, "list", potion_lobby_list, 0);
}
