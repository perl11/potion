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
  "path", "query", "pathq", "expr",
  "table", "block", "data", "proto"
};

const int potion_ast_sizes[] = {
  1, 1, 2, 3,
  1, 1, 1, 1,
  1, 1, 1, 2
};

PN potion_source(Potion *P, PNByte p, PN a, PN b, PN c) {
  struct PNSource *t = PN_OBJ_ALLOC(struct PNSource, PN_TSOURCE,
    potion_ast_sizes[p] * sizeof(PN));
  t->part = p;
  t->a[0] = a;
  if (potion_ast_sizes[p] > 1) t->a[1] = b;
  if (potion_ast_sizes[p] > 2) t->a[2] = c;
  return (PN)t;
}

PN potion_source_name(Potion *P, PN cl, PN self) {
  struct PNSource *t = (struct PNSource *)self;
  return potion_str(P, potion_ast_names[t->part]);
}

PN potion_source_inspect(Potion *P, PN cl, PN self) {
  int i, n;
  struct PNSource *t = (struct PNSource *)self;
  printf("%s", potion_ast_names[t->part]);
  n = potion_ast_sizes[t->part];
  for (i = 0; i < n; i++) {
    printf(" ");
    if (i == 0 && n > 1) printf("(");
    potion_send(t->a[i], PN_inspect);
    if (i == n - 1 && n > 1) printf(")");
  }
  return PN_NIL;
}

void potion_source_init(Potion *P) {
  PN src_vt = PN_VTABLE(PN_TSOURCE);
  potion_method(src_vt, "name", potion_source_name, 0);
  potion_method(src_vt, "inspect", potion_source_inspect, 0);
  potion_method(src_vt, "compile", potion_source_compile, 0); // in compile.c
}
