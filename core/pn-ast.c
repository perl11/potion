//
// pn-ast.c
// the ast for Potion code in-memory
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include "internal.h"
#include "potion.h"
#include "pn-ast.h"

struct PNSource {
  PN_OBJECT_HEADER
  PNByte part;
};

PN potion_source(Potion *P, PNByte p) {
  struct PNSource *t = PN_OBJ_ALLOC(struct PNSource, PN_TSOURCE, 0);
  t->part = p;
  return (PN)t;
}

PN potion_source_part(Potion *P, PN cl, PN self) {
  struct PNSource *t = (struct PNSource *)self;
  return potion_str(P, "<name>");
}

void potion_source_init(Potion *P) {
  PN src_vt = PN_VTABLE(PN_TSOURCE);
  potion_method(src_vt, "part", potion_source_part, 0);
}
