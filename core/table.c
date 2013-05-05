///\file table.c
/// implement unordered hashes and ordered lists (PNTable and PNTuple)
///\class PNTable - unordered hash, the central table type, based on khash
///\class PNTuple - ordered list (array)
//
// (c) 2008 why the lucky stiff, the freelance professor
// (c) 2013 perl11 org
//
#include <stdio.h>
#include <stdlib.h>
#include "potion.h"
#include "internal.h"
#include "khash.h"
#include "table.h"

///\memberof PNTable
/// "string" method
///\return PNString
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

///\memberof PNTable
/// "empty" method
///\return PNTable
PN potion_table_empty(Potion *P) {
  return (PN)PN_ALLOC_N(PN_TTABLE, struct PNTable, 0);
}

///\return self PNTable
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

///\memberof PNTable
/// "at" method
///\param key PN
///\return PN value or PN_NIL
PN potion_table_at(Potion *P, PN cl, PN self, PN key) {
  vPN(Table) t = (struct PNTable *)potion_fwd(self);
  unsigned k = kh_get(PN, t, key);
  if (k != kh_end(t)) return kh_val(PN, t, k);
  return PN_NIL;
}

///\memberof PNTable
/// "each" method. call block on each member (hash order)
///\param block PNClosure
///\return self PNTable
PN potion_table_each(Potion *P, PN cl, PN self, PN block) {
  vPN(Table) t = (struct PNTable *)potion_fwd(self);
  unsigned k;
  for (k = kh_begin(t); k != kh_end(t); ++k)
    if (kh_exist(PN, t, k)) {
      PN_CLOSURE(block)->method(P, block, P->lobby, kh_key(PN, t, k), kh_val(PN, t, k));
    }
  return self;
}

///\memberof PNTable
/// "put" method. write to the hash
///\param key PN
///\param value PN
///\return self PNTable
PN potion_table_put(Potion *P, PN cl, PN self, PN key, PN value) {
  int ret;
  vPN(Table) t = (struct PNTable *)potion_fwd(self);
  unsigned k = kh_put(PN, t, key, &ret);
  PN_QUICK_FWD(struct PNTable *, t);
  kh_val(PN, t, k) = value;
  PN_TOUCH(self);
  return self;
}

///\memberof PNTable
/// "remove" method. remove key from hash
///\param key PN
///\return self PNTable
PN potion_table_remove(Potion *P, PN cl, PN self, PN key) {
  vPN(Table) t = (struct PNTable *)potion_fwd(self);
  unsigned k = kh_get(PN, t, key);
  if (k != kh_end(t)) kh_del(PN, t, k);
  return self;
}

/// helper function for potion_table_put:"put"
///\param key PN
///\param value PN
///\return self PNTable
PN potion_table_set(Potion *P, PN self, PN key, PN value) {
  self = potion_fwd(self);
  return potion_table_put(P, PN_NIL, potion_table_cast(P, self), key, value);
}

///\memberof PNTable
/// "length" method. count keys
///\return PNNumber
PN potion_table_length(Potion *P, PN cl, PN self) {
  vPN(Table) t = (struct PNTable *)potion_fwd(self);
  return PN_NUM(kh_size(t));
}

// TUPLE - ordered lists (arrays)
// not autovivifying

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

///\memberof PNTuple
/// "append" and "push" method. (to the end)
///\param value PN
///\return PNTuple
PN potion_tuple_append(Potion *P, PN cl, PN self, PN value) {
  return potion_tuple_push(P, self, value);
}

/// Return index of found value or -1
///\param value PN
///\return int
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

///\memberof PNTuple
/// "at" method.
///\param index PNNumber
///\return PNTuple
PN potion_tuple_at(Potion *P, PN cl, PN self, PN index) {
  long i = PN_INT(index), len = PN_TUPLE_LEN(self);
  if (i < 0) i += len;
  if (i >= len) return PN_NIL;
  return PN_TUPLE_AT(self, i);
}

///\memberof PNTuple
/// "clone" method.
///\return new PNTuple
PN potion_tuple_clone(Potion *P, PN cl, PN self) {
  vPN(Tuple) t1 = PN_GET_TUPLE(self);
  NEW_TUPLE(t2, t1->len);
  PN_MEMCPY_N(t2->set, t1->set, PN, t1->len);
  return (PN)t2;
}

