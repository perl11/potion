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
#include "asm.h"
#include "pn-ast.h"

int pos = 0;
PN input = PN_NIL;
PNAsm * volatile sbuf;
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
}

#define YYSTYPE PN

%}

potion = -- s:statements end-of-file { $$ = P->source = PN_AST(CODE, s); }

statements = s1:stmt { $$ = s1 = PN_TUP(s1); }
        (sep s2:stmt { $$ = s1 = PN_PUSH(s1, s2); })*
         sep?
     | ''            { $$ = PN_NIL; }

stmt = s:sets
       ( or x:sets          { s = PN_OP(AST_OR, s, x); }
       | and x:sets         { s = PN_OP(AST_AND, s, x); })*
       { $$ = s; }

sets = e:eqs
       ( assign x:eqs       { e = PN_AST2(ASSIGN, e, x); }
       | or assign x:eqs    { e = PN_AST2(ASSIGN, e, PN_OP(AST_OR, e, x)); }
       | and assign x:eqs   { e = PN_AST2(ASSIGN, e, PN_OP(AST_OR, e, x)); }
       | pipe assign x:eqs  { e = PN_AST2(ASSIGN, e, PN_OP(AST_PIPE, e, x)); }
       | caret assign x:eqs { e = PN_AST2(ASSIGN, e, PN_OP(AST_CARET, e, x)); }
       | amp assign x:eqs   { e = PN_AST2(ASSIGN, e, PN_OP(AST_AMP, e, x)); }
       | bitl assign x:eqs  { e = PN_AST2(ASSIGN, e, PN_OP(AST_BITL, e, x)); }
       | bitr assign x:eqs  { e = PN_AST2(ASSIGN, e, PN_OP(AST_BITR, e, x)); }
       | plus assign x:eqs  { e = PN_AST2(ASSIGN, e, PN_OP(AST_PLUS, e, x)); }
       | minus assign x:eqs { e = PN_AST2(ASSIGN, e, PN_OP(AST_MINUS, e, x)); }
       | times assign x:eqs { e = PN_AST2(ASSIGN, e, PN_OP(AST_TIMES, e, x)); }
       | div assign x:eqs   { e = PN_AST2(ASSIGN, e, PN_OP(AST_DIV, e, x)); }
       | rem assign x:eqs   { e = PN_AST2(ASSIGN, e, PN_OP(AST_REM, e, x)); }
       | pow assign x:eqs   { e = PN_AST2(ASSIGN, e, PN_OP(AST_POW, e, x)); })*
       { $$ = e; }
             
eqs = c:cmps
      ( cmp x:cmps          { c = PN_OP(AST_CMP, c, x); }
      | eq x:cmps           { c = PN_OP(AST_EQ, c, x); }
      | neq x:cmps          { c = PN_OP(AST_NEQ, c, x); })*
      { $$ = c; }

cmps = o:bitors
       ( gte x:bitors        { o = PN_OP(AST_GTE, o, x); }
       | gt x:bitors         { o = PN_OP(AST_GT, o, x); }
       | lte x:bitors        { o = PN_OP(AST_LTE, o, x); }
       | lt x:bitors         { o = PN_OP(AST_LT, o, x); })*
       { $$ = o; }

bitors = a:bitand
         ( pipe x:bitand       { a = PN_OP(AST_PIPE, a, x); }
         | caret x:bitand      { a = PN_OP(AST_CARET, a, x); })*
         { $$ = a; }

bitand = b:bitshift
         ( amp x:bitshift      { b = PN_OP(AST_AMP, b, x); })*
         { $$ = b; }

bitshift = s:sum
           ( bitl x:sum          { s = PN_OP(AST_BITL, s, x); }
           | bitr x:sum          { s = PN_OP(AST_BITR, s, x); })*
           { $$ = s; }

sum = p:product
      ( plus x:product      { p = PN_OP(AST_PLUS, p, x); }
      | minus x:product     { p = PN_OP(AST_MINUS, p, x); })*
      { $$ = p; }

product = e:expr
          ( times x:expr           { e = PN_OP(AST_TIMES, e, x); }
          | div x:expr             { e = PN_OP(AST_DIV, e, x); }
          | rem x:expr             { e = PN_OP(AST_REM, e, x); }
          | pow x:expr             { e = PN_OP(AST_POW, e, x); })*
          { $$ = e; }

