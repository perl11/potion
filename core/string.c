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

void potion_add_str(PN self, unsigned k, PN id) {
  struct PNStrTable *t = (struct PNStrTable *)self;
  kh_value(&t->kh, k) = id;
}

unsigned potion_lookup_str(PN self, const char *str, PN *id) {
  int ret;
  struct PNStrTable *t = (struct PNStrTable *)self;
  unsigned k = kh_put(str, &t->kh, str, &ret);
  if (!ret) *id = kh_value(&t->kh, k);
  return k;
}

PN potion_str(Potion *P, const char *str) {
  PN id = PN_NIL;
  unsigned k = potion_lookup_str(P->strings, str, &id);
  if (!id) {
    size_t len = strlen(str);
    struct PNString *s = PN_BOOT_OBJ_ALLOC(struct PNString, PN_TSTRING, len + 1);
    s->len = (unsigned int)len;
    PN_MEMCPY_N(s->chars, str, char, len);
    s->chars[len] = '\0';
    id = (PN)s;

    potion_add_str(P->strings, k, id);
  }
  return id;
}

static PN potion_str_length(Potion *P, PN closure, PN self) {
  return PN_NUM(PN_STR_LEN(self));
}

void potion_str_hash_init(Potion *P) {
  struct PNStrTable *t = PN_BOOT_OBJ_ALLOC(struct PNStrTable, PN_TTABLE, 0);
  P->strings = (PN)t;
}

void potion_str_init(Potion *P) {
  PN str_vt = PN_VTABLE(PN_TSTRING);
  potion_method(str_vt, "length", potion_str_length, 0);
}
