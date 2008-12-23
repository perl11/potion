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
  PN id = potion_lookup_str(P->strings, str);
  if (!id)
  {
    size_t len = strlen(str);
    struct PNString *s = (struct PNString *)
      potion_allocate(P, 0, PN_VTABLE(PN_TSTRING),
        PN_NUM((sizeof(struct PNString)-sizeof(struct PNObject))+len+1));
    s->len = (unsigned int)len;
    s->hash = potion_strhash(P, str, len);
    PN_MEMCPY_N(s->chars, str, char, len);
    s->chars[len] = '\0';
    id = (PN)s;

    potion_def_method(P, 0, P->strings, id, PN_NIL);
  }
  return id;
}

static PN potion_str_length(Potion *P, PN closure, PN self)
{
  return PN_NUM(PN_STR_LEN(self));
}

void potion_str_init(Potion *P)
{
  PN str_vt = PN_VTABLE(PN_TSTRING);
  potion_method(str_vt, "length", potion_str_length, 0);
}
