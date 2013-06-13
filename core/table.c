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
#include "p2.h"
#include "internal.h"
#include "khash.h"
#include "table.h"

///\memberof PNTable
/// "string" method
///\return PNString
PN potion_table_string(Potion *P, PN cl, PN self) {
  vPN(Table) t = (struct PNTable *)potion_fwd(self);
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
  vPN(Table) t = (struct PNTable *)potion_fwd(self);
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
  vPN(Table) t = (struct PNTable *)potion_fwd(self);
  DBG_CHECK_TYPE(t,PN_TTABLE);
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
  DBG_CHECK_TYPE(t,PN_TTABLE);
  unsigned k = kh_get(PN, t, key);
  if (k != kh_end(t)) kh_del(PN, t, k);
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
///\return PNNumber
PN potion_table_length(Potion *P, PN cl, PN self) {
  vPN(Table) t = (struct PNTable *)potion_fwd(self);
  DBG_CHECK_TYPE(t,PN_TTABLE);
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
  DBG_CHECK_TYPE(t,PN_TTUPLE);
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
  DBG_CHECK_TYPE(self,PN_TTUPLE);
  return potion_tuple_push(P, self, value);
}

/// Return index of found value or -1
///\param tuple PNTuple
///\param value PN
///\return int
PN_SIZE potion_tuple_find(Potion *P, PN tuple, PN value) {
  DBG_CHECK_TYPE(tuple,PN_TTUPLE);
  PN_TUPLE_EACH(tuple, i, v, {
    if (v == value) return i;
  });
  return -1;
}

///\param tuple PNTuple
///\param value PN
PN_SIZE potion_tuple_push_unless(Potion *P, PN tuple, PN value) {
  DBG_CHECK_TYPE(tuple,PN_TTUPLE);
  PN_SIZE idx = potion_tuple_find(P, tuple, value);
  if (idx != -1) return idx;

  potion_tuple_push(P, tuple, value);
  return PN_TUPLE_LEN(tuple) - 1;
}

/**\memberof PNTuple
 "at" method, the generic tuple accessor.
 \code
       t=(0,1,2)
       t(0)  #=> 0
       t(1)  #=> 1
       t(-1) #=> 2
 \endcode
 \param index PNNumber. If negative, count from end. If too large, return nil.
 \return tuple element at index */
PN potion_tuple_at(Potion *P, PN cl, PN self, PN index) {
  DBG_CHECK_TYPE(self,PN_TTUPLE);
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
  DBG_CHECK_TYPE(t1,PN_TTUPLE);
  NEW_TUPLE(t2, t1->len);
  PN_MEMCPY_N(t2->set, t1->set, PN, t1->len);
  return (PN)t2;
}

///\memberof PNTuple
/// "each" method. call block on each member (linear order)
///\param block PNClosure
///\return self PNTuple
PN potion_tuple_each(Potion *P, PN cl, PN self, PN block) {
  DBG_CHECK_TYPE(self,PN_TTUPLE);
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
  DBG_CHECK_TYPE(self,PN_TTUPLE);
  if (PN_TUPLE_LEN(self) < 1) return PN_NIL;
  return PN_TUPLE_AT(self, 0);
}

///\memberof PNTuple
/// "join" method.
///\param sep PNString
///\return PNString
PN potion_tuple_join(Potion *P, PN cl, PN self, PN sep) {
  DBG_CHECK_TYPE(self,PN_TTUPLE);
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
  DBG_CHECK_TYPE(self,PN_TTUPLE);
  long len = PN_TUPLE_LEN(self);
  if (len < 1) return PN_NIL;
  return PN_TUPLE_AT(self, len - 1);
}

///\memberof PNTuple
/// "string" method. serializable ascii dump
///\return PNString
PN potion_tuple_string(Potion *P, PN cl, PN self) {
  DBG_CHECK_TYPE(self,PN_TTUPLE);
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
  DBG_CHECK_TYPE(t,PN_TTUPLE);
  PN obj = t->set[t->len - 1];
  PN_REALLOC(t, PN_TTUPLE, struct PNTuple, sizeof(PN) * (t->len - 1));
  t->len--;
  PN_TOUCH(self);
  return obj;
}

///\memberof PNTuple
/// \c "put" method. write value at index key
/// Note: If the key is not a number converts the tuple to a table
///\param key PNNumber index
///\param value PN
///\return self PNTuple
PN potion_tuple_put(Potion *P, PN cl, PN self, PN key, PN value) {
  if (PN_IS_NUM(key)) {
    DBG_CHECK_TYPE(self,PN_TTUPLE);
    long i = PN_INT(key), len = PN_TUPLE_LEN(self);
    if (i < 0) i += len;
    if (i < len) {
      PN_TUPLE_AT(self, i) = value;
      PN_TOUCH(self);
      return self;
    } else if (i == len)
      return potion_tuple_push(P, self, value);
  }
#ifdef P2
  return potion_type_error(P, key);
#endif
  return potion_table_put(P, PN_NIL, potion_table_cast(P, self), key, value);
}

///\memberof PNTuple
/// "unshift" method. put new element to the front
///\param value PN
///\return PNTuple
PN potion_tuple_unshift(Potion *P, PN cl, PN self, PN value) {
  vPN(Tuple) t = PN_GET_TUPLE(self);
  DBG_CHECK_TYPE(t,PN_TTUPLE);
  PN_REALLOC(t, PN_TTUPLE, struct PNTuple, sizeof(PN) * (t->len + 1));
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
  DBG_CHECK_TYPE(t,PN_TTUPLE);
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
  DBG_CHECK_TYPE(self,PN_TTUPLE);
  PN_TUPLE_EACH(self, i, v, {
    potion_send(v, PN_print);
  });
  return PN_NIL;
}

///\memberof PNTuple
/// "length" of a list. Number of elements
///\return PNNumber
PN potion_tuple_length(Potion *P, PN cl, PN self) {
  DBG_CHECK_TYPE(self,PN_TTUPLE);
  return PN_NUM(PN_TUPLE_LEN(self));
}

///\memberof PNTuple
/// "reverse" a list non-destructively
///\return a new PNTuple
PN potion_tuple_reverse(Potion *P, PN cl, PN self) {
  DBG_CHECK_TYPE(self,PN_TTUPLE);
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
  DBG_CHECK_TYPE(t,PN_TTUPLE);
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
  DBG_CHECK_TYPE(t,PN_TTUPLE);
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
  DBG_CHECK_TYPE(t,PN_TTUPLE);
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
///\return found index or -1
PN potion_tuple_bsearch(Potion *P, PN cl, PN self, PN x) {
  struct PNTuple *t = PN_GET_TUPLE(self);
  DBG_CHECK_TYPE(t,PN_TTUPLE);
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
static void potion_sort_internal(Potion *P, PN cl, PN self, ///< sort data
	       PN_SIZE from, ///< first index, usually 0
	       PN_SIZE to,   ///< last index, usually len-1
	       PN      cmp)  ///< cmp method for 2 values, returning -1,0,1
{
  DBG_CHECK_TYPE(self,PN_TTUPLE);
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
 \returns a sorted copy of the tuple
*/
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

static PN potion_tuple_cmp(Potion *P, PN cl, PN self, PN value) {
  DBG_CHECK_TYPE(self,PN_TTUPLE);
  switch (potion_type(value)) {
  case PN_TBOOLEAN: // false < () < true
    return value == PN_FALSE ? -1 : 1;
  case PN_TNIL:
    return -1; //nil < () < (...)
  case PN_TTUPLE: // recurse
    if(PN_TUPLE_LEN(self) && PN_TUPLE_LEN(value)) {
      PN cmp;
      if ((cmp = potion_send(potion_tuple_first(P,cl,self), PN_cmp,
			     potion_tuple_first(P,cl,value)))
	  == PN_NUM(0))
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
      if (PN_TUPLE_LEN(value)) return -1;
      else if (PN_TUPLE_LEN(self)) return 1;
      else return 0;
    }
  default:
    potion_fatal("Invalid tuple cmp type");
  }
}

#undef SWAP
#undef GET
#undef SET

///\memberof Lobby
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
  potion_method(tpl_vt, "remove", potion_tuple_remove, "index=N");
  potion_method(tpl_vt, "delete", potion_tuple_delete, "index=N");
  //potion_method(tpl_vt, "slice", potion_tuple_slice, "from=N|to=N");
  potion_method(tpl_vt, "unshift", potion_tuple_unshift, "value=o");
  potion_method(tpl_vt, "shift", potion_tuple_shift, 0);
  potion_method(tpl_vt, "bsearch", potion_tuple_bsearch, "value=o");
  potion_method(tpl_vt, "sort", potion_tuple_sort, "|block=&");
  potion_method(tpl_vt, "ins_sort", potion_tuple_ins_sort, "|block=&");
  potion_method(tpl_vt, "cmp", potion_tuple_cmp, "value=o");
  potion_method(tpl_vt, "string", potion_tuple_string, 0);
  potion_method(P->lobby, "list", potion_lobby_list, "length=N");
}
