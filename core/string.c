///\file string.c
/// internals of utf-8 and byte strings
///\see PNString class members
///\see PNBytes class members
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "potion.h"
#include "internal.h"
#include "khash.h"
#include "table.h"

#define BYTES_FACTOR 1 / 8 * 9
#define BYTES_CHUNK  32
#define BYTES_ALIGN(len) PN_ALIGN(len + sizeof(struct PNBytes), BYTES_CHUNK) - sizeof(struct PNBytes)

void potion_add_str(Potion *P, PN s) {
  int ret;
  kh_put(str, P->strings, s, &ret);
  PN_QUICK_FWD(struct PNTable *, P->strings);
}

PN potion_lookup_str(Potion *P, const char *str) {
  vPN(Table) t = P->strings;
  unsigned k = kh_get(str, t, str);
  if (k != kh_end(t)) return kh_key(str, t, k);
  return PN_NIL;
}

PN potion_str(Potion *P, const char *str) {
  PN val = potion_lookup_str(P, str);
  if (val == PN_NIL) {
    size_t len = strlen(str);
    vPN(String) s = PN_ALLOC_N(PN_TSTRING, struct PNString, len + 1);
    s->len = (PN_SIZE)len;
    PN_MEMCPY_N(s->chars, str, char, len);
    s->chars[len] = '\0';
    potion_add_str(P, (PN)s);
    val = (PN)s;
  }
  return val;
}

PN potion_str2(Potion *P, char *str, size_t len) {
  PN exist = PN_NIL;

  vPN(String) s = PN_ALLOC_N(PN_TSTRING, struct PNString, len + 1);
  s->len = (PN_SIZE)len;
  assert(len < 0x10000000);
  PN_MEMCPY_N(s->chars, str, char, len);
  s->chars[len] = '\0';

  exist = potion_lookup_str(P, s->chars);
  if (exist == PN_NIL) {
    potion_add_str(P, (PN)s);
    exist = (PN)s;
  }
  return exist;
}

PN potion_strcat(Potion *P, char *str, char *str2) {
  PN exist = PN_NIL;
  int len = strlen(str);
  int len2 = strlen(str2);
  vPN(String) s = PN_ALLOC_N(PN_TSTRING, struct PNString, len+len2+1);
  PN_MEMCPY_N(s->chars, str,  char, len);
  PN_MEMCPY_N(s->chars+len, str2, char, len2);
  s->chars[len+len2] = '\0';
  s->len = len+len2;
  exist = potion_lookup_str(P, s->chars);
  if (exist == PN_NIL) {
    potion_add_str(P, (PN)s);
    exist = (PN)s;
  }
  return exist;
}

PN potion_str_format(Potion *P, const char *format, ...) {
  vPN(String) s;
  PN_SIZE len;
  va_list args;

  va_start(args, format);
  len = (PN_SIZE)vsnprintf(NULL, 0, format, args);
  va_end(args);
  s = PN_ALLOC_N(PN_TSTRING, struct PNString, len + 1);

  va_start(args, format);
  vsnprintf(s->chars, len + 1, format, args);
  va_end(args);
  s->len = len;
  return (PN)s;
}

///\memberof PNString
/// "length" method. number of chars
static PN potion_str_length(Potion *P, PN cl, PN self) {
  return PN_NUM(potion_cp_strlen_utf8(PN_STR_PTR(self)));
}

///\memberof PNString
/// "eval" a string.
static PN potion_str_eval(Potion *P, PN cl, PN self) {
  return potion_eval(P, self);
}

///\memberof PNString
/// "number" method. as atoi/atof
static PN potion_str_number(Potion *P, PN cl, PN self) {
  char *str = PN_STR_PTR(self);
  int i = 0, dec = 0, sign = 0, len = PN_STR_LEN(self);
  if (len < 1) return PN_ZERO;

  sign = (str[0] == '-' ? -1 : 1);
  if (str[0] == '-' || str[0] == '+') {
    dec++; str++; len--;
  }
  for (i = 0; i < len; i++)
    if (str[i] < '0' || str[i] > '9')
      break;
  if (i < 10 && i == len) {
    return PN_NUM(sign * PN_ATOI(str, i, 10));
  }

  return potion_strtod(P, PN_STR_PTR(self), PN_STR_LEN(self));
}