///\memberof PNTuple
/// "each" method. call block on each member (linear order)
///\param block PNClosure
///\return self PNTuple
PN potion_tuple_each(Potion *P, PN cl, PN self, PN block) {
  int with_index = potion_sig_arity(P, PN_CLOSURE(block)->sig) >= 2;
  PN_TUPLE_EACH(self, i, v, {
    if (with_index)
      PN_CLOSURE(block)->method(P, block, P->lobby, v, PN_NUM(i));
    else
      PN_CLOSURE(block)->method(P, block, P->lobby, v);
  });
  return self;
}

///\memberof PNTuple
/// "first" method.
///\return first PN or PN_NIL if the PNTuple is empty
PN potion_tuple_first(Potion *P, PN cl, PN self) {
  if (PN_TUPLE_LEN(self) < 1) return PN_NIL;
  return PN_TUPLE_AT(self, 0);
}

///\memberof PNTuple
/// "join" method.
///\param sep PNString
///\return PNString
PN potion_tuple_join(Potion *P, PN cl, PN self, PN sep) {
  PN out = potion_byte_str(P, "");
  PN_TUPLE_EACH(self, i, v, {
    if (i > 0 && sep != PN_NIL) potion_bytes_obj_string(P, out, sep);
    potion_bytes_obj_string(P, out, v);
  });
  return PN_STR_B(out);
}

///\memberof PNTuple
/// "last" method.
///\return last PN or PN_NIL if the PNTuple is empty
PN potion_tuple_last(Potion *P, PN cl, PN self) {
  long len = PN_TUPLE_LEN(self);
  if (len < 1) return PN_NIL;
  return PN_TUPLE_AT(self, len - 1);
}

///\memberof PNTuple
/// "string" method. serializable ascii dump
///\return PNString
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

///\memberof PNTuple
/// "pop" method. remove the last element and return it
///\return last PN
PN potion_tuple_pop(Potion *P, PN cl, PN self, PN key) {
  vPN(Tuple) t = PN_GET_TUPLE(self);
  PN obj = t->set[t->len - 1];
  PN_REALLOC(t, PN_TTUPLE, struct PNTuple, sizeof(PN) * (t->len - 1));
  t->len--;
  PN_TOUCH(self);
  return obj;
}

///\memberof PNTuple
/// "put" method. write value at index key
///\param key PNNumber index
///\param value PN
///\return self PNTuple
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

///\memberof PNTuple
/// "unshift" method. put new element to the front
///\param value PN
///\return PNTuple
PN potion_tuple_unshift(Potion *P, PN cl, PN self, PN value) {
  vPN(Tuple) t = PN_GET_TUPLE(self);
  PN_REALLOC(t, PN_TTUPLE, struct PNTuple, sizeof(PN) * (t->len + 1));
  memmove((void *)&t->set[1], (void *)&t->set[0], sizeof(PN) * t->len);
  t->set[0] = value;
  t->len++;
  PN_TOUCH(self);
  return self;
}

///\memberof PNTuple
/// "shift" method. remove first element and return it
///\param value PN
///\return PNTuple
PN potion_tuple_shift(Potion *P, PN cl, PN self) {
  vPN(Tuple) t = PN_GET_TUPLE(self);
  PN obj = t->set[0];
  memmove((void *)&t->set[0], (void *)&t->set[1], sizeof(PN) * t->len);
  PN_REALLOC(t, PN_TTUPLE, struct PNTuple, sizeof(PN) * (t->len - 1));
  t->len--;
  PN_TOUCH(self);
  return obj;
}

///\memberof PNTuple
/// "print" method. call print on all elements
///\return PN_NIL
PN potion_tuple_print(Potion *P, PN cl, PN self) {
  PN_TUPLE_EACH(self, i, v, {
    potion_send(v, PN_print);
  });
  return PN_NIL;
}

///\memberof PNTuple
/// "length" of a list. Number of elements
///\return PNNumber
PN potion_tuple_length(Potion *P, PN cl, PN self) {
  return PN_NUM(PN_TUPLE_LEN(self));
}

