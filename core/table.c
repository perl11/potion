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

#define NEW_TUPLE(t, size) \
  vPN(Tuple) t = PN_ALLOC_N(PN_TTUPLE, struct PNTuple, size * sizeof(PN)); \
  t->alloc = t->len = size

///\memberof PNTable
/// "string" method
///\return PNString
PN potion_table_string(Potion *P, PN cl, PN self) {
  vPN(Table) t = (vPN(Table))potion_fwd(self);
  DBG_CHECK_TYPE(t,PN_TTABLE);
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
  DBG_CHECK_TYPE(self,PN_TTABLE);
  return self;
}

///\memberof PNTable
/// "at" method
///\param key PN
///\return PN value or PN_NIL
PN potion_table_at(Potion *P, PN cl, PN self, PN key) {
  vPN(Table) t = (vPN(Table))potion_fwd(self);
  DBG_CHECK_TYPE(t,PN_TTABLE);
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
  DBG_CHECK_TYPE(t,PN_TTABLE);
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
  vPN(Table) t = (vPN(Table))potion_fwd(self);
  DBG_CHECK_TYPE(t,PN_TTABLE);
  unsigned k = kh_put(PN, t, key, &ret);
  PN_QUICK_FWD(struct PNTable * volatile, t);
  kh_val(PN, t, k) = value;
  PN_TOUCH(self);
  return self;
}

///\memberof PNTable
/// "remove" method. remove key from hash
///\param key PN
///\return self PNTable
PN potion_table_remove(Potion *P, PN cl, PN self, PN key) {
  vPN(Table) t = (vPN(Table))potion_fwd(self);
  DBG_CHECK_TYPE(t,PN_TTABLE);
  unsigned k = kh_get(PN, t, key);
  if (k != kh_end(t)) kh_del(PN, t, k);
  PN_TOUCH(self);
  return self;
}

/// helper function for potion_table_put:"put", accepts tuple or table
///\param key PN
///\param value PN
///\return self PNTable
PN potion_table_set(Potion *P, PN self, PN key, PN value) {
  self = potion_fwd(self);
  return potion_table_put(P, PN_NIL, potion_table_cast(P, self), key, value);
}

///\memberof PNTable
/// "length" method. count keys
///\return PNInteger
PN potion_table_length(Potion *P, PN cl, PN self) {
  vPN(Table) t = (vPN(Table))potion_fwd(self);
  DBG_CHECK_TYPE(t,PN_TTABLE);
  return PN_NUM(kh_size(t));
}

static
PN potion_table_clone(Potion *P, PN cl, PN self) {
  vPN(Table) t = (vPN(Table))potion_fwd(self);
  DBG_CHECK_TYPE(t,PN_TTABLE);
  vPN(Table) t2 = (vPN(Table))PN_ALLOC_N(PN_TTABLE, struct PNTable, 0);
  unsigned k; int ret;
  t2 = kh_resize_PN(P, t2, kh_size(t));
  for (k = kh_begin(t); k != kh_end(t); ++k)
    if (kh_exist(PN, t, k)) {
      unsigned key = kh_put(PN, t2, kh_key(PN, t, k), &ret);
      PN_QUICK_FWD(struct PNTable *, t);
      PN_QUICK_FWD(struct PNTable *, t2);
      kh_val(PN, t2, key) = kh_val(PN, t, k);
    }
  PN_TOUCH(t2);
  return (PN)t2;
}

/**\memberof PNTable
 Extract slice copy of a table with only the given keys. Not found keys are ignored.
 \code
       t = (0="", 1="", 2="")
       t slice          #=> t
       t slice((1,2,3)) #=> (1="", 2="")
       t slice(())      #=> ()
 \endcode
 \param keys PNTuple.
 \return new PNTable */