///\memberof PNString
/// "string" method. Returns self
static PN potion_str_string(Potion *P, PN cl, PN self) {
  return self;
}

///\memberof PNString
/// "clone" method. Returns self, strings are immutable.
static PN potion_str_clone(Potion *P, PN cl, PN self) {
  return self;
}

///\memberof PNString
/// "print" method. fwrite to stdout
///\returns nil
static PN potion_str_print(Potion *P, PN cl, PN self) {
  if (fwrite(PN_STR_PTR(self), 1, PN_STR_LEN(self), stdout))
    return PN_STR0;
  else
    return PN_NIL;
}

/// returns byte position of the index-th utf8 char
// Maybe use the optimized strlen in contrib.c
static size_t potion_utf8char_offset(const char *s, size_t index) {
  int i;
  for (i = 0; s[i]; i++)
    if ((s[i] & 0xC0) != 0x80)
      if (index-- == 0)
        return i;
  return i;
}

/// returns byte position of next utf8 char after s[offset]
static size_t potion_utf8char_nextchar(const char *s, size_t offset) {
  size_t i;
  for (i = offset+1; s[i]; i++)
    if ((s[i] & 0xC0) != 0x80)
      return i;
  return i;
}

/* By Bjoern Hoehrmann
  from http://lists.w3.org/Archives/Public/www-archive/2009Apr/0001.html

  The first 128 entries are tuples of 4 bit values. The lower bits
  are a mask that when xor'd with a byte removes the leading utf-8
  bits. The upper bits are a character class number. The remaining
  160 entries are a minimal deterministic finite automaton. It has
  10 states and each state has 13 character class transitions, and
  3 unused transitions for padding reasons. When the automaton en-
  ters state zero, it has found a complete valid utf-8 code point;
  if it enters state one then the input sequence is not utf-8. The
  start state is state nine. Note the mixture of octal and double
  for stylistic reasons.
  The state is ignored in this code,  since slice already ensures
  that the utf8 codepoint is not malformed. */
static const uint8_t utf8d[] = {
  070,070,070,070,070,070,070,070,070,070,070,070,070,070,070,070,
  050,050,050,050,050,050,050,050,050,050,050,050,050,050,050,050,
  030,030,030,030,030,030,030,030,030,030,030,030,030,030,030,030,
  030,030,030,030,030,030,030,030,030,030,030,030,030,030,030,030,
  204,204,188,188,188,188,188,188,188,188,188,188,188,188,188,188,
  188,188,188,188,188,188,188,188,188,188,188,188,188,188,188,188,
  174,158,158,158,158,158,158,158,158,158,158,158,158,142,126,126,
  111, 95, 95, 95, 79,207,207,207,207,207,207,207,207,207,207,207,

  0,1,1,1,8,7,6,4,5,4,3,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,2,2,2,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,1,1,1,1,1,1,1,1,1,1,1,1,
  1,4,4,1,1,1,1,1,1,1,1,1,1,1,1,1,1,4,4,4,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,4,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,8,7,6,4,5,4,3,2,1,1,1,1,
};

/// decode the utf8 codepoint at s, i.e. ord
static unsigned long potion_utf8char_decode(const char *s) {
  unsigned char data, byte;
  unsigned long unic = 0;
  while ((byte = *s++)) {
    if (byte >= 0x80) {
      data = utf8d[ byte - 0x80 ];
      byte = (byte ^ (uint8_t)(data << 4));
    }
    unic = (unic << 6) | byte;
  }
  return unic;
}