expr = (mminus a:atom { a = PN_OP(AST_INC, a, PN_NUM(-1) ^ 1); }
     | pplus a:atom   { a = PN_OP(AST_INC, a, PN_NUM(1) ^ 1); }
     | minus a:atom   { a = PN_OP(AST_MINUS, PN_AST(VALUE, PN_ZERO), PN_AST(EXPR, PN_TUP(a))); }
     | plus a:atom    { a = PN_OP(AST_PLUS, PN_AST(VALUE, PN_ZERO), PN_AST(EXPR, PN_TUP(a))); }
     | not a:atom     { a = PN_AST(NOT, PN_AST(EXPR, PN_TUP(a))); }
     | wavy a:atom    { a = PN_AST(WAVY, PN_AST(EXPR, PN_TUP(a))); }
     | a:atom (pplus  { a = PN_OP(AST_INC, a, PN_NUM(1)); }
             | mminus { a = PN_OP(AST_INC, a, PN_NUM(-1)); })?) { $$ = a = PN_TUP(a); }
       (c:call { a = PN_PUSH(a, c) })*
     { $$ = PN_AST(EXPR, a); }

atom = e:value | e:closure | e:table | e:call

call = (n:name { v = PN_NIL; b = PN_NIL; } (v:value | v:table)? |
       (v:value | v:table) { n = PN_AST(MESSAGE, PN_NIL); b = PN_NIL; })
         b:block? { $$ = n; PN_S(n, 1) = v; PN_S(n, 2) = b; }

name = p:path           { $$ = PN_AST(PATH, p); }
     | quiz ( m:message { $$ = PN_AST(QUERY, m); }
            | p:path    { $$ = PN_AST(PATHQ, p); })
     | m:message        { $$ = PN_AST(MESSAGE, m); }

lick-items = i1:lick-item     { $$ = i1 = PN_TUP(i1); }
            (sep i2:lick-item { $$ = i1 = PN_PUSH(i1, i2); })*
             sep?
           | ''               { $$ = PN_NIL; }

lick-item = m:message t:table v:loose { $$ = PN_AST3(LICK, m, v, t); }
          | m:message t:table { $$ = PN_AST3(LICK, m, PN_NIL, t); }
          | m:message v:loose t:table { $$ = PN_AST3(LICK, m, v, t); }
          | m:message v:loose { $$ = PN_AST2(LICK, m, v); }
          | m:message         { $$ = PN_AST(LICK, m); }

loose = value
      | v:unquoted { $$ = PN_AST(VALUE, v); }

closure = t:table? b:block { $$ = PN_AST2(PROTO, t, b); }
table = table-start s:statements table-end { $$ = PN_AST(TABLE, s); }
block = block-start s:statements block-end { $$ = PN_AST(BLOCK, s); }
lick = lick-start i:lick-items lick-end { $$ = PN_AST(TABLE, i); }

path = '/' message      { $$ = potion_str2(P, yytext, yyleng); }
message = < utfw+ > -   { $$ = potion_str2(P, yytext, yyleng); }

value = i:immed - { $$ = PN_AST(VALUE, i); }
      | lick

immed = nil   { $$ = PN_NIL; }
      | true  { $$ = PN_TRUE; }
      | false { $$ = PN_FALSE; }
      | hex   { $$ = PN_NUM(PN_ATOI(yytext, yyleng, 16)); }
      | int   { if (yyleng < 10) { $$ = PN_NUM(PN_ATOI(yytext, yyleng, 10)); }
                else { $$ = potion_decimal(P, yytext, yyleng); } }
      | dec   { $$ = potion_decimal(P, yytext, yyleng); }
      | str1 | str2