static
PN potion_table_slice(Potion *P, PN cl, PN self, PN keys) {
  vPN(Table) t =  (vPN(Table))potion_fwd(self);
  DBG_CHECK_TYPE(t,PN_TTABLE);
  if (!keys)
    return potion_table_clone(P, cl, self);
  else {
    DBG_CHECK_TYPE(keys,PN_TTUPLE);
  }
  vPN(Table) t2 = (vPN(Table))PN_ALLOC_N(PN_TTABLE, struct PNTable, 0);
  t2 = kh_resize_PN(P, t2, PN_TUPLE_LEN(keys)); //ca, could overshoot for dupl and outliers
  PN_TUPLE_EACH(keys, i, v, {
    DBG_vt("%d:%ld ", i, PN_INT(v));
    unsigned k = kh_get(PN, t, v);
    if (k != kh_end(t)) {
      int ret;
      unsigned k2 = kh_put(PN, t2, v, &ret);
      PN_QUICK_FWD(struct PNTable *, t);
      PN_QUICK_FWD(struct PNTable *, t2);
      kh_val(PN, t2, k2) = kh_val(PN, t, k);
    }
  });
  PN_TOUCH(t2);
  return (PN)t2;
}

/**\memberof PNTable
 \code
     (0="", 1="", 2="") keys => (0, 1, 2)
 \endcode
 \return new PNTuple */
static
PN potion_table_keys(Potion *P, PN cl, PN self) {
  vPN(Table) t =  (vPN(Table))potion_fwd(self);
  DBG_CHECK_TYPE(t,PN_TTABLE);
  NEW_TUPLE(t2, kh_size(t));
  int i = 0; unsigned k;
  for (k = kh_begin(t); k != kh_end(t); ++k)
    if (kh_exist(PN, t, k)) {
      PN_TUPLE_AT(t2, i++) = kh_key(PN, t, k);
    }
  PN_TOUCH(t2);
  return (PN)t2;
}

/**\memberof PNTable
 \code
     (0="",1="",2="") keys => ("","","")
 \endcode
 \return new PNTuple */
static
PN potion_table_values(Potion *P, PN cl, PN self) {
  vPN(Table) t =  (vPN(Table))potion_fwd(self);
  DBG_CHECK_TYPE(t,PN_TTABLE);
  NEW_TUPLE(t2, kh_size(t));
  int i = 0; unsigned k;
  for (k = kh_begin(t); k != kh_end(t); ++k)
    if (kh_exist(PN, t, k)) {
      PN_TUPLE_AT(t2, i++) = kh_val(PN, t, k);
    }
  PN_TOUCH(t2);
  return (PN)t2;
}

static
PN potion_table_cmp(Potion *P, PN cl, PN self, PN value) {
  DBG_CHECK_TYPE(self,PN_TTABLE);
  switch (potion_type(value)) {
  case PN_TBOOLEAN: // false < () < true
    return value == PN_FALSE ? PN_NUM(-1) : PN_NUM(1);
  case PN_TNIL:
    return PN_NUM(-1); //nil < () < (...)
  case PN_TTABLE: { // recurse. compare all keys and values
    vPN(Table) t0 = (vPN(Table))potion_fwd(self);
    vPN(Table) t1 = (vPN(Table))potion_fwd(value);
    unsigned k;
    PN cmp = PN_NUM(-1);
    if (kh_size(t0) != kh_size(t1))
      return kh_size(t0) < kh_size(t1) ? PN_NUM(-1) : PN_NUM(1);
    for (k = kh_begin(t0); k != kh_end(t0); ++k) {
      if (kh_exist(PN, t0, k) && kh_exist(PN, t1, k)) {
        cmp = potion_send(kh_val(PN, t0, k), PN_cmp, kh_val(PN, t1, k));
        if (cmp != PN_ZERO)
          return cmp;
      }
    }
    return cmp;
  }
  default:
    potion_fatal("Invalid table cmp type");
    return 0;
  }
}

static
PN potion_table_equal(Potion *P, PN cl, PN self, PN value) {
  if (PN_IS_TUPLE(self)) self = potion_table_cast(P, self);
  DBG_CHECK_TYPE(self,PN_TTABLE);
  return (PN_ZERO == potion_table_cmp(P, cl, self, value)) ? PN_TRUE : PN_FALSE;
}

