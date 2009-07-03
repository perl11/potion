//
// khash.h
// a hashtable library, modified to suit potion's gc
//
// Copyright (c) 2008, by Attractive Chaos <attractivechaos@aol.co.uk>
// (Released under the MIT license, see COPYING)
//

#ifndef __AC_KHASH_H
#define __AC_KHASH_H

#define AC_VERSION_KHASH_H "0.2.2"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef uint32_t khint_t;
typedef khint_t khiter_t;

#define __ac_HASH_PRIME_SIZE 32
static const uint32_t __ac_prime_list[__ac_HASH_PRIME_SIZE] =
{
  0ul,          3ul,          11ul,         23ul,         53ul,
  97ul,         193ul,        389ul,        769ul,        1543ul,
  3079ul,       6151ul,       12289ul,      24593ul,      49157ul,
  98317ul,      196613ul,     393241ul,     786433ul,     1572869ul,
  3145739ul,    6291469ul,    12582917ul,   25165843ul,   50331653ul,
  100663319ul,  201326611ul,  402653189ul,  805306457ul,  1610612741ul,
  3221225473ul, 4294967291ul
};

#define KHASH_FLAG(name, h, i) *(uint32_t *)(h->table + kh_flag_##name(i))
#define KHASH_KEY(name, h, i) *kh_key_##name(h, i)
#define KHASH_VAL(name, h, i) *kh_val_##name(h, i)

#define __ac_isempty(flag, i) ((flag[i>>4]>>((i&0xfU)<<1))&2)
#define __ac_set_isempty_false(flag, i) (flag[i>>4]&=~(2ul<<((i&0xfU)<<1)))
#define __kh_isempty(name, h, i) ((KHASH_FLAG(name, h, i)>>((i&0xfU)<<1))&2)
#define __kh_isdel(name, h, i) ((KHASH_FLAG(name, h, i)>>((i&0xfU)<<1))&1)
#define __kh_iseither(name, h, i) ((KHASH_FLAG(name, h, i)>>((i&0xfU)<<1))&3)
#define __kh_set_isdel_false(name, h, i) (KHASH_FLAG(name, h, i)&=~(1ul<<((i&0xfU)<<1)))
#define __kh_set_isempty_false(name, h, i) (KHASH_FLAG(name, h, i)&=~(2ul<<((i&0xfU)<<1)))
#define __kh_set_isboth_false(name, h, i) (KHASH_FLAG(name, h, i)&=~(3ul<<((i&0xfU)<<1)))
#define __kh_set_isdel_true(name, h, i) (KHASH_FLAG(name, h, i)|=1ul<<((i&0xfU)<<1))

static const double __kh_HASH_UPPER = 0.77;

#define PN_TABLE_HEADER PN_SIZE n_buckets, size, n_occupied, upper_bound;

