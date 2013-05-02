///\file ast.c
/// the ast for Potion code in-memory implements PNSource
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include "p2.h"
#include "internal.h"
#include "ast.h"

const char *potion_ast_names[] = {
  "code", "value", "assign", "not", "or", "and", "cmp", "eq", "neq",
  "gt", "gte", "lt", "lte", "pipe", "caret", "amp", "wavy", "bitl",
  "bitr", "plus", "minus", "inc", "times", "div", "rem", "pow",
  "msg", "path", "query", "pathq", "expr", "list",
  "block", "lick", "proto"
};

const int potion_ast_sizes[] = {
  1, 3, 2, 1, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 1, 2,
  2, 2, 2, 2, 2, 2, 2, 2,
  3, 1, 1, 1, 1, 1, 1, 3,
  2
};

PN potion_source(Potion *P, u8 p, PN a, PN b, PN c) {
  vPN(Source) t = PN_ALLOC_N(PN_TSOURCE, struct PNSource, 0 * sizeof(PN));
  // t->a[0] = t->a[1] = t->a[2] = (vPN(Source))0;
  // TODO: potion_ast_sizes[p] * sizeof(PN) (then fix gc_copy)

  t->part = p;
  t->a[0] = (vPN(Source))a;
  if (potion_ast_sizes[p] > 1) t->a[1] = (vPN(Source))b;
  if (potion_ast_sizes[p] > 2) t->a[2] = (vPN(Source))c;
  return (PN)t;
}

///\memberof PNSource
/// "name" method
PN potion_source_name(Potion *P, PN cl, PN self) {
  vPN(Source) t = (struct PNSource *)self;
  return potion_str(P, potion_ast_names[t->part]);
}

///\memberof PNSource
/// "string" method
PN potion_source_string(Potion *P, PN cl, PN self) {
  int i, n, cut = 0;
  vPN(Source) t = (struct PNSource *)self;
  PN out = potion_byte_str(P, potion_ast_names[t->part]);
  n = potion_ast_sizes[t->part];
  for (i = 0; i < n; i++) {
    pn_printf(P, out, " ");
    if (i == 0 && n > 1) pn_printf(P, out, "(");
    else if (i > 0) {
      if (t->a[i] == PN_NIL) { // omit subsequent nil
	if (!cut) cut = PN_STR_LEN(out);
      }
      else cut = 0;
    }
    potion_bytes_obj_string(P, out, (PN)t->a[i]);
    if (i == n - 1 && n > 1) {
      if (cut > 0) {
	vPN(Bytes) b = (struct PNBytes *)potion_fwd(out);
	//DBG_vt("cut at %d, len=%d: \"%s\"\n", cut, b->len, b->chars);
	if (cut < b->len) {
	  b->len = cut;
	  if (b->chars[cut-1] == ' ')
	    b->chars[cut-1] = '\0';
	  else
	    b->chars[cut] = '\0';
	}
      }
      pn_printf(P, out, ")");
    }
  }
  return PN_STR_B(out);
}

void potion_source_init(Potion *P) {
  PN src_vt = PN_VTABLE(PN_TSOURCE);
  potion_method(src_vt, "compile", potion_source_compile, 0); // in compile.c
  potion_method(src_vt, "name", potion_source_name, 0);
  potion_method(src_vt, "string", potion_source_string, 0);
}
