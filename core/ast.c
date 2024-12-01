///\file ast.c
/// the ast for Potion code in-memory implements PNSource
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include "potion.h"
#include "internal.h"
#include "ast.h"

///\see potion.h: enum PN_AST
const char *potion_ast_names[] = {
  "code", "value", "assign", "not", "or", "and", "cmp", "eq", "neq",
  "gt", "gte", "lt", "lte", "pipe", "caret", "amp", "wavy", "bitl",
  "bitr", "plus", "minus", "inc", "times", "div", "rem", "pow",
  "msg", "path", "query", "pathq", "expr", "list", "block", "lick",
  "proto", "debug"
};

const int potion_ast_sizes[] = {
  1, 3, 2, 1, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 1, 2,
  2, 2, 2, 2, 2, 2, 2, 2,
  3, 1, 1, 1, 1, 1, 1, 3,
  2, 3
};

/// PNSource constructor
///\param p AST type
///\param a,b,c AST operands
///\param lineno linenumber in src file
///\param line associated line PNString in src file
///\returns a new AST node with up to three operands
PN potion_source(Potion *P, u8 p, PN a, PN b, PN c, int lineno, PN line) {
  int size = potion_ast_sizes[p];
  // TODO: potion_ast_sizes[p] * sizeof(PN) (then fix gc_copy)
  vPN(Source) t = PN_ALLOC(PN_TSOURCE, struct PNSource);
  t->part = (enum PN_AST)p;
  t->loc.fileno = P->fileno; // only advanced by load/require
  t->loc.lineno = lineno;
  t->line = line;
#if 1
  switch (size) {
  case 3: t->a[2] = PN_SRC(c);
  case 2: t->a[1] = PN_SRC(b);
  case 1: t->a[0] = PN_SRC(a); break;
  default: potion_fatal("invalid AST type");
  }
#else
  switch (size) {
  case 3: t->a[0] = PN_SRC(a); t->a[1] = PN_SRC(b); t->a[2] = PN_SRC(c); break;
  case 2: t->a[0] = PN_SRC(a); t->a[1] = PN_SRC(b); t->a[2] = 0; break;
  case 1: t->a[0] = PN_SRC(a); t->a[1] = t->a[2] = 0; break;
  default: potion_fatal("invalid AST type");
  }
#endif
  return (PN)t;
}

///\memberof PNSource
/// "size" method
///\returns number of ast trees: 1,2 or 3
static PN potion_source_size(Potion *P, PN cl, PN self) {
  vPN(Source) t = (struct PNSource *)potion_fwd(self);
  return PN_NUM(potion_ast_sizes[t->part]);
}

///\memberof PNSource
/// "name" method
static PN potion_source_name(Potion *P, PN cl, PN self) {
  vPN(Source) t = (struct PNSource *)potion_fwd(self);
  return potion_str(P, potion_ast_names[t->part]);
}

///\memberof PNSource
/// "file" method
///\returns filename of associated AST
static PN potion_source_file(Potion *P, PN cl, PN self) {
  vPN(Source) t = (struct PNSource *)potion_fwd(self);
  return PN_TUPLE_AT(pn_filenames, t->loc.fileno);
}

///\memberof PNSource
/// "lineno" method
///\returns line number of associated AST
static PN potion_source_lineno(Potion *P, PN cl, PN self) {
  vPN(Source) t = (struct PNSource *)potion_fwd(self);
  return PN_NUM(t->loc.lineno);
}

///\memberof PNSource
/// "line" method
///\returns line string of associated AST
static PN potion_source_line(Potion *P, PN cl, PN self) {
  vPN(Source) t = (struct PNSource *)potion_fwd(self);
  return t->line;
}

///\memberof PNSource
/// "string" method of the AST
/// Note: Does not output the AST type, filename nor lineno
static PN potion_source_string(Potion *P, PN cl, PN self) {
  int i, n, cut = 0;
  vPN(Source) t = (struct PNSource *)potion_fwd(self);
  PN out = potion_byte_str(P, potion_ast_names[t->part]);
  int lineno = t->loc.lineno;
  n = potion_ast_sizes[t->part];
  for (i = 0; i < n; i++) {
    pn_printf(P, out, " ");
    if (i == 0 && n > 1) pn_printf(P, out, "(");
    else if (i > 0) {
      if (!t->a[i]) { // omit subsequent nils
	if (!cut) cut = PN_STR_LEN(out);
      }
      else cut = 0;
    }
    if (PN_IS_STR(t->a[i])) {
      pn_printf(P, out, "\"");
      potion_bytes_obj_string(P, out, (PN)t->a[i]);
      pn_printf(P, out, "\"");
    } else {
      potion_bytes_obj_string(P, out, (PN)t->a[i]);
    }
    if ((PN_TYPE(t->a[i]) == PN_TSOURCE) && (t->a[i]->loc.lineno > lineno)) {
      pn_printf(P, out, "\n");
      lineno = t->a[i]->loc.lineno;
    }
    if (i == n - 1 && n > 1) {
      if (cut > 0) {
	vPN(Bytes) b = (struct PNBytes *)potion_fwd(out);
	//DBG_vt("cut at %d, len=%d: \"%s\"\n", cut, b->len, b->chars);
	if (cut < b->len) {
	  b->len = cut - 1;
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

///\memberof PNSource
/// \returns file.c:lineno
static PN potion_source_loc(Potion *P, PN cl, PN self) {
  PN out = potion_byte_str(P, "");
  pn_printf(P, out, "%s:%ld",
            PN_STR_PTR(potion_source_file(P, cl, self)),
            PN_INT(potion_source_lineno(P, cl, self)));
  return PN_STR_B(out);
}

void potion_source_init(Potion *P) {
  PN src_vt = PN_VTABLE(PN_TSOURCE);
  potion_method(src_vt, "name", potion_source_name, 0);
  potion_method(src_vt, "string", potion_source_string, 0);
  potion_method(src_vt, "size", potion_source_size, 0);
  potion_method(src_vt, "file", potion_source_file, 0);
  potion_method(src_vt, "lineno", potion_source_lineno, 0);
  potion_method(src_vt, "line", potion_source_line, 0);
  potion_method(src_vt, "loc",  potion_source_loc, 0);
}
