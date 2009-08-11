//
// string.c
// internals of utf-8 and byte strings
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
  PN_MEMCPY_N(s->chars, str, char, len);
  s->chars[len] = '\0';

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

  return (PN)s;
}

static PN potion_str_length(Potion *P, PN closure, PN self) {
  return PN_NUM(potion_cp_strlen_utf8(PN_STR_PTR(self)));
}

static PN potion_str_eval(Potion *P, PN closure, PN self) {
  return potion_eval(P, self);
}

static PN potion_str_number(Potion *P, PN closure, PN self) {
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

  return potion_decimal(P, PN_STR_PTR(self), PN_STR_LEN(self));
}

static PN potion_str_string(Potion *P, PN closure, PN self) {
  return self;
}

static PN potion_str_print(Potion *P, PN closure, PN self) {
  printf("%s", PN_STR_PTR(self));
  return PN_NIL;
}

static size_t potion_utf8char_offset(const char *s, size_t index) {
  int i;
  for (i = 0; s[i]; i++)
    if ((s[i] & 0xC0) != 0x80)
      if (index-- == 0)
        return i;
  return i;
}

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

static PN potion_str_slice(Potion *P, PN closure, PN self, PN start, PN end) {
  char *str = PN_STR_PTR(self);
  size_t len = potion_cp_strlen_utf8(str);
  size_t startoffset = potion_utf8char_offset(str, PN_INT(potion_str_slice_index(start, len, 0)));
  size_t endoffset = potion_utf8char_offset(str, PN_INT(potion_str_slice_index(end, len, len)));
  return potion_str2(P, str + startoffset, endoffset - startoffset);
}

static PN potion_str_at(Potion *P, PN closure, PN self, PN index) {
  return potion_str_slice(P, closure, self, index, PN_NUM(PN_INT(index) + 1));
}

PN potion_byte_str(Potion *P, const char *str) {
  size_t len = strlen(str);
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
  potion_bytes_append(P, 0, bytes, potion_send(obj, PN_string));
}

PN potion_bytes_append(Potion *P, PN closure, PN self, PN str) {
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

static PN potion_bytes_length(Potion *P, PN closure, PN self) {
  PN str = potion_fwd(self);
  return PN_NUM(PN_STR_LEN(str));
}

// TODO: ensure it's UTF-8 data
PN potion_bytes_string(Potion *P, PN closure, PN self) {
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

static PN potion_bytes_print(Potion *P, PN closure, PN self) {
  PN str = potion_fwd(self);
  printf("%s", PN_STR_PTR(str));
  return PN_NIL;
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
  potion_method(str_vt, "slice", potion_str_slice, "start=N,end=N");
  potion_method(byt_vt, "append", potion_bytes_append, 0);
  potion_method(byt_vt, "length", potion_bytes_length, 0);
  potion_method(byt_vt, "print", potion_bytes_print, 0);
  potion_method(byt_vt, "string", potion_bytes_string, 0);
}