/// helper function for potion_str_slice to fix index.
inline static PN potion_str_slice_index(PN index, size_t len, int nilvalue) {
  int i = PN_INT(index);
  int corrected;
  if (PN_IS_NIL(index)) {
    corrected = nilvalue;
  } else if (i < 0) {
    corrected = i + len;
    if (corrected < 0) {
      corrected = 0;
    }
  } else if (i > len) {
    corrected = len;
  } else {
    corrected = i;
  }
  return PN_NUM(corrected);
}

///\memberof PNString
/// "slice" method. supports negative indices, and end<start
///\param start PNInteger
///\param end   PNInteger
///\return PNString substring
static PN potion_str_slice(Potion *P, PN cl, PN self, PN start, PN end) {
  char *str = PN_STR_PTR(self);
  size_t len = potion_cp_strlen_utf8(str);
  size_t endoffset;
  if (!start)
    return self;
  else {
    DBG_CHECK_TYPE(start, PN_TNUMBER);
  }
  size_t startoffset = potion_utf8char_offset(str,
                         PN_INT(potion_str_slice_index(start, len, 0)));
  if (!end)
    end = PN_NUM(len);
  else {
    DBG_CHECK_INT(end);
  }
  if (end < start) {
    endoffset = potion_utf8char_offset(str,
                  PN_INT(potion_str_slice_index(start+end, len, len)));
  } else {
    endoffset = potion_utf8char_offset(str,
                  PN_INT(potion_str_slice_index(end, len, len)));
  }
  return potion_str2(P, str + startoffset, endoffset - startoffset);
}

///\memberof PNString
/// "bytes" method. Convert PNString to PNBytes
static PN potion_str_bytes(Potion *P, PN cl, PN self) {
  return potion_byte_str2(P, PN_STR_PTR(self), PN_STR_LEN(self));
}

///\memberof PNString
/// "+" method.
///\param x PNString
///\return concat PNString
PN potion_str_add(Potion *P, PN cl, PN self, PN x) {
  char *s = malloc(PN_STR_LEN(self) + PN_STR_LEN(x));
  PN str;
  if (s == NULL) potion_allocation_error();
  PN_MEMCPY_N(s, PN_STR_PTR(self), char, PN_STR_LEN(self));
  PN_MEMCPY_N(s + PN_STR_LEN(self), PN_STR_PTR(x), char, PN_STR_LEN(x));
  str = potion_str2(P, s, PN_STR_LEN(self) + PN_STR_LEN(x));
  free(s);
  return str;
}

///\memberof PNString
/// default function type_call_is for PNString, returning the character at
/// the given position.
///\param index PNInteger
///\return PNString substring index .. index+1
static PN potion_str_at(Potion *P, PN cl, PN self, PN index) {
  size_t startoffset, endoffset;
  ssize_t start;
  char *str  = PN_STR_PTR(potion_fwd(self));
  DBG_CHECK_TYPE(index, PN_TNUMBER);
  start = PN_INT(index);
  if (start < 0) {
    size_t len = potion_cp_strlen_utf8(str);
    start = PN_INT(potion_str_slice_index(index, len, 0)); // supports s(-1)
  }
  startoffset = potion_utf8char_offset(str, start);
  endoffset = potion_utf8char_nextchar(str, startoffset);
  return potion_str2(P, str + startoffset, endoffset - startoffset);
}

///\memberof PNString
///\memberof PNBytes
/// "ord" method for PNString and PNBytes. return nil on strings longer than 1 char
///\param index int (optional, default: 0)
///\return PNInteger
static PN potion_str_ord(Potion *P, PN cl, PN self, PN index) {
  const char *str = PN_STR_PTR(potion_fwd(self));
  if (PN_STR_LEN(self) > 255) goto slow;
  else {
    long len = potion_cp_strlen_utf8(str);
    if (len == PN_STR_LEN(self) && PN_INT(index) < len) {
      return PN_NUM(str[PN_INT(index)]);
    }
    else {
    slow:
      return PN_NUM(potion_utf8char_decode(
               &str[potion_utf8char_offset(str, PN_INT(index))]));
    }
  }
}

