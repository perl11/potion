#
# syntax.g
# Potion tokens and grammar
#
# (c) 2009 _why
#

%{
#include <stdio.h>
#include <stdlib.h>
#include "potion.h"
#include "internal.h"
#include "pn-ast.h"

int pos = 0;
PN input = PN_NIL;
Potion *P = 0;

#define YY_INPUT(buf, result, max) { \
  if (pos < PN_STR_LEN(input)) { \
    result = max; \
    if (pos + max > PN_STR_LEN(input)) \
      result = (PN_STR_LEN(input) - pos); \
    PN_MEMCPY_N(buf, PN_STR_PTR(input) + pos, char, result + 1); \
    pos += max; \
  } else { \
    result = 0; \
  } \
  printf("YY_INPUT: %x, %d, %d, %d\n", PN_TYPE(input), max, PN_STR_LEN(input), result); \
}

#define YYSTYPE PN

%}

potion = - e:exprs end-of-file { $$ = P->source = PN_AST(CODE, e); }

exprs = e1:expr { $$ = PN_TUP(e1); }
        (sep e2:expr { $$ = PN_PUSH($$, e2); })*

expr = v:value - { $$ = PN_AST(EXPR, PN_TUP(v)); }
     | t:table   { $$ = PN_AST(EXPR, PN_TUP(t)); }

table = table-start table-end { $$ = PN_AST(TABLE, PN_NIL); }

value = i:immed { $$ = PN_AST(VALUE, i); }

immed = nil   { $$ = PN_NIL; }
      | true  { $$ = PN_TRUE; }
      | false { $$ = PN_FALSE; }
      | hex   { $$ = PN_NUM(PN_ATOI(yytext, yyleng, 16)); }
      | dec   { $$ = potion_decimal(P, yytext, yyleng); }
      | int   { $$ = PN_NUM(PN_ATOI(yytext, yyleng, 10)); }
      | str1 | str2

utfw = [A-Za-z0-9_$@{}]
     | '\304' [\250-\277]
     | [\305-\337] [\200-\277]
     | [\340-\357] [\200-\277] [\200-\277]
     | [\360-\364] [\200-\277] [\200-\277] [\200-\277]
utf8 = [\t\n\r -~]
     | [\302-\337] [\200-\277]
     | [\340-\357] [\200-\277] [\200-\277]
     | [\360-\364] [\200-\277] [\200-\277] [\200-\277]
message = < utfw+ >

comma = ',' -
block-start = ':' -
block-end = '.' -
table-start = '(' -
table-end = ')' -
lick-start = '[' -
lick-end = ']' -
query = '?' -
path = < '/' ('/' | utfw)+ >

nil = "nil"
true = "true"
false = "false"
int = < [0-9]+ >
hexl = [0-9A-Fa-f]
hex = '0x' < hexl+ >
dec = < ('0' | [1-9][0-9]*)
      '.' [0-9]+ ('e' [-+] [0-9]+)? >

q1 = [']
c1 = (!q1 utf8)+
str1 = q1 < (q1 q1 | c1)+ >
       q1 { $$ = potion_str2(P, yytext, yyleng); }

escc        = '\\' q2 | '\\' '\\' | '\\' '/'
escn        = '\\' 'n'
escb        = '\\' 'b'
escf        = '\\' 'f'
escr        = '\\' 'r'
esct        = '\\' 't'
escu        = '\\' 'u' hexl hexl hexl hexl

q2 = ["]
e2 = '\\' ["]
c2 = (!q2 utf8)+
str2 = q2 < (e2 | escc | escn | escb | escf | escr | esct | escu | c2)+ >
       q2 { $$ = potion_str2(P, yytext, yyleng); }

- = (space | comment)*
sep = (end-of-line | comma) (space | comment | end-of-line | comma)*
comment	= '#' (!end-of-line utf8)*
space = ' ' | '\f' | '\v' | '\t'
end-of-line = '\r\n' | '\n' | '\r'
end-of-file = !.

%%

PN potion_greg(Potion *PP, PN cl, PN self, PN code) {
  P = PP;
  pos = 0;
  input = code;
  if (!yyparse())
    printf("** Syntax error!\n");
  return P->source;
}
