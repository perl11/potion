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

unsigned int potion_strhash(Potion *P, const char *str, size_t len) {
  unsigned int h = (unsigned int)len;
  size_t step = (len >> 5) + 1; /* limit amount of string to hash */
  size_t l1;
  for (l1 = len; l1 >= step; l1 -= step)
    h = h ^ ((h << 5) + (h >> 2) + (unsigned char)str[l1 - 1]);
  // TODO: check GC (see src/string.c in lua)
  return h;
}

PN potion_str(Potion *P, const char *str) {
  size_t len = strlen(str);
  struct PNString *s = PN_ALLOC2(struct PNString, len);
  s->vt = PN_TSTRING;
  s->len = (unsigned int)len;
  s->hash = potion_strhash(P, str, len);
  PN_MEMCPY_N(s->chars, str, char, len);
  return (PN)s;
}