PN potion_byte_str(Potion *P, const char *str) {
  return potion_byte_str2(P, str, strlen(str));
}

PN potion_byte_str2(Potion *P, const char *str, size_t len) {
  vPN(Bytes) s = (struct PNBytes *)potion_bytes(P, len);
  PN_MEMCPY_N(s->chars, str, char, len);
  s->chars[len] = '\0';
  return (PN)s;
}

PN potion_bytes(Potion *P, size_t len) {
  size_t siz = BYTES_ALIGN(len + 1);
  vPN(Bytes) s = PN_ALLOC_N(PN_TBYTES, struct PNBytes, siz);
  s->siz = (PN_SIZE)siz;
  s->len = (PN_SIZE)len;
  return (PN)s;
}

///\memberof PNBytes
/// "clone" returns a copy of the byte buffer
///\return PNBytes
PN potion_bytes_clone(Potion *P, PN cl, PN self) {
  vPN(Bytes) b = (struct PNBytes *)potion_fwd(self);
  vPN(Bytes) s = PN_ALLOC_N(PN_TBYTES, struct PNBytes, b->siz);
  s->siz = b->siz;
  s->len = b->len;
  return (PN)s;
}

PN_SIZE pn_printf(Potion *P, PN bytes, const char *format, ...) {
  PN_SIZE len;
  va_list args;
  vPN(Bytes) s = (struct PNBytes *)potion_fwd(bytes);

  va_start(args, format);
  len = (PN_SIZE)vsnprintf(NULL, 0, format, args);
  va_end(args);

  if (s->len + len + 1 > s->siz) {
    size_t siz = BYTES_ALIGN(((s->len + len) * BYTES_FACTOR) + 1);
    PN_REALLOC(s, PN_TBYTES, struct PNBytes, siz);
    s->siz = (PN_SIZE)siz;
  }

  va_start(args, format);
  vsnprintf(s->chars + s->len, len + 1, format, args);
  va_end(args);

  s->len += len;
  return len;
}

void potion_bytes_obj_string(Potion *P, PN bytes, PN obj) {
  potion_bytes_append(P, 0, bytes, obj ? potion_send(obj, PN_string) : PN_STR("nil"));
}

///\memberof PNBytes
/// "append" method.
///\param str PNBytes or PNString
///\return PNBytes
PN potion_bytes_append(Potion *P, PN cl, PN self, PN str) {
  vPN(Bytes) s = (struct PNBytes *)potion_fwd(self);
  PN fstr = potion_fwd(str);
  PN_SIZE len = PN_STR_LEN(fstr);

  if (s->len + len + 1 > s->siz) {
    size_t siz = BYTES_ALIGN(((s->len + len) * BYTES_FACTOR) + 1);
    PN_REALLOC(s, PN_TBYTES, struct PNBytes, siz);
    s->siz = (PN_SIZE)siz;
  }

  PN_MEMCPY_N(s->chars + s->len, PN_STR_PTR(fstr), char, len);
  s->len += len;
  s->chars[s->len] = '\0';
  return self;
}

///\memberof PNBytes
/// "length" method. Number of bytes, not chars.
///\return PNInteger
static PN potion_bytes_length(Potion *P, PN cl, PN self) {
  PN str = potion_fwd(self);
  return PN_NUM(PN_STR_LEN(str));
}

///\memberof PNBytes
/// "string" method.
// TODO: ensure it's UTF-8 data
PN potion_bytes_string(Potion *P, PN cl, PN self) {
  PN exist = potion_lookup_str(P, PN_STR_PTR(self = potion_fwd(self)));
  if (exist == PN_NIL) {
    PN_SIZE len = PN_STR_LEN(self);
    vPN(String) s = PN_ALLOC_N(PN_TSTRING, struct PNString, len + 1);
    s->len = len;
    PN_MEMCPY_N(s->chars, PN_STR_PTR(self), char, len + 1);
    potion_add_str(P, (PN)s);
    exist = (PN)s;
  }
  return exist;
}