// TUPLE - ordered lists, i.e. arrays in consecutive memory
// not autovivifying

PN potion_tuple_empty(Potion *P) {
  //NEW_TUPLE(t, 0);
  NEW_TUPLE(t, 3); // prealloc 3 elems
  t->len = 0;
  return (PN)t;
}

PN potion_tuple_with_size(Potion *P, unsigned long size) {
  NEW_TUPLE(t, size);
  return (PN)t;
}

PN potion_tuple_new(Potion *P, PN value) {
  NEW_TUPLE(t, 3); // overallocate by 2 elems
  t->len = 1;
  t->set[0] = value;
  return (PN)t;
}

PN potion_tuple_push(Potion *P, PN tuple, PN value) {
  vPN(Tuple) t = PN_GET_TUPLE(tuple);
  DBG_CHECK_TUPLE(t);
  if (t->len >= t->alloc) {
    PN_REALLOC(t, PN_TTUPLE, struct PNTuple, sizeof(PN) * (t->alloc + 3)); // overalloc by 2
    t->alloc += 3;
  }
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
  DBG_CHECK_TUPLE(self);
  return potion_tuple_push(P, self, value);
}

/// Return index of found value or PN_NONE
///\param tuple PNTuple
///\param value PN
///\return int
PN_SIZE potion_tuple_find(Potion *P, PN tuple, PN value) {
  DBG_CHECK_TUPLE(tuple);
  PN_TUPLE_EACH(tuple, i, v, {
    if (v == value) return i;
  });
  return PN_NONE;
}

///\param tuple PNTuple
///\param value PN
PN_SIZE potion_tuple_push_unless(Potion *P, PN tuple, PN value) {
  DBG_CHECK_TUPLE(tuple);
  PN_SIZE idx = potion_tuple_find(P, tuple, value);
  if (idx != PN_NONE) return idx;

  potion_tuple_push(P, tuple, value);
  return PN_TUPLE_LEN(tuple) - 1;
}

/**\memberof PNTuple
 "at" method, the safe generic tuple accessor. Use the tpl[index] syntax for the fast unchecked version.
 \code
       t=(0,1,2)
       t(0)  #=> 0
       t(1)  #=> 1
       t(-1) #=> 2
 \endcode
 \param index PNInteger. If negative, count from end. If too large, return nil.
 \return tuple element at index */
PN potion_tuple_at(Potion *P, PN cl, PN self, PN index) {
  DBG_CHECK_TUPLE(self);
  long i = PN_INT(index), len = PN_TUPLE_LEN(self);
  if (i < 0) i += len;
  if (i >= len) return PN_NIL;
  return PN_TUPLE_AT(self, i);
}

///\memberof PNTuple
/// "clone" method.
///\return new PNTuple
static
PN potion_tuple_clone(Potion *P, PN cl, PN self) {
  vPN(Tuple) t1 = PN_GET_TUPLE(self);
  DBG_CHECK_TUPLE(t1);
  NEW_TUPLE(t2, t1->len);
  PN_MEMCPY_N(t2->set, t1->set, PN, t1->len);
  t2->alloc = t1->len;
  PN_TOUCH(t2);
  return (PN)t2;
}

/**\memberof PNTuple
 Extract slice copy of a tuple, similar to strings. Supports negative indices and end<start.
 \code
       (0,1,2) slice        #=> (0,1,2)
       (0,1,2) slice(1)     #=> (1,2)
       (0,1,2) slice(0,1)   #=> (0,1)
       (0,1,2) slice(-1)    #=> (2)
       (0,1,2) slice(1,1)   #=> (1)
       (0,1,2) slice(1,-1)  #=> (0,1)
       (0,1,2) slice(2,-1)  #=> (2)
       (0,1,2) slice(-1,-2) #=> (0,1)
 \endcode
 \param start PNInteger. Default 0. If negative, count from end. If too large, return nil.
 \param end   PNInteger. Optional, default last index. If negative, count from end. If too large, return nil.
 \return new PNTuple */
