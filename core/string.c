//
// string.c
// internals of utf-8 and byte strings
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "internal.h"
#include "potion.h"
#include "khash.h"

KHASH_MAP_INIT_STR(str, PN);

struct PNStrTable {
  PN_OBJECT_HEADER
  kh_str_t kh;
};

void potion_add_str(PN self, const char *str, PN id) {
  int ret;
  struct PNStrTable *t = (struct PNStrTable *)self;
  unsigned k = kh_put(str, &t->kh, str, &ret);
  if (!ret) kh_del(str, &t->kh, k);
  kh_value(&t->kh, k) = id;
}

PN potion_lookup_str(PN self, const char *str) {
  struct PNStrTable *t = (struct PNStrTable *)self;
  unsigned k = kh_get(str, &t->kh, str);
  if (k != kh_end(&t->kh)) return kh_value(&t->kh, k);
  return PN_NIL;
}

PN potion_str(Potion *P, const char *str) {
  PN id = potion_lookup_str(P->strings, str);
  if (!id) {
    size_t len = strlen(str);
    struct PNString *s = PN_BOOT_OBJ_ALLOC(struct PNString, PN_TSTRING, len + 1);
    s->len = (unsigned int)len;
    PN_MEMCPY_N(s->chars, str, char, len);
    s->chars[len] = '\0';
    id = (PN)s;

    potion_add_str(P->strings, s->chars, id);
  }
  return id;
}

PN potion_str2(Potion *P, char *str, size_t len) {
  PN s;
  char c = str[len];
  str[len] = '\0';
  s = potion_str(P, str);
  str[len] = c;
  return s;
}

PN potion_bytes(Potion *P, size_t len) {
  struct PNString *s = PN_OBJ_ALLOC(struct PNString, PN_TBYTES, len + 1);
  s->len = (unsigned int)len;
  s->chars[len] = '\0';
  return (PN)s;
}

static PN potion_str_length(Potion *P, PN closure, PN self) {
  return PN_NUM(PN_STR_LEN(self));
}

static PN potion_str_inspect(Potion *P, PN closure, PN self) {
  printf("%s", PN_STR_PTR(self));
  return PN_NIL;
}

static PN potion_bytes_inspect(Potion *P, PN closure, PN self) {
  printf("#<bytes>");
  return PN_NIL;
}

void potion_str_hash_init(Potion *P) {
  struct PNStrTable *t = PN_BOOT_OBJ_ALLOC(struct PNStrTable, PN_TTABLE, 0);
  P->strings = (PN)t;
}

void potion_str_init(Potion *P) {
  PN str_vt = PN_VTABLE(PN_TSTRING);
  PN byt_vt = PN_VTABLE(PN_TBYTES);
  potion_method(str_vt, "inspect", potion_str_inspect, 0);
  potion_method(str_vt, "length", potion_str_length, 0);
  potion_method(byt_vt, "inspect", potion_bytes_inspect, 0);
  potion_method(byt_vt, "length", potion_str_length, 0);
}