///\memberof PNBytes
/// "print" method.
static PN potion_bytes_print(Potion *P, PN cl, PN self) {
  self = potion_fwd(self);
  if (fwrite(PN_STR_PTR(self), 1, PN_STR_LEN(self), stdout))
    return PN_STR0;
  else
    return PN_NIL;
}

///\memberof PNBytes
/// "each" method. call block on all bytes
///\param block=&
///\return PN_NIL
static PN potion_bytes_each(Potion *P, PN cl, PN self, PN block) {
  self = potion_fwd(self);
  char *s = PN_STR_PTR(self);
  int i;
  for (i = 0; i < PN_STR_LEN(self); i++) {
    PN_CLOSURE(block)->method(P, block, P->lobby, potion_byte_str2(P, &s[i], 1));
  }
  return PN_NIL;
}

///\memberof PNBytes
/// type_call_is() for PNBytes. (?)
///\param index PNInteger
///\return PNString substring index .. index+1
static PN potion_bytes_at(Potion *P, PN cl, PN self, PN index) {
  char c;
  self = potion_fwd(self);
  index = PN_INT(index);
  if (index >= PN_STR_LEN(self) || (signed long)index < 0)
    return PN_NIL;
  c = PN_STR_PTR(self)[index];
  return potion_byte_str2(P, &c, 1);
}

/**\memberof PNString
  "cmp" a string to argument str, casted to a string
   \code "a" cmp "b" #=> -1 \endcode
   \code "a" cmp "a" #=>  0 \endcode
   \code "z" cmp "a" #=>  1 \endcode
 \param str PN string compared to
 \return PNInteger (positive, negative or 0)
 \sa potion_tuple_sort. */
static PN potion_str_cmp(Potion *P, PN cl, PN self, PN str) {
  if (PN_IS_STR(str)) {
    return strcmp(PN_STR_PTR(self), PN_STR_PTR(str));
  } else {
    return strcmp(PN_STR_PTR(self), PN_STR_PTR(potion_send(PN_string, str)));
  }
}

void potion_str_hash_init(Potion *P) {
  P->strings = PN_CALLOC_N(PN_TSTRINGS, struct PNTable, 0);
}

void potion_str_init(Potion *P) {
  PN str_vt = PN_VTABLE(PN_TSTRING);
  PN byt_vt = PN_VTABLE(PN_TBYTES);
  potion_type_call_is(str_vt, PN_FUNC(potion_str_at, 0));
  potion_method(str_vt, "eval", potion_str_eval, 0);
  potion_method(str_vt, "length", potion_str_length, 0);
  potion_method(str_vt, "number", potion_str_number, 0);
  potion_method(str_vt, "print", potion_str_print, 0);
  potion_method(str_vt, "string", potion_str_string, 0);
  potion_method(str_vt, "clone", potion_str_clone, 0);
  potion_method(str_vt, "slice", potion_str_slice, "start=N|end=N");
  potion_method(str_vt, "bytes", potion_str_bytes, 0);
  potion_method(str_vt, "+", potion_str_add, "str=S");
  potion_method(str_vt, "ord", potion_str_ord, "|index=N");
  potion_method(str_vt, "cmp", potion_str_cmp, "str=o");
  
  potion_type_call_is(byt_vt, PN_FUNC(potion_bytes_at, 0));
  potion_method(byt_vt, "append", potion_bytes_append, "str=S");
  potion_method(byt_vt, "length", potion_bytes_length, 0);
  potion_method(byt_vt, "print", potion_bytes_print, 0);
  potion_method(byt_vt, "string", potion_bytes_string, 0);
  potion_method(byt_vt, "clone", potion_bytes_clone, 0);
  potion_method(byt_vt, "ord", potion_str_ord, 0);
  potion_method(byt_vt, "each", potion_bytes_each, "block=&");
}