///\memberof PNTuple
/// "reverse" a list non-destructively
///\return a new PNTuple
PN potion_tuple_reverse(Potion *P, PN cl, PN self) {
  unsigned long len = PN_TUPLE_LEN(self);
  PN tuple = potion_tuple_with_size(P, len);
  len--;
  PN_TUPLE_EACH(self, i, x, {
    PN_TUPLE_AT(tuple, len - i) = x;
  });
  return tuple;
}

#define GET(i)    t->set[i]
#define SET(i,v)  t->set[i] = v
#define SWAP(a,b) tmp = GET(a); SET(a, GET(b)); SET(b, tmp)

///\memberof PNTuple
/// "reverse" a list destructively
///\return the same PNTuple with reversed elements
PN potion_tuple_nreverse(Potion *P, PN cl, PN self) {
  struct PNTuple *t = PN_GET_TUPLE(self);
  PN_SIZE len = t->len;
  if (len) {
    PN_SIZE i; PN tmp;
    for (i = 0; i < (PN_SIZE)(len/2); i++) {
      SWAP(len-i-1, i);
    }
  }
  return self;
}

///\memberof PNTuple
/// search for value x in an ordered PNTuple, ordered by PN_UNIQ.
///\param x PN (PNUniq in fact)
///\return found index or -1
PN potion_tuple_bsearch(Potion *P, PN cl, PN self, PN x) {
  struct PNTuple *t = PN_GET_TUPLE(self);
  PNUniq xu = PN_UNIQ(x);
  long i = 0, j = t->len - 1;
  while (i <= j) {
    long m = j + ((i - j) / 2);
    PNUniq u = PN_UNIQ(t->set[m]);
    if (u == xu)
      return PN_NUM(m);
    else if (u > xu)
      j = m - 1;
    else
      i = m + 1;
  }
  return PN_NUM(-1);
}

#if CAN_QSORT
static int potion_qsort_cmp (const void* v1, const void *v2) {
  return potion_send(P, PN_cmp, (PN)v1, (PN)v2);
}
#endif

///\memberof PNTuple
/// sort PNTuple by value
///\param cmp (optional) comparison closure: (a,b) a < b ? -1 : a == b ? 0 : 1.
///\return sorted PNTuple
PN potion_tuple_sort(Potion *P, PN cl, PN self, PN cmp) {
  struct PNTuple *t = PN_GET_TUPLE(self);
  unsigned long i, j, tmp;
  vPN(Closure) c;
  //if (cmp == PN_NIL) cmp = PN_CLOSURE(&potion_any_cmp);
#if CAN_QSORT
  if (t->len < 13) {
#endif
  if (cmp == PN_NIL) {
    for (i = 1; i < t->len; i++) {
      j = i;
      while (j > 0 && PN_UNIQ(t->set[j-1]) > PN_UNIQ(t->set[j])) {
	tmp = t->set[j];
	t->set[j] = t->set[j - 1];
	t->set[j - 1] = tmp;
	j--;
      }
    }
  }
  else {
    c = PN_CLOSURE(cmp);
    for (i = 1; i < t->len; i++) {
      j = i;
      while (j > 0 && c->method(P, cmp, P->lobby, t->set[j-1], t->set[j]) > 0) {
	tmp = t->set[j];
	t->set[j] = t->set[j - 1];
	t->set[j - 1] = tmp;
	j--;
      }
    }
  }
#if CAN_QSORT
  // But need P for the cmp func
  else {
    qsort(t->set, t->len, sizeof(PN), potion_qsort_cmp);
  }
#endif
  return self;
}

#undef SWAP
#undef GET
#undef SET

///\memberof PNLobby
/// global "list" method. return a new empty list
///\param size PNNumber
///\return PNTuple
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
  potion_method(tpl_vt, "reverse", potion_tuple_reverse, 0);
  potion_method(tpl_vt, "nreverse", potion_tuple_nreverse, 0);
  //potion_method(tpl_vt, "remove", potion_tuple_remove, "index=N");
  // TODO: add Tuple remove
  potion_method(tpl_vt, "unshift", potion_tuple_unshift, "value=o");
  potion_method(tpl_vt, "shift", potion_tuple_shift, 0);
  potion_method(tpl_vt, "bsearch", potion_tuple_bsearch, "value=o");
  potion_method(tpl_vt, "sort", potion_tuple_sort, "|block=&");
  potion_method(tpl_vt, "string", potion_tuple_string, 0);
  potion_method(P->lobby, "list", potion_lobby_list, "length=N");
}
