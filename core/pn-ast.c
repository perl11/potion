//
// pn-ast.c
// the ast for Potion code in-memory
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include "potion.h"
#include "internal.h"
#include "pn-ast.h"

const char *potion_ast_names[] = {
  "code", "value", "assign", "message",
  "path", "query", "expr", "table",
  "block", "data"
};

struct PNSource {
  PN_OBJECT_HEADER
  PNByte part;
  PN a;
};

PN potion_source(Potion *P, PNByte p, PN a) {
  struct PNSource *t = PN_OBJ_ALLOC(struct PNSource, PN_TSOURCE, 0);
  t->part = p;
  t->a = a;
  return (PN)t;
}

PN potion_source_part(Potion *P, PN cl, PN self) {
  struct PNSource *t = (struct PNSource *)self;
  return potion_str(P, potion_ast_names[t->part]);
}

PN potion_source_inspect(Potion *P, PN cl, PN self) {
  struct PNSource *t = (struct PNSource *)self;
  printf("%s ", potion_ast_names[t->part]);
  potion_send(t->a, PN_inspect);
  return PN_NIL;
}

void potion_source_init(Potion *P) {
  PN src_vt = PN_VTABLE(PN_TSOURCE);
  potion_method(src_vt, "part", potion_source_part, 0);
  potion_method(src_vt, "inspect", potion_source_inspect, 0);
}