utfw = [A-Za-z0-9_$@;`{}]
     | '\304' [\250-\277]
     | [\305-\337] [\200-\277]
     | [\340-\357] [\200-\277] [\200-\277]
     | [\360-\364] [\200-\277] [\200-\277] [\200-\277]
utf8 = [\t\n\r\40-\176]
     | [\302-\337] [\200-\277]
     | [\340-\357] [\200-\277] [\200-\277]
     | [\360-\364] [\200-\277] [\200-\277] [\200-\277]

comma = ','
block-start = ':' --
block-end = '.' -
table-start = '(' --
table-end = ')' -
lick-start = '[' --
lick-end = ']' -
quiz = '?' --
assign = '=' --
pplus = "++" -
mminus = "--" -
minus = '-' --
plus = '+' --
wavy = '~' --
times = '*' --
div = '/' --
rem = '%' --
pow = "**" --
bitl = "<<" --
bitr = ">>" --
amp = '&' --
caret = '^' --
pipe = '|' --
lt = '<' --
lte = "<=" --
gt = '>' --
gte = ">=" --
neq = "!=" --
eq = "==" --
cmp = "<=>" --
and = ("&&" | "and") --
or = ("||" | "or") --
not = ("!" | "not") --


nil = "nil"
true = "true"
false = "false"
int = < ('0' | [1-9][0-9]*) !'.' !'e' >
hexl = [0-9A-Fa-f]
hex = '0x' < hexl+ >
dec = < ('0' | [1-9][0-9]*)
        ('.' [0-9]+)? ('e' [-+] [0-9]+)? >

q1 = [']
c1 = < (!q1 utf8)+ > { sbuf = potion_asm_write(P, sbuf, yytext, yyleng); }
str1 = q1 { sbuf = potion_asm_clear(P, sbuf); }
       < (q1 q1 { sbuf = potion_asm_write(P, sbuf, "'", 1); } | c1)* >
       q1 { $$ = potion_bytes_string(P, PN_NIL, (PN)sbuf); }

esc         = '\\'
escn        = esc 'n' { sbuf = potion_asm_write(P, sbuf, "\n", 1); }
escb        = esc 'b' { sbuf = potion_asm_write(P, sbuf, "\b", 1); }
escf        = esc 'f' { sbuf = potion_asm_write(P, sbuf, "\f", 1); }
escr        = esc 'r' { sbuf = potion_asm_write(P, sbuf, "\r", 1); }
esct        = esc 't' { sbuf = potion_asm_write(P, sbuf, "\t", 1); }
escu        = esc 'u' < hexl hexl hexl hexl > {
  int nbuf = 0;
  char utfc[4] = {0, 0, 0, 0};
  unsigned long code = PN_ATOI(yytext, yyleng, 16);
  if (code < 0x80) {
    utfc[nbuf++] = code;
  } else if (code < 0x7ff) {
    utfc[nbuf++] = (code >> 6) | 0xc0;
    utfc[nbuf++] = (code & 0x3f) | 0x80;
  } else {
    utfc[nbuf++] = (code >> 12) | 0xe0;
    utfc[nbuf++] = ((code >> 6) & 0x3f) | 0x80;
    utfc[nbuf++] = (code & 0x3f) | 0x80;
  }
  sbuf = potion_asm_write(P, sbuf, utfc, nbuf);
}
escc = esc < utf8 > { sbuf = potion_asm_write(P, sbuf, yytext, yyleng); }

q2 = ["]
e2 = '\\' ["] { sbuf = potion_asm_write(P, sbuf, "\"", 1); }
c2 = < (!q2 !esc utf8)+ > { sbuf = potion_asm_write(P, sbuf, yytext, yyleng); }
str2 = q2 { sbuf = potion_asm_clear(P, sbuf); }
       < (e2 | escn | escb | escf | escr | esct | escu | escc | c2)* >
       q2 { $$ = potion_bytes_string(P, PN_NIL, (PN)sbuf); }

unquoted = < (!sep !lick-end utf8)+ > { $$ = potion_str2(P, yytext, yyleng); }

- = (space | comment)*
-- = (space | comment | end-of-line)*
sep = (end-of-line | comma) (space | comment | end-of-line | comma)*
comment	= '#' (!end-of-line utf8)*
space = ' ' | '\f' | '\v' | '\t'
end-of-line = '\r\n' | '\n' | '\r'
end-of-file = !.

%%

PN potion_greg_parse(Potion *PP, PN code) {
  PN buf = (PN)potion_asm_new(PP);
  P = PP;
  pos = 0;
  input = code;
  sbuf = (PNAsm *)buf;
  if (!yyparse())
    printf("** Syntax error!\n");
  return P->source;
}