static
PN potion_tuple_slice(Potion *P, PN cl, PN self, PN start, PN end) {
  vPN(Tuple) t1 = PN_GET_TUPLE(self);
  long i, l;
  DBG_CHECK_TUPLE(t1);
  if (!start)
    return potion_tuple_clone(P, cl, self);
  else {
    DBG_CHECK_INT(start);
    i = PN_INT(start);
    if (i < 0) i = t1->len + i;
  }
  if (!end)
    l = t1->len - i;
  else {
    long e = PN_INT(end);
    DBG_CHECK_INT(end);
    if (e < 0) e = t1->len + e;
    l = e - i;
    if (l < 0) { i = e; l = abs(l) + 1; }
    else l++;
    if (l > t1->len) l = t1->len; // permit overshoots
  }
  DBG_vt("; splice(i=%ld,len=%ld)\n", i, l);
  if (!l || i >= t1->len) return PN_NIL;
  NEW_TUPLE(t2, l);
  PN_MEMCPY_N(&t2->set[0], &t1->set[i], PN, l);
  t2->alloc = l;
  PN_TOUCH(t2);
  return (PN)t2;
}


///\memberof PNTuple
/// "each" method. call block on each member (linear order)
///\param block PNClosure
///\return self PNTuple
PN potion_tuple_each(Potion *P, PN cl, PN self, PN block) {
  DBG_CHECK_TUPLE(self);
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
  DBG_CHECK_TUPLE(self);
  if (PN_TUPLE_LEN(self) < 1) return PN_NIL;
  return PN_TUPLE_AT(self, 0);
}

///\memberof PNTuple
/// "join" method.
///\param sep PNString
///\return PNBytes
PN potion_tuple_join(Potion *P, PN cl, PN self, PN sep) {
  DBG_CHECK_TUPLE(self);
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
  DBG_CHECK_TUPLE(self);
  long len = PN_TUPLE_LEN(self);
  if (len < 1) return PN_NIL;
  return PN_TUPLE_AT(self, len - 1);
}