#define KHASH_INIT(name, kh_t, khkey_t, khval_t, khkey_t2, kh_is_map, __hash_func, __hash_equal, __hash_func2, __hash_equal2) \
  static inline khint_t kh_pair_##name() { return (kh_is_map ? (sizeof(khkey_t) + sizeof(khval_t)) : sizeof(khkey_t)); } \
  static inline khint_t kh_size_##name(khint_t n) { return (((n >> 4) + 1) * (sizeof(uint32_t))) + (n * kh_pair_##name()); } \
  static inline khint_t kh_flag_##name(khint_t i) { return (i >> 4) * (sizeof(uint32_t) + ((sizeof(uint32_t) * 4) * kh_pair_##name())); } \
  static inline khint_t kh_key_at_##name(khint_t i) { return (((i >> 4) + 1) * sizeof(uint32_t)) + (i * kh_pair_##name()); } \
  static inline khint_t kh_val_at_##name(khint_t i) { return (((i >> 4) + 1) * sizeof(uint32_t)) + (i * kh_pair_##name()) + sizeof(khkey_t); } \
  static inline khkey_t *kh_key_##name(kh_t *h, khint_t i) { return (khkey_t *)(h->table + kh_key_at_##name(i)); } \
  static inline khval_t *kh_val_##name(kh_t *h, khint_t i) { return (khval_t *)(h->table + kh_val_at_##name(i)); } \
  static inline void kh_clear_##name(Potion *P, kh_t *h) \
  { \
    if (h && h->n_buckets > 0) { \
      int i = ((h->n_buckets) >> 4) + 1; \
      while (i-- > 0) KHASH_FLAG(name, h, i) = 0xaaaaaaaa; \
      h->size = h->n_occupied = 0; \
    } \
  } \
  static inline khint_t kh_get_##name(Potion *P, kh_t *h, khkey_t2 key) \
  { \
    if (h->n_buckets) { \
      khint_t inc, k, i, last; \
      k = __hash_func2(key); i = k % h->n_buckets; \
      inc = 1 + k % (h->n_buckets - 1); last = i; \
      while (!__kh_isempty(name, h, i) && (__kh_isdel(name, h, i) || !__hash_equal2(KHASH_KEY(name, h, i), key))) { \
        if (i + inc >= h->n_buckets) i = i + inc - h->n_buckets; \
        else i += inc; \
        if (i == last) return h->n_buckets; \
      } \
      return __kh_iseither(name, h, i)? h->n_buckets : i; \
    } else return 0; \
  } \
  static inline kh_t *kh_resize_##name(Potion *P, kh_t *h, khint_t new_n_buckets) \
  { \
    vPN(Data) nft = 0; \
    uint32_t *new_flags = 0; \
    khint_t j = 1; \
    { \
      khint_t t = __ac_HASH_PRIME_SIZE - 1; \
      while (__ac_prime_list[t] > new_n_buckets) --t; \
      new_n_buckets = __ac_prime_list[t+1]; \
      if (h->size >= (khint_t)(new_n_buckets * __kh_HASH_UPPER + 0.5)) j = 0; \
      else { \
        nft = PN_DALLOC_N(uint32_t, (new_n_buckets >> 4) + 1); \
        if (h->n_buckets < new_n_buckets) \
          PN_REALLOC(h, PN_TUSER, kh_t, kh_size_##name(new_n_buckets)); \
        new_flags = (uint32_t *)nft->data; \
        memset(new_flags, 0xaa, ((new_n_buckets>>4) + 1) * sizeof(uint32_t)); \
      } \
    } \
    if (j) { \
      for (j = 0; j != h->n_buckets; ++j) { \
        if (__kh_iseither(name, h, j) == 0) { \
          khkey_t key = KHASH_KEY(name, h, j); \
          khval_t val; \
          if (kh_is_map) val = KHASH_VAL(name, h, j); \
          __kh_set_isdel_true(name, h, j); \
          while (1) { \
            khint_t inc, k, i; \
            k = __hash_func(key); \
            i = k % new_n_buckets; \
            inc = 1 + k % (new_n_buckets - 1); \
            while (!__ac_isempty(new_flags, i)) { \
              if (i + inc >= new_n_buckets) i = i + inc - new_n_buckets; \
              else i += inc; \
            } \
            __ac_set_isempty_false(new_flags, i); \
            if (i < h->n_buckets && __kh_iseither(name, h, i) == 0) { \
              { khkey_t tmp = KHASH_KEY(name, h, i); KHASH_KEY(name, h, i) = key; key = tmp; } \
              if (kh_is_map) { khval_t tmp = KHASH_VAL(name, h, i); KHASH_VAL(name, h, i) = val; val = tmp; } \
              __kh_set_isdel_true(name, h, i); \
            } else { \
              KHASH_KEY(name, h, i) = key; \
              if (kh_is_map) KHASH_VAL(name, h, i) = val; \
              break; \
            } \
          } \
        } \
      } \
      if (h->n_buckets > new_n_buckets) \
        PN_REALLOC(h, PN_TUSER, kh_t, kh_size_##name(new_n_buckets)); \
      j = (new_n_buckets >> 4) + 1; \
      new_flags = (uint32_t *)nft->data; \
      while (j-- > 0) KHASH_FLAG(name, h, j << 4) = new_flags[j]; \
      h->n_buckets = new_n_buckets; \
      h->n_occupied = h->size; \
      h->upper_bound = (khint_t)(h->n_buckets * __kh_HASH_UPPER + 0.5); \
    } \
    return h; \
  } \
  static inline khint_t kh_put_##name(Potion *P, kh_t *h, khkey_t key, int *ret) \
  { \
    khint_t x; \
    if (h->n_occupied >= h->upper_bound) { \
      if (h->n_buckets > (h->size<<1)) h = kh_resize_##name(P, h, h->n_buckets - 1); \
      else h = kh_resize_##name(P, h, h->n_buckets + 1); \
    } \
    { \
      khint_t inc, k, i, site, last; \
      x = site = h->n_buckets; k = __hash_func(key); i = k % h->n_buckets; \
      if (__kh_isempty(name, h, i)) x = i; \
      else { \
        inc = 1 + k % (h->n_buckets - 1); last = i; \
        while (!__kh_isempty(name, h, i) && (__kh_isdel(name, h, i) || !__hash_equal(KHASH_KEY(name, h, i), key))) { \
          if (__kh_isdel(name, h, i)) site = i; \
          if (i + inc >= h->n_buckets) i = i + inc - h->n_buckets; \
          else i += inc; \
          if (i == last) { x = site; break; } \
        } \
        if (x == h->n_buckets) { \
          if (__kh_isempty(name, h, i) && site != h->n_buckets) x = site; \
          else x = i; \
        } \
      } \
    } \
    if (__kh_isempty(name, h, x)) { \
      KHASH_KEY(name, h, x) = key; \
      __kh_set_isboth_false(name, h, x); \
      ++h->size; ++h->n_occupied; \
      *ret = 1; \
    } else if (__kh_isdel(name, h, x)) { \
      KHASH_KEY(name, h, x) = key; \
      __kh_set_isboth_false(name, h, x); \
      ++h->size; \
      *ret = 2; \
    } else *ret = 0; \
    return x; \
  } \
  static inline void kh_del_##name(Potion *P, kh_t *h, khint_t x) \
  { \
    if (x != h->n_buckets && !__kh_iseither(name, h, x)) { \
      __kh_set_isdel_true(name, h, x); \
      --h->size; \
    } \
  }

/* --- BEGIN OF HASH FUNCTIONS --- */

#define kh_int_hash_func(key) (uint32_t)(key)
#define kh_int_hash_equal(a, b) (a == b)
#define kh_int64_hash_func(key) (uint32_t)((key)>>33^(key)^(key)<<11)
#define kh_int64_hash_equal(a, b) (a == b)
static inline khint_t __kh_X31_hash_string(const char *s)
{
  khint_t h = *s;
  if (h) for (++s ; *s; ++s) h = (h << 5) - h + *s;
  return h;
}
static inline khint_t __luaS_hash_string(const char *s)
{
  size_t len = strlen(s);
  khint_t h = len;
  size_t step = (len >> 5) + 1; /* limit amount of string to hash */
  size_t l1;
  for (l1 = len; l1 >= step; l1 -= step)
    h = h ^ ((h << 5) + (h >> 2) + (unsigned char)s[l1 - 1]);
  return h;
}
#define kh_pnstr_hash_func(key) __luaS_hash_string(PN_STR_PTR(key))
#define kh_pnstr_hash_equal(a, b) (strcmp(PN_STR_PTR(a), PN_STR_PTR(b)) == 0)
#define kh_str_hash_func(key) __luaS_hash_string(key)
#define kh_str_hash_equal(a, b) (strcmp(PN_STR_PTR(a), b) == 0)
#define kh_pn_hash_func(key) (uint32_t)PN_UNIQ(key)
#define kh_pn_hash_equal(a, b) (a == b)

/* --- END OF HASH FUNCTIONS --- */

/* Other necessary macros... */

#define kh_clear(name, h) kh_clear_##name(P, h)
#define kh_resize(name, h, s) h = kh_resize_##name(P, h, s)
#define kh_put(name, h, k, r) kh_put_##name(P, h, k, r)
#define kh_get(name, h, k) kh_get_##name(P, h, k)
#define kh_del(name, h, k) kh_del_##name(P, h, k)

#define kh_exist(name, h, x) (!__kh_iseither(name, (h), (x)))
#define kh_key(name, h, x) (KHASH_KEY(name, h, x))
#define kh_val(name, h, x) (KHASH_VAL(name, h, x))
#define kh_begin(h) (khint_t)(0)
#define kh_end(h) ((h)->n_buckets)
#define kh_size(h) ((h)->size)
#define kh_n_buckets(h) ((h)->n_buckets)
#define kh_mem(name, h) (((struct PNTable *)h)->n_buckets == 0 ? 0 : kh_size_##name(((struct PNTable *)h)->n_buckets))

#define KHASH_MAP_INIT_STR(name, t) \
  KHASH_INIT(name, t, _PN, PNUniq, const char *, 0, kh_pnstr_hash_func, \
    kh_pnstr_hash_equal, kh_str_hash_func, kh_str_hash_equal)

#define KHASH_MAP_INIT_PN(name, t) \
  KHASH_INIT(name, t, _PN, _PN, _PN, 1, kh_pn_hash_func, \
    kh_pn_hash_equal, kh_pn_hash_func, kh_pn_hash_equal)

#endif /* __AC_KHASH_H */
