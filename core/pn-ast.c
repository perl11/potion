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
  "code", "value", "assign", "not", "or", "and", "cmp", "eq", "neq",
  "gt", "gte", "lt", "lte", "pipe", "caret", "amp", "bitl",
  "bitr", "plus", "minus", "inc", "times", "div", "rem", "pow",
  "message", "path", "query", "pathq", "expr", "table",
  "block", "data", "proto"
};

const int potion_ast_sizes[] = {
  1, 1, 2, 1, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2,
  3, 1, 1, 1, 1, 1, 1, 1,
  2
};

PN potion_source(Potion *P, u8 p, PNv a, PNv b, PNv c) {
  struct PNSource * volatile t = PN_OBJ_ALLOC(struct PNSource, PN_TSOURCE,
    potion_ast_sizes[p] * sizeof(PN));
  t->part = p;
  t->a[0] = a;
  if (potion_ast_sizes[p] > 1) t->a[1] = b;
  if (potion_ast_sizes[p] > 2) t->a[2] = c;
  return (PN)t;
}

PN potion_source_name(Potion *P, PNv cl, PNv self) {
  struct PNSource * volatile t = (struct PNSource *)self;
  return potion_str(P, potion_ast_names[t->part]);
}

PN potion_source_string(Potion *P, PNv cl, PNv self) {
  int i, n;
  struct PNSource * volatile t = (struct PNSource *)self;
  PNv out = potion_byte_str(P, potion_ast_names[t->part]);
  n = potion_ast_sizes[t->part];
  for (i = 0; i < n; i++) {
    pn_printf(P, out, " ");
    if (i == 0 && n > 1) pn_printf(P, out, "(");
    potion_bytes_obj_string(P, out, t->a[i]);
    if (i == n - 1 && n > 1) pn_printf(P, out, ")");
  }
  return out;
}

void potion_source_init(Potion *P) {
  PNv src_vt = PN_VTABLE(PN_TSOURCE);
  potion_method(src_vt, "compile", potion_source_compile, 0); // in compile.c
  potion_method(src_vt, "name", potion_source_name, 0);
  potion_method(src_vt, "string", potion_source_string, 0);
}