///\memberof PNTuple
/// "string" method. serializable ascii dump
///\return PNString
PN potion_tuple_string(Potion *P, PN cl, PN self) {
  DBG_CHECK_TUPLE(self);
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
PN potion_tuple_pop(Potion *P, PN cl, PN self) {
  vPN(Tuple) t = PN_GET_TUPLE(self);
  DBG_CHECK_TUPLE(t);
  PN obj = t->set[t->len - 1];
  PN_REALLOC(t, PN_TTUPLE, struct PNTuple, sizeof(PN) * (t->len - 1));
  t->len--;
  PN_TOUCH(self);
  return obj;
}

///\memberof PNTuple
/// \c "put" method. write value at index key
/// Note: If the key is not a number converts the tuple to a table
///\param key PNInteger index
///\param value PN
///\return self PNTuple
PN potion_tuple_put(Potion *P, PN cl, PN self, PN key, PN value) {
  if (PN_IS_INT(key)) {
    DBG_CHECK_TUPLE(self);
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
  DBG_CHECK_TUPLE(t);
  if (t->len >= t->alloc) {
    PN_REALLOC(t, PN_TTUPLE, struct PNTuple, sizeof(PN) * (t->alloc + 3)); // overalloc by 2
    t->alloc += 3;
  }
  PN_MEMMOVE_N(&t->set[1], &t->set[0], PN, t->len);
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
  DBG_CHECK_TUPLE(t);
  PN obj = t->set[0];
  PN_MEMMOVE_N(&t->set[0], &t->set[1], PN, t->len);
  PN_REALLOC(t, PN_TTUPLE, struct PNTuple, sizeof(PN) * (t->len - 1));
  t->len--;
  PN_TOUCH(self);
  return obj;
}

///\memberof PNTuple
/// "print" method. call print on all elements
///\return PN_NIL
PN potion_tuple_print(Potion *P, PN cl, PN self) {
  DBG_CHECK_TUPLE(self);
  PN_TUPLE_EACH(self, i, v, {
    potion_send(v, PN_print);
  });
  return PN_STR0;
}

///\memberof PNTuple
/// "length" of a list. Number of elements
///\return PNInteger
PN potion_tuple_length(Potion *P, PN cl, PN self) {
  DBG_CHECK_TYPE(self,PN_TTUPLE);
  return PN_NUM(PN_TUPLE_LEN(self));
}

///\memberof PNTuple
/// "reverse" a list non-destructively
///\return a new PNTuple
PN potion_tuple_reverse(Potion *P, PN cl, PN self) {
  DBG_CHECK_TUPLE(self);
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
//xor swap
#define SWAP(a,b) if (a != b) { \
  t->set[a] ^= GET(b); \
  t->set[b] ^= GET(a); \
  t->set[a] ^= GET(b); }

/**\memberof PNTuple
  "remove" an element at index from tuple.
  \see potion_tuple_delete for the destructive variant
  \return a copy of PNTuple with one less element */
PN potion_tuple_remove(Potion *P, PN cl, PN self, PN index) {
  struct PNTuple *t = PN_GET_TUPLE(self);
  DBG_CHECK_TUPLE(t);
  if (t->len) {
    PN_SIZE i = PN_INT(index);
    PN data = potion_tuple_clone(P, cl, self);
    t = PN_GET_TUPLE(data);
    if (i < t->len)
      PN_MEMMOVE_N(&t->set[i], &t->set[i+1], PN, t->len - i);
    t->len--;
    //((struct PNFwd *)data)->ptr = (PN)t;
    PN_TOUCH(data);
    return data;
  }
  return self;
}

/**\memberof PNTuple
   destructively "delete" an element at index from tuple.
  \see potion_tuple_remove for the copying variant
  \return PNTuple */
PN potion_tuple_delete(Potion *P, PN cl, PN self, PN index) {
  struct PNTuple *t = PN_GET_TUPLE(self);
  DBG_CHECK_TUPLE(t);
  if (t->len) {
    PN_SIZE i = PN_INT(index);
    if (i < t->len)
      PN_MEMMOVE_N(&t->set[i], &t->set[i+1], PN, t->len - i);
    t->len--;
    PN_TOUCH(self);
  }
  return self;
}

///\memberof PNTuple
/// "reverse" a list destructively
///\return the same PNTuple with reversed elements
PN potion_tuple_nreverse(Potion *P, PN cl, PN self) {
  struct PNTuple *t = PN_GET_TUPLE(self);
  DBG_CHECK_TUPLE(t);
  PN_SIZE len = t->len;
  if (len) {
    PN_SIZE i;
    for (i = 0; i < (PN_SIZE)(len/2); i++) {
      SWAP(len-i-1, i);
    }
    PN_TOUCH(self);
  }
  return self;
}

///\memberof PNTuple
/// search for value x in an ordered PNTuple, ordered by PN_UNIQ.
///\param x PN (PNUniq in fact)
///\return found index or false
PN potion_tuple_bsearch(Potion *P, PN cl, PN self, PN x) {
  struct PNTuple *t = PN_GET_TUPLE(self);
  DBG_CHECK_TUPLE(t);
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

/// space-efficient but destructive and not-stable qsort
static
void potion_sort_internal(Potion *P, PN cl, PN self, ///< sort data
	       PN_SIZE from, ///< first index, usually 0
	       PN_SIZE to,   ///< last index, usually len-1
	       PN      cmp)  ///< cmp method for 2 values, returning -1,0,1
{
#ifdef DEBUG
  if (PN_TTUPLE != PN_TYPE(self)) potion_fatal("Invalid type");
#endif
  if (from < to) {
    struct PNTuple *t = PN_GET_TUPLE(self);
    // which pivot? first is worst case if already sorted.
    // random, last, middle or best: median-of-3.
    PN_SIZE i, index = from + (to - from)/2;
    PN pivot = GET(index);
    SWAP(index, to);
    index = from;
    // partition the portion of the tuple between indexes from and to,
    // inclusively, by moving all elements less than pivot before
    // the pivot, and the equal or greater elements after it.
    if (cmp == PN_NIL) { // default: sort by uniq, not value
      for (i=from; i < to-1; i++) { // from ≤ i < to
	if (PN_UNIQ(GET(i)) <= PN_UNIQ(pivot)) { SWAP(i, index); index++; }
      }
    } else if (cmp == PN_TRUE) { // sort by ascending number
      for (i=from; i < to; i++) {
	if (GET(i) <= pivot) {
	  SWAP(i, index); index++;
	}
      }
    } else if (cmp == PN_FALSE) { // sort by descending number
      for (i=from; i < to; i++) {
	if (GET(i) > pivot) { SWAP(i, index); index++;
	}
      }
    } else {
      vPN(Closure) c = PN_CLOSURE(cmp);
      for (i=from; i < to; i++) { // call cmp
	if (PN_INT(c->method(P, cl, cmp, GET(i), pivot)) > 0)
	  { SWAP(i, index); index++; }
      }
    }
    SWAP(index, to); // Move pivot element back to its final place

    if (index > 0)
      potion_sort_internal(P,cl,self, from, index-1, cmp);
    potion_sort_internal(P,cl,self, index+1, to,   cmp);
    PN_TOUCH(self);
  }
}
 
/**\memberof PNTuple
  sort elements, safe non-destructive version.
  generic instable quicksort, via in-place quicksort partition algorithm.

 \param  compare method cmp(a,b) => -1,0,1,
         or NIL for UNIQ (random, but stable),
         or true for ascending or false for descending order by value.
         true or false will fail on most complex data types,
	 works only with num (ints).
 \returns a sorted copy of the tuple */
/// TODO: bitonic __m128 sort (SSE accelerated) for typed tuples
/// E.g. alike https://github.com/zerovm/zerovm-samples/blob/master/bitonic_sort/src/sort_uint_proper_with_args.c
static PN potion_tuple_sort(Potion *P, PN cl,
			    PN self, ///< sort tuple
			    PN cmp)  ///< cmp method for 2 values, returning -1,0,1
{
  PN data = potion_tuple_clone(P, cl, self);
  PN_SIZE len = PN_TUPLE_LEN(self);
  if (cmp != PN_NIL && !PN_IS_BOOL(cmp) && !PN_IS_CLOSURE(cmp))
    potion_fatal("sort: invalid cmp type");
  potion_sort_internal(P, cl, data, 0, len-1, cmp);
  return data;
}
 
/**\memberof PNTuple
   "ins_sort" method, used internally, \warning destructive.
   sort PNTuple by UNIQ, uses insertion_sort for less than 10 elements
   otherwise quicksort.
   \param cmp (optional) comparison closure: (a,b) a < b ? -1 : a == b ? 0 : 1.
          The default sort method is not by value, it is by uniq (random, but stable).
   \return the same, but sorted PNTuple
   \see potion_tuple_sort for the safe variant and cmp variants. */
PN potion_tuple_ins_sort(Potion *P, PN cl, PN self, PN cmp) {
  struct PNTuple *t = PN_GET_TUPLE(self);
  DBG_CHECK_TYPE(t,PN_TTUPLE);
  unsigned long i, j;
  vPN(Closure) c;
  if (t->len < MAX_INS_SORT) {
    // simple insertion sort for smaller arrays (<13)
    if (cmp == PN_NIL) { // default: sort by uniq, not value
      for (i = 1; i < t->len; i++) {
 	j = i;
 	while (j > 0 && PN_UNIQ(GET(j-1)) > PN_UNIQ(GET(j))) {
 	  SWAP(j, j-1);
 	  j--;
 	}
      }
    }
    else if (PN_IS_CLOSURE(cmp)) {
      c = PN_CLOSURE(cmp);
      for (i = 1; i < t->len; i++) {
	j = i;
	while (j > 0 && PN_INT(c->method(P, cl, cmp, GET(j-1), GET(j))) > 0) {
	  SWAP(j, j-1);
	  j--;
	}
      }
    }
    else if (cmp == PN_TRUE) {
      for (i = 1; i < t->len; i++) {
	j = i;
	while (j > 0 && GET(j-1) > GET(j)) {
	  SWAP(j, j-1);
	  j--;
	}
      }
    }
    else if (cmp == PN_FALSE) {
      for (i = 1; i < t->len; i++) {
	j = i;
	while (j > 0 && GET(j-1) < GET(j)) {
	  SWAP(j, j-1);
	  j--;
	}
      }
    }
    else {
      potion_fatal("sort: invalid cmp type");
    }
    PN_TOUCH(self);
  }
  else {
    if (cmp != PN_NIL && !PN_IS_BOOL(cmp) && !PN_IS_CLOSURE(cmp))
      potion_fatal("sort: invalid cmp type");
    potion_sort_internal(P, cl, self, 0, t->len-1, cmp);
  }
  return self;
}

static
PN potion_tuple_cmp(Potion *P, PN cl, PN self, PN value) {
  DBG_CHECK_TYPE(self,PN_TTUPLE);
  switch (potion_type(value)) {
  case PN_TBOOLEAN: // false < () < true
    return value == PN_FALSE ? PN_NUM(-1) : PN_NUM(1);
  case PN_TNIL:
    return PN_NUM(-1); //nil < () < (...)
  case PN_TTUPLE: // recurse
    if(PN_TUPLE_LEN(self) && PN_TUPLE_LEN(value)) {
      PN cmp;
      if ((cmp = potion_send(potion_tuple_first(P,cl,self), PN_cmp,
			     potion_tuple_first(P,cl,value)))
	  == PN_ZERO)
	{
	  PN t1 = potion_tuple_clone(P,cl,self);
	  PN t2 = potion_tuple_clone(P,cl,value);
	  potion_tuple_pop(P,cl,t1);
	  potion_tuple_pop(P,cl,t2);
	  return potion_send(t1, PN_cmp, t2);
	}
      else {
	return cmp;
      }
    }
    else {
      if (PN_TUPLE_LEN(value)) return PN_NUM(-1);
      else if (PN_TUPLE_LEN(self)) return PN_NUM(1);
      else return PN_ZERO;
    }
  default:
    potion_fatal("Invalid tuple cmp type");
    return 0;
  }
}

static
PN potion_tuple_equal(Potion *P, PN cl, PN self, PN value) {
  //if (PN_IS_TABLE(self)) self = potion_tuple_cast(P, self);
  DBG_CHECK_TYPE(self,PN_TTUPLE);
  return (PN_ZERO == potion_tuple_cmp(P, cl, self, value)) ? PN_TRUE : PN_FALSE;
}

#undef SWAP
#undef GET
#undef SET

///\memberof Lobby
/// global "list" method. return a new empty list
///\param size PNInteger
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
  potion_method(tbl_vt, "clone", potion_table_clone, 0);
  potion_method(tbl_vt, "slice", potion_table_slice, "|keys=u");
  potion_method(tbl_vt, "keys", potion_table_keys, 0);
  potion_method(tbl_vt, "values", potion_table_values, 0);
  potion_method(tbl_vt, "equal", potion_table_equal, "value=o");

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
  potion_method(tpl_vt, "remove", potion_tuple_remove, "index=N");
  potion_method(tpl_vt, "delete", potion_tuple_delete, "index=N");
  potion_method(tpl_vt, "slice", potion_tuple_slice, "start:=0,end:=nil");
  potion_method(tpl_vt, "unshift", potion_tuple_unshift, "value=o");
  potion_method(tpl_vt, "shift", potion_tuple_shift, 0);
  potion_method(tpl_vt, "bsearch", potion_tuple_bsearch, "value=o");
  potion_method(tpl_vt, "sort", potion_tuple_sort, "|block=&");
  potion_method(tpl_vt, "ins_sort", potion_tuple_ins_sort, "|block=&");
  potion_method(tpl_vt, "cmp", potion_tuple_cmp, "value=o");
  potion_method(tbl_vt, "equal", potion_tuple_equal, "value=o");
  potion_method(tpl_vt, "string", potion_tuple_string, 0);
  potion_method(P->lobby, "list", potion_lobby_list, "length=N");
}
