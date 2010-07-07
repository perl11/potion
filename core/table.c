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
  for (k = kh_begin(t); k != kh_end(t); ++k)
    if (kh_exist(PN, t, k)) {
      if (i++ > 0) pn_printf(P, out, ", ");
      potion_bytes_obj_string(P, out, kh_key(PN, t, k));
      pn_printf(P, out, "=");
      potion_bytes_obj_string(P, out, kh_val(PN, t, k));
    }
  pn_printf(P, out, ")");
  return PN_STR_B(out);
}

PN potion_table_empty(Potion *P) {
  return (PN)PN_ALLOC_N(PN_TTABLE, struct PNTable, 0);
}

PN potion_table_cast(Potion *P, PN self) {
  if (PN_IS_TUPLE(self)) {
    int ret; unsigned k;
    vPN(Table) t = PN_ALLOC_N(PN_TTABLE, struct PNTable, 0);
    PN_TUPLE_EACH(self, i, v, {
      k = kh_put(PN, t, PN_NUM(i), &ret);
      PN_QUICK_FWD(struct PNTable *, t);
      kh_val(PN, t, k) = v;
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
  unsigned k = kh_get(PN, t, key);
  if (k != kh_end(t)) return kh_val(PN, t, k);
  return PN_NIL;
}

PN potion_table_each(Potion *P, PN cl, PN self, PN block) {
  vPN(Table) t = (struct PNTable *)potion_fwd(self);
  unsigned k;
  for (k = kh_begin(t); k != kh_end(t); ++k)
    if (kh_exist(PN, t, k)) {
      PN_CLOSURE(block)->method(P, block, self, kh_key(PN, t, k), kh_val(PN, t, k));
    }
  return self;
}

PN potion_table_put(Potion *P, PN cl, PN self, PN key, PN value) {
  int ret;
  vPN(Table) t = (struct PNTable *)potion_fwd(self);
  unsigned k = kh_put(PN, t, key, &ret);
  PN_QUICK_FWD(struct PNTable *, t);
  kh_val(PN, t, k) = value;
  PN_TOUCH(self);
  return self;
}

PN potion_table_remove(Potion *P, PN cl, PN self, PN key) {
  vPN(Table) t = (struct PNTable *)potion_fwd(self);
  unsigned k = kh_get(PN, t, key);
	if (k != kh_end(t)) kh_del(PN, t, k);
  return self;
}

PN potion_table_set(Potion *P, PN self, PN key, PN value) {
  self = potion_fwd(self);
  return potion_table_put(P, PN_NIL, potion_table_cast(P, self), key, value);
}

PN potion_table_length(Potion *P, PN cl, PN self) {
  vPN(Table) t = (struct PNTable *)potion_fwd(self);
  return PN_NUM(kh_size(t));
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

PN potion_tuple_append(Potion *P, PN cl, PN self, PN value) {
  return potion_tuple_push(P, self, value);
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

PN potion_tuple_each(Potion *P, PN cl, PN self, PN block) {
  PN_TUPLE_EACH(self, i, v, {
    PN_CLOSURE(block)->method(P, block, self, v);
  });
  return self;
}

PN potion_tuple_first(Potion *P, PN cl, PN self) {
  if (PN_TUPLE_LEN(self) < 1) return PN_NIL;
  return PN_TUPLE_AT(self, 0);
}

PN potion_tuple_join(Potion *P, PN cl, PN self, PN sep) {
  PN out = potion_byte_str(P, "");
  PN_TUPLE_EACH(self, i, v, {
    if (i > 0 && sep != PN_NIL) potion_bytes_obj_string(P, out, sep);
    potion_bytes_obj_string(P, out, v);
  });
  return PN_STR_B(out);
}

PN potion_tuple_last(Potion *P, PN cl, PN self) {
  long len = PN_TUPLE_LEN(self);
  if (len < 1) return PN_NIL;
  return PN_TUPLE_AT(self, len - 1);
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
  return PN_STR_B(out);
}

PN potion_tuple_pop(Potion *P, PN cl, PN self, PN key) {
  vPN(Tuple) t = PN_GET_TUPLE(self);
  PN obj = t->set[t->len - 1];
  PN_REALLOC(t, PN_TTUPLE, struct PNTuple, sizeof(PN) * (t->len - 1));
  t->len--;
  PN_TOUCH(self);
  return obj;
}

PN potion_tuple_put(Potion *P, PN cl, PN self, PN key, PN value) {
  if (PN_IS_NUM(key)) {
    long i = PN_INT(key), len = PN_TUPLE_LEN(self);
    if (i < 0) i += len;
    if (i < len) {
      PN_TUPLE_AT(self, i) = value;
      PN_TOUCH(self);
      return self;
    } else if (i == len)
      return potion_tuple_push(P, self, value);
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
  PNUniq xu = PN_UNIQ(x);
  long i = 0, j = t->len - 1;
  while (i <= j) {
    long m = j + (i - j) / 2;
    PNUniq u = PN_UNIQ(t->set[m]);
    if (u == xu)
      return m;
    else if (u > xu)
      j = m - 1;
    else
      i = m + 1;
  }
  return -1;
}

void potion_tuple_ins_sort(PN self) {
  struct PNTuple *t = PN_GET_TUPLE(self);
  unsigned long i, j, tmp;
  for (i = 1; i < t->len; i++) {
    j = i;
    while (j > 0 && PN_UNIQ(t->set[j - 1]) > PN_UNIQ(t->set[j])) {
      tmp = t->set[j];
      t->set[j] = t->set[j - 1];
      t->set[j - 1] = tmp;
      j--;
    }
  }
}

PN potion_lobby_list(Potion *P, PN cl, PN self, PN size) {
  return potion_tuple_with_size(P, PN_INT(size));
}

void potion_table_init(Potion *P) {
  PN tbl_vt = PN_VTABLE(PN_TTABLE);
  PN tpl_vt = PN_VTABLE(PN_TTUPLE);
  potion_type_call_is(tbl_vt, PN_FUNC(potion_table_at, "key=o"));
  potion_type_callset_is(tbl_vt, PN_FUNC(potion_table_put, "key=o,value=o"));
  potion_method(tbl_vt, "at", potion_table_at, "key=o");
  potion_method(tbl_vt, "each", potion_table_each, "block=&");
  potion_method(tbl_vt, "length", potion_table_length, 0);
  potion_method(tbl_vt, "put", potion_table_put, "key=o,value=o");
  potion_method(tbl_vt, "remove", potion_table_remove, "index=o");
  potion_method(tbl_vt, "string", potion_table_string, 0);
  potion_type_call_is(tpl_vt, PN_FUNC(potion_tuple_at, "index=N"));
  potion_type_callset_is(tpl_vt, PN_FUNC(potion_tuple_put, "index=N,value=o"));
  potion_method(tpl_vt, "append", potion_tuple_append, "value=o");
  potion_method(tpl_vt, "at", potion_tuple_at, "index=N");
  potion_method(tpl_vt, "each", potion_tuple_each, "block=&");
  potion_method(tpl_vt, "clone", potion_tuple_clone, 0);
  potion_method(tpl_vt, "first", potion_tuple_first, 0);
  potion_method(tpl_vt, "join", potion_tuple_join, "|sep=S");
  potion_method(tpl_vt, "last", potion_tuple_last, 0);
  potion_method(tpl_vt, "length", potion_tuple_length, 0);
  potion_method(tpl_vt, "print", potion_tuple_print, 0);
  potion_method(tpl_vt, "pop", potion_tuple_pop, 0);
  potion_method(tpl_vt, "push", potion_tuple_append, "value=o");
  potion_method(tpl_vt, "put", potion_tuple_put, "index=N,value=o");
  // TODO: add Tuple remove
  potion_method(tpl_vt, "string", potion_tuple_string, 0);
  potion_method(P->lobby, "list", potion_lobby_list, "length=N");
}
