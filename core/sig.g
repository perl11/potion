#
# sig.g
# Method signature syntax (from C)
#
# (c) 2009 _why
#

%{
#include <stdio.h>
#include <stdlib.h>
#include "potion.h"
#include "internal.h"

static int pos = 0;
static char *input = NULL;
static PNAsm * volatile sbuf;
static Potion *P = 0;

#define YY_INPUT(buf, result, max) { \
  if (pos < strlen(input)) { \
    result = max; \
    if (pos + max > strlen(input)) \
      result = (strlen(input) - pos); \
    PN_MEMCPY_N(buf, input + pos, char, result + 1); \
    pos += max; \
  } else { \
    result = 0; \
  } \
}

#define YYSTYPE PN
#define YYPARSE potion_sig_yyparse
#define YYPARSEFROM potion_sig_yyparse_from

%}

sig = args+ end-of-file
args = arg-list (sep arg-list)*
arg-list = arg-set (optional arg-set)?
         | optional arg-set
arg-set = arg (comma arg)*

utfw = [A-Za-z0-9_$@;`{}]
     | '\304' [\250-\277]
     | [\305-\337] [\200-\277]
     | [\340-\357] [\200-\277] [\200-\277]
     | [\360-\364] [\200-\277] [\200-\277] [\200-\277]
name = < utfw+ > - { $$ = potion_str2(P, yytext, yyleng); }
type = < ('s' | 'S' | 'n' | 'N' | 'b' | 'B' | 'k' | 't' | 'o' | 'O' | '-' | '&') > -
       { $$ = PN_NUM(yytext[0]); }
arg = n:name eq t:type { P->source = PN_PUSH(PN_PUSH(PN_PUSH(P->source, n), t), PN_NIL); }
    | t:type           { P->source = PN_PUSH(P->source, t); }
optional = '|' -       { P->source = PN_PUSH(P->source, PN_NUM('|')); }
eq = '=' -
comma = ',' -
sep = '.' -            { P->source = PN_PUSH(P->source, PN_NUM('.')); }

- = space*
space = ' ' | '\f' | '\v' | '\t' | '\r' | '\n'
end-of-file = !.

%%

PN potion_sig(Potion *PP, char *fmt) {
  if (fmt == NULL) return PN_NIL; // no signature, arg check off
  if (fmt[0] == '\0') return PN_FALSE; // empty signature, no args

  P = PP;
  pos = 0;
  input = fmt;
  P->source = PN_TUP0();
  yypos = yylimit = 0;
  if (!YYPARSE())
    printf("** Syntax error!\n");
  return P->source;
}

int potion_sig_find(Potion *P, PN cl, PN name)
{
  PN_SIZE idx = 0;
  PN sig;
  if (!PN_IS_CLOSURE(cl))
    cl = potion_obj_get_call(P, cl);

  if (!PN_IS_CLOSURE(cl))
    return -1;

  sig = PN_CLOSURE(cl)->sig;

  if (!PN_IS_TUPLE(sig))
    return -1;

  PN_TUPLE_EACH(sig, i, v, {
    if (v == PN_NUM(idx) || v == name)
      return idx;
    if (PN_IS_NUM(v))
      idx++;
  });

  return -1;
}
