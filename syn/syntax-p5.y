# -*- mode: antlr; tab-width:8 -*-
#
# syntax-p5.g
# perl5 tokens and grammar
#
# (c) 2009 _why
# (c) 2013 by perl11 org
#

%{
#ifndef P2
# define P2
#endif
#include "p2.h"
#include "internal.h"
#include "asm.h"
#include "ast.h"

#define YYSTYPE PN
#define YY_XTYPE Potion *
#define YY_XVAR P

#define YY_INPUT(G, buf, result, max) { \
  YY_XTYPE P = G->data; \
  if (P->yypos < PN_STR_LEN(P->input)) { \
    result = max; \
    if (P->yypos + max > PN_STR_LEN(P->input)) \
      result = (PN_STR_LEN(P->input) - P->yypos); \
    PN_MEMCPY_N(buf, PN_STR_PTR(P->input) + P->yypos, char, result + 1); \
    P->yypos += max; \
  } else { \
    result = 0; \
  } \
}

#define YY_NAME(N) p5_code_##N

#define YY_TNUM 3
#define YY_TDEC 13

#ifdef YY_DEBUG
# define YYDEBUG_PARSE   DEBUG_PARSE
# define YYDEBUG_VERBOSE DEBUG_PARSE_VERBOSE

# define YY_SET(G, text, count, thunk, P) \
  yyprintf((stderr, "%s %d %p:<%s>\n", thunk->name, count,(void*)yy,\
           PN_STR_PTR(potion_send(yy, PN_string, 0)))); \
  G->val[count]= yy;
#endif

#define DEF_PSRC	(P->source?P->source:PN_TUP0())
//const char *Nullch = '\0';
#define SRC_TPL1(x)     P->source = PN_PUSH(DEF_PSRC, (x))
#define SRC_TPL2(x,y)   P->source = PN_PUSH(PN_PUSH(DEF_PSRC, (x)), (y))
#define SRC_TPL3(x,y,z) P->source = PN_PUSH(PN_PUSH(PN_PUSH(DEF_PSRC, (x)), (y)), (z))

%}

perl5 = -- s:statements end-of-file
        { $$ = P->source = PN_AST(CODE, s);
          if (yyleng) YY_ERROR(G,"** Syntax error"); }

# AST BLOCK captures lexicals
# Note that if/else blocks (mblock) do not capture lexicals
# block = '{' s:lineseq '}' { $$ = PN_AST(BLOCK, s) }

statements =
    s1:stmt           { $$ = s1 = PN_IS_TUPLE(s1) ? s1 : PN_TUP(s1) }
        (sep? s2:stmt { $$ = s1 = PN_PUSH(s1, s2) })* sep?
    | ''              { $$ = PN_NIL }

stmt = pkgdecl
    | BEGIN b:block           { p2_eval(P, b) }
    | subrout
    | u:use                   { $$ = PN_AST(EXPR, u) }
    | i:ifstmt                { $$ = PN_AST(EXPR, i) }
    | assigndecl
    | block
    | s:sets semi
        ( or x:sets semi      { s = PN_OP(AST_OR, s, x) }
        | and x:sets semi     { s = PN_OP(AST_AND, s, x) })*
                              { $$ = s }
    | s:sets                  { $$ = s }

listexprs = e1:eqs           { $$ = e1 = PN_IS_TUPLE(e1) ? e1 : PN_TUP(e1) }
        ( - comma - e2:eqs   { $$ = e1 = PN_PUSH(e1, e2) } )*
# listexprs + named args: $x=1 (i.e. assignment)
callexprs = e1:sets           { $$ = e1 = PN_IS_TUPLE(e1) ? e1 : PN_TUP(e1) }
        ( - comma - e2:sets   { $$ = e1 = PN_PUSH(e1, e2) } )*

BEGIN   = "BEGIN" space+
PACKAGE = "package" space+
USE     = "use" space+
SUB     = "sub" space+
IF      = "if" space+
ELSIF   = "elsif" space+
ELSE    = "else" space+
MY      = "my" space+

p5-siglist = list-start args2* list-end - { $$ = PN_AST(LIST, P->source); P->source = PN_NIL }
#TODO: store name globally
subrout = SUB n:id - l:p5-siglist b:block -
          { $$ = PN_AST2(ASSIGN, PN_AST(EXPR, PN_TUP(PN_AST(MSG, n))),
                                 PN_AST(EXPR, PN_TUP(PN_AST2(PROTO, l, b)))) }
        | SUB n:id - b:block -
          { $$ = PN_AST2(ASSIGN, PN_AST(EXPR, PN_TUP(PN_AST(MSG, n))),
                                 PN_AST(EXPR, PN_TUP(PN_AST2(PROTO, PN_AST(LIST, PN_NIL), b)))) }
anonsub = SUB l:p5-siglist? b:block -
        { $$ = PN_AST2(PROTO, l, b) }
# so far no difference in global or lex assignment
#subrout = SUB n:id - l:p5-siglist? a:subattrlist? b:block
#lexsubrout = MY - SUB n:subname p:proto? a:subattrlist? b:subbody
#        { $$ = PN_AST2(ASSIGN, n, PN_AST2(PROTO, p, b)) }
#subattrlist = ':' -? arg-name

# TODO: compile-time sideeffs: require + import
use = USE n:id                     { $$ = PN_AST2(MSG, PN_use, n) }
    | USE n:id - fatcomma - l:atom { $$ = PN_AST3(MSG, PN_use, n, l) }

pkgdecl = PACKAGE n:arg-name semi          {} # TODO: set namespace
    | PACKAGE n:arg-name v:version? b:block

ifstmt = IF e:ifexpr s:block - !"els"  { $$ = PN_TUP(PN_OP(AST_AND, e, s)) }
       | IF e:ifexpr s1:block -        { $$ = e = PN_AST3(MSG, PN_if, PN_AST(LIST, PN_TUP(e)), s1) }
         (ELSIF e1:ifexpr f:block -    { $$ = e = PN_PUSH(PN_TUPIF(e), PN_AST3(MSG, PN_elsif, PN_AST(LIST, PN_TUP(e1)), f)) } )*
         (ELSE s2:block                { $$ = PN_PUSH(PN_TUPIF(e), PN_AST3(MSG, PN_else, PN_NIL, s2)) } )?
ifexpr = '(' - eqs - ')' -

assigndecl =
        l:listvar - assign r:list - { $$ = PN_AST2(ASSIGN, l, r) }
      | MY l:listvar - assign r:list - { $$ = PN_AST2(ASSIGN, l, r) }
      | l:list - assign r:list - {
        PN s1 = PN_TUP0(); PN_TUPLE_EACH(l, i, v, {
          s1 = PN_PUSH(s1, PN_AST2(ASSIGN, v, PN_TUPLE_AT(r,i)));
        }); $$ = s1 }
      | l:global - assign e:eqs - { $$ = PN_AST2(ASSIGN, l, e) }
      | MY - l:lexical - assign e:eqs - { $$ = PN_AST2(ASSIGN, l, e) }
      | l:global - assign r:list { YY_ERROR(G, "** Assignment error") }

lexical = global

sets = e:eqs
       ( assign s:sets       { e = PN_AST2(ASSIGN, e, s) }
       | or assign s:sets    { e = PN_AST2(ASSIGN, e, PN_OP(AST_OR, e, s)) }
       | and assign s:sets   { e = PN_AST2(ASSIGN, e, PN_OP(AST_OR, e, s)) }
       | pipe assign s:sets  { e = PN_AST2(ASSIGN, e, PN_OP(AST_PIPE, e, s)) }
       | caret assign s:sets { e = PN_AST2(ASSIGN, e, PN_OP(AST_CARET, e, s)) }
       | amp assign s:sets   { e = PN_AST2(ASSIGN, e, PN_OP(AST_AMP, e, s)) }
       | bitl assign s:sets  { e = PN_AST2(ASSIGN, e, PN_OP(AST_BITL, e, s)) }
       | bitr assign s:sets  { e = PN_AST2(ASSIGN, e, PN_OP(AST_BITR, e, s)) }
       | plus assign s:sets  { e = PN_AST2(ASSIGN, e, PN_OP(AST_PLUS, e, s)) }
       | minus assign s:sets { e = PN_AST2(ASSIGN, e, PN_OP(AST_MINUS, e, s)) }
       | times assign s:sets { e = PN_AST2(ASSIGN, e, PN_OP(AST_TIMES, e, s)) }
       | div assign s:sets   { e = PN_AST2(ASSIGN, e, PN_OP(AST_DIV, e, s)) }
       | rem assign s:sets   { e = PN_AST2(ASSIGN, e, PN_OP(AST_REM, e, s)) }
       | pow assign s:sets   { e = PN_AST2(ASSIGN, e, PN_OP(AST_POW, e, s)) })?
       { $$ = e }

eqs = c:cmps
      ( cmp x:cmps          { c = PN_OP(AST_CMP, c, x) }
      | eq x:cmps           { c = PN_OP(AST_EQ, c, x) }
      | neq x:cmps          { c = PN_OP(AST_NEQ, c, x) })*
      { $$ = c }

cmps = o:bitors
       ( gte x:bitors        { o = PN_OP(AST_GTE, o, x) }
       | gt x:bitors         { o = PN_OP(AST_GT, o, x) }
       | lte x:bitors        { o = PN_OP(AST_LTE, o, x) }
       | lt x:bitors         { o = PN_OP(AST_LT, o, x) })*
       { $$ = o }

bitors = a:bitand
         ( pipe x:bitand       { a = PN_OP(AST_PIPE, a, x) }
         | caret x:bitand      { a = PN_OP(AST_CARET, a, x) })*
         { $$ = a }

bitand = b:bitshift
         ( amp x:bitshift      { b = PN_OP(AST_AMP, b, x) })*
         { $$ = b }

bitshift = s:sum
           ( bitl x:sum          { s = PN_OP(AST_BITL, s, x) }
           | bitr x:sum          { s = PN_OP(AST_BITR, s, x) })*
           { $$ = s }

sum = p:product
      ( plus x:product      { p = PN_OP(AST_PLUS, p, x) }
      | minus x:product     { p = PN_OP(AST_MINUS, p, x) })*
      { $$ = p }

product = p:power
          ( times x:power           { p = PN_OP(AST_TIMES, p, x) }
          | div x:power             { p = PN_OP(AST_DIV, p, x) }
          | rem x:power             { p = PN_OP(AST_REM, p, x) })*
          { $$ = p }

power = e:expr
        ( pow x:expr { e = PN_OP(AST_POW, e, x) })*
        { $$ = e }

expr = c:method  	        { $$ = PN_AST(EXPR, c) }
    | c:calllist		{ $$ = PN_AST(EXPR, c) }
    | c:call e:expr 		{ $$ = PN_AST(EXPR, PN_PUSH(PN_TUPIF(e), c)) }
    | c:call l:listexprs        { $$ = potion_tuple_shift(P, 0, PN_S(l,0));
            if (!PN_S(l, 0)) { PN_SRC(c)->a[1] = PN_SRC($$); }
            $$ = PN_PUSH(PN_TUP($$), c); }
    | e:opexpr			{ $$ = PN_AST(EXPR, PN_TUP(e)) }
    | c:call			{ $$ = PN_AST(EXPR, c) }
    | e:atom			{ $$ = PN_AST(EXPR, PN_TUP(e)) }

opexpr = not e:expr		{ $$ = PN_AST(NOT, e) }
    | bitnot e:expr		{ $$ = PN_AST(WAVY, e) }
    | l:atom times !times r:atom { $$ = PN_OP(AST_TIMES, l, r) }
    | l:atom div   !div r:atom   { $$ = PN_OP(AST_DIV,  l, r) }
    | l:atom minus !minus r:atom { $$ = PN_OP(AST_MINUS, l, r) }
    | l:atom plus !plus r:atom   { $$ = PN_OP(AST_PLUS,  l, r) }
    | mminus e:mvalue		{ $$ = PN_OP(AST_INC, e, PN_NUM(-1) ^ 1) }
    | pplus e:mvalue		{ $$ = PN_OP(AST_INC, e, PN_NUM(1) ^ 1) }
    | e:mvalue (pplus		{ $$ = PN_OP(AST_INC, e, PN_NUM(1)) }
             | mminus		{ $$ = PN_OP(AST_INC, e, PN_NUM(-1)) }) {}

atom = e:value | e:list | e:anonsub

#FIXME methods and indirect methods:
#   chr 101  => (expr (value (101), msg ("chr")))
#   chr(101,1) => (expr (value (101), msg ("chr") list (value 1)))
#   print chr 101 => (expr (value (101), msg ("chr"), msg ("print")))
#   obj->meth(args) => (expr (msg obj), msg (meth) list (expr args))
#TODO: if (cond) {block} => expr (if, cond, block)
# callexprs allows assignment for named args
calllist = m:name - list-start - list-end { 
                 PN_SRC(m)->a[1] = PN_SRC(PN_AST(LIST, PN_NIL));
                 $$ = PN_TUP(m) }
         | m:name - l:list - { PN_SRC(m)->a[1] = PN_SRC(l); $$ = PN_TUP(m) }
                 # $$ = potion_tuple_shift(P, 0, PN_S(l,0));
                 # if (PN_TUPLE_LEN(PN_S(l, 0))) { PN_SRC(m)->a[1] = PN_SRC(l); }
                 # $$ = PN_PUSH(PN_TUP($$), m) }
         | m:name - list-start l:callexprs list-end - {
                 PN_SRC(m)->a[1] = PN_SRC(PN_AST(LIST, l));
                 $$ = PN_TUP(m) }
call = m:name - { $$ = m }
method = v:value - arrow m:name - l:list - {
            PN_SRC(m)->a[1] = PN_SRC(l); $$ = PN_PUSH(PN_TUPIF(v), m) }
       | v:value - arrow m:name - {
            $$ = PN_PUSH(PN_TUPIF(v), m) }

name = !keyword m:id      { $$ = PN_AST(MSG, m) }
     | !keyword m:funcvar { $$ = PN_AST(MSG, m) }

lick-items = i1:lick-item     { $$ = i1 = PN_TUP(i1) }
            (sep i2:lick-item { $$ = i1 = PN_PUSH(i1, i2) })*
             sep?
           | ''               { $$ = PN_NIL }

lick-item = m:msg t:list v:loose { $$ = PN_AST3(LICK, m, v, t) }
          | m:msg t:list { $$ = PN_AST3(LICK, m, PN_NIL, t) }
          | m:msg v:loose t:list { $$ = PN_AST3(LICK, m, v, t) }
          | m:msg v:loose { $$ = PN_AST2(LICK, m, v) }
          | m:msg         { $$ = PN_AST(LICK, m) }

loose = value
      | v:unquoted { $$ = PN_AST(VALUE, v) }

# anonymous sub, w or w/o proto (aka list)
#sub = 'sub' - n:arg-name - t:list? b:block  { PN_AST2(ASSIGN, n, PN_AST2(PROTO, t, b)) }
list = list-start s:listexprs list-end -     { $$ = PN_AST(LIST, s) }
block = block-start s:statements block-end - { $$ = PN_AST(BLOCK, s) }
lick = lick-start i:lick-items lick-end -    { $$ = PN_AST(LIST, i) }
group = group-start s:statements group-end - { $$ = PN_AST(EXPR, s) }

#path = '/' < utfw+ > - { $$ = PN_STRN(yytext, yyleng) }
#path    = < utfw+ > -  { $$ = PN_STRN(yytext, yyleng) }
msg = < utfw+ > -   	{ $$ = PN_STRN(yytext, yyleng) }

mvalue = i:immed - { $$ = PN_AST(VALUE, i) }
      | global

value = i:immed - { $$ = PN_AST(VALUE, i) }
      | global
      | lick
      | group

immed = undef { $$ = PN_NIL }
#      | true  { $$ = PN_TRUE }
#      | false { $$ = PN_FALSE }
      | hex   { $$ = PN_NUM(PN_ATOI(yytext, yyleng, 16)) }
      | dec   { $$ = ($$ == YY_TNUM) ? PN_NUM(PN_ATOI(yytext, yyleng, 10))
                   : potion_decimal(P, yytext, yyleng) }
      | str1 | str2

global  = scalar | listvar | hashvar | listel | hashel | funcvar | globvar
# send the value a msg, every global is a closure (see name)
scalar  = < '$' i:id > - !'[' !'{'
        { $$ = PN_AST(MSG, PN_STRCAT("$", PN_STR_PTR(i))) }
listvar = < '@' i:id > - { $$ = PN_AST(MSG, PN_STRCAT("@", PN_STR_PTR(i))) }
hashvar = < '%' i:id > - { $$ = PN_AST(MSG, PN_STRCAT("%", PN_STR_PTR(i))) }
funcvar = < '&' i:id > - { $$ = PN_AST(MSG, PN_STRCAT("&", PN_STR_PTR(i))) }
globvar = < '*' i:id > - { $$ = PN_AST(MSG, PN_STRCAT("*", PN_STR_PTR(i))) }
listel  = < '$' l:id - '[' - i:value - ']' > -
        { $$ = PN_AST2(MSG, PN_STRCAT("@", PN_STR_PTR(l)), PN_AST(EXPR, PN_TUP(i))) }
hashel  = < '$' h:id - '{' - k:value - '}' > -
        { $$ = PN_AST2(MSG, PN_STRCAT("%", PN_STR_PTR(h)), PN_AST(EXPR, PN_TUP(k))) }

semi = ';'
comma = ','
fatcomma = '=>'
arrow = "->" -
block-start = '{' space*
block-end = semi? space* '}'
list-start = '(' -
list-end = ')' -
lick-start = '[' -
lick-end = ']' -
group-start = '{'
group-end = '}'
bitnot = '~' -
assign = '=' -
defassign = ":=" --
pplus = "++" -
mminus = "--" -
minus = '-' -
plus = '+' -
times = '*' -
div = '/' -
rem = '%' -
pow = "**" -
bitl = "<<" -
bitr = ">>" -
amp = '&' -
caret = '^' -
pipe = '|' -
lt = '<' -
lte = "<=" -
gt = '>' -
gte = ">=" -
neq = ("!=" | "ne" !utfw) --
eq = ("==" | "eq" !utfw) --
cmp = "<=>" --
and = ("&&" | "and" !utfw) --
or = ("||" | "or" !utfw) --
not = ("!" | "not" !utfw) --
# only compiler specific keywords
keyword = ("and" | "or" | "not")

undef = "undef" !utfw
#true = "true" !utfw
#false = "false" !utfw
hexl = [0-9A-Fa-f]
hex = '0x' < hexl+ >
dec = < ('0' | [1-9][0-9]*) { $$ = YY_TNUM }
        ('.' [0-9]+ { $$ = YY_TDEC })?
        ('e' [-+] [0-9]+ { $$ = YY_TDEC })? >
version = 'v'? < ('0' | [1-9][0-9]*) { $$ = YY_TNUM }
          ('.' [0-9]+ { $$ = YY_TDEC })? >

q1 = [']   # ' emacs highlight problems
c1 = < (!q1 utf8)+ > { P->pbuf = potion_asm_write(P, P->pbuf, yytext, yyleng) }
str1 = q1 { P->pbuf = potion_asm_clear(P, P->pbuf) }
       < (q1 q1 { P->pbuf = potion_asm_write(P, P->pbuf, "'", 1) } | c1)* >
       q1 { $$ = potion_bytes_string(P, PN_NIL, (PN)P->pbuf) }

esc         = '\\'
escn        = esc 'n' { P->pbuf = potion_asm_write(P, P->pbuf, "\n", 1) }
escb        = esc 'b' { P->pbuf = potion_asm_write(P, P->pbuf, "\b", 1) }
escf        = esc 'f' { P->pbuf = potion_asm_write(P, P->pbuf, "\f", 1) }
escr        = esc 'r' { P->pbuf = potion_asm_write(P, P->pbuf, "\r", 1) }
esct        = esc 't' { P->pbuf = potion_asm_write(P, P->pbuf, "\t", 1) }
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
  P->pbuf = potion_asm_write(P, P->pbuf, utfc, nbuf);
}
escc = esc < utf8 > { P->pbuf = potion_asm_write(P, P->pbuf, yytext, yyleng) }

q2 = ["]
e2 = '\\' ["] { P->pbuf = potion_asm_write(P, P->pbuf, "\"", 1) }
c2 = < (!q2 !esc utf8)+ > { P->pbuf = potion_asm_write(P, P->pbuf, yytext, yyleng) }
str2 = q2 { P->pbuf = potion_asm_clear(P, P->pbuf) }
       < (e2 | escn | escb | escf | escr | esct | escu | escc | c2)* >
       q2 { $$ = potion_bytes_string(P, PN_NIL, (PN)P->pbuf) }

unq-char = '{' unq-char+ '}'
         | '[' unq-char+ ']'
         | '(' unq-char+ ')'
         | !'{' !'[' !'(' !'}' !']' !')' utf8
unq-sep = sep !'{' !'[' !'('
unquoted = < (!unq-sep !lick-end unq-char)+ > { $$ = PN_STRN(yytext, yyleng) }

# lexer rules which are only printed with -DP, not with -Dp:
- = (space | comment)*
-- = (space | comment | semi)*
sep = semi (space | comment | semi)*
comment	= '#' (!end-of-line utf8)*
# PSXSPC
# \240 U+A0 NO-BREAK SPACE
# \205 U+85 NEL
space = ' ' | '\f' | '\v' | '\t' | '\205' | '\240' | end-of-line
end-of-line = '\r\n' | '\n' | '\r' { $$ = PN_AST2(DEBUG, PN_NUM(G->lineno), PN_NIL) }
end-of-file = !'\0'
# FIXME: starting wordchar (no numbers) + wordchars
id = < IDFIRST utfw* > { $$ = PN_STRN(yytext, yyleng) }
# isWORDCHAR && IDFIRST, no numbers
IDFIRST = [A-Za-z_]
     | '\304' [\250-\277]
     | [\305-\337] [\200-\277]
     | [\340-\357] [\200-\277] [\200-\277]
     | [\360-\364] [\200-\277] [\200-\277] [\200-\277]
# isWORDCHAR? \w and [:word:]
utfw = [A-Za-z0-9_]
     | '\304' [\252-\277]
     | [\305-\337] [\200-\277]
     | [\340-\357] [\200-\277] [\200-\277]
     | [\360-\364] [\200-\277] [\200-\277] [\200-\277]
# isWORDCHAR && XID_Continue
#IDCONT = [A-Za-z0-9_ ():\240-]
#     | '\304' [\250-\277]
#     | [\305-\337] [\200-\277]
#     | [\340-\357] [\200-\277] [\200-\277]
#     | [\360-\364] [\200-\277] [\200-\277] [\200-\277]
#IDPRINT = [\40-\176]
#     | [\302-\337] [\200-\277]
#     | [\340-\357] [\200-\277] [\200-\277]
#     | [\360-\364] [\200-\277] [\200-\277] [\200-\277]
utf8 = [\t\n\r\40-\176]
     | [\302-\337] [\200-\277]
     | [\340-\357] [\200-\277] [\200-\277]
     | [\360-\364] [\200-\277] [\200-\277] [\200-\277]

# for potion_sig, used in the runtime initialization
sig = args+ end-of-file
args = arg-list (arg-sep arg-list)*
arg-list = arg-set (optional arg-set)?
         | optional arg-set
arg-set = arg (comma - arg)*

arg-name = < utfw+ > - { $$ = PN_STRN(yytext, yyleng) }
arg-modifier = < ('-' | '\\' | '*' ) >  { $$ = PN_NUM(yytext[0]); }
# for FFIs, map to potion and C types. See potion_type_char()
arg-type = < [NS&oTaubnBsFPlkftxrcdm] > - { $$ = PN_NUM(yytext[0]) }
arg = m:arg-modifier n:arg-name assign t:arg-type
                        { SRC_TPL3(n,t,m) }
    | m:arg-modifier n:arg-name
                        { SRC_TPL3(n,0,m) }
    | n:arg-name assign t:arg-type
                        { SRC_TPL2(n,t) }
    | n:arg-name defassign d:value     # x:=0, optional
                        { SRC_TPL3(n, PN_NUM(':'), PN_S(d,0)) }
    # single types without name (N,o) as for FFIs forbidden, use (dummy=N) instead
    # | assign t:arg-type { SRC_TPL2(PN_STR(""),t) }
    | n:arg-name        { SRC_TPL1(n) }
optional = '|' -        { SRC_TPL1(PN_NUM('|')) }
arg-sep = '.' -         { SRC_TPL1(PN_NUM('.')) } #x,y... ignore rest

# p5 sigs. used by the seperate p2_sig, already in compiled 3-tuple format
sig_p5 = args2* end-of-file
args2 = arg2-list (arg2-yada)*
YADA = "..."
arg2-yada = YADA -  { SRC_TPL1(PN_NUM('.')) }
arg2-list = arg2-set (optional arg2-set)?
          | optional arg2-set
arg2-set = arg2 (comma - arg2)*

arg2-sigil = < [$@%] >          { $$ = PN_STRN(yytext, yyleng) }
arg2-name = s:arg2-sigil i:id - { $$ = potion_str_add(P, 0, s, i) }
# types are classes
arg2-type = !'$' i:id space+  { $$ = potion_class_find(P, i); if (!$$) yyerror(G,"Invalid signature type") }
arg2 = !arg2-sigil t:arg2-type m:arg-modifier n:arg2-name { SRC_TPL3(n,t,m) }
     | !arg2-sigil t:arg2-type n:arg2-name 		{ SRC_TPL2(n,t) }
     | m:arg-modifier n:arg2-name 		 	{ SRC_TPL3(n,0,m) }
     | n:arg2-name - assign d:value			{ SRC_TPL3(n,PN_NUM(':'), PN_S(d,0)) }
     | n:arg2-name					{ SRC_TPL1(n) }

%%

PN p2_parse(Potion *P, PN code, char *filename) {
  GREG *G = YY_NAME(parse_new)(P);
  P->yypos = 0;
  P->input = code;
  P->source = PN_NIL;
  P->pbuf = potion_asm_new(P);
  yydebug = P->flags;

  G->filename = filename;
  if (!YY_NAME(parse)(G)) {
    YY_ERROR(G, "** Syntax error");
    fprintf(stderr, "%s", PN_STR_PTR(code));
  }
  YY_NAME(parse_free)(G);

  code = P->source;
  P->source = PN_NIL;
  return code;
}

// duplicate but still needed to compile internal methods
PN potion_sig(Potion *P, char *fmt) {
  PN out = PN_NIL;
  if (fmt == NULL) return PN_NIL;
  if (fmt[0] == '\0') return PN_FALSE;

  GREG *G = YY_NAME(parse_new)(P);
  P->yypos = 0;
  P->input = potion_byte_str(P, fmt);
  P->source = out = PN_TUP0();
  P->pbuf = NULL;
  yydebug = P->flags;

  if (!YY_NAME(parse_from)(G, yy_sig))
    YY_ERROR(G, "** Signature syntax error");
  YY_NAME(parse_free)(G);

  out = P->source;
  P->source = PN_NIL;
  return out;
}

PN p2_sig(Potion *P, char *fmt) {
  PN out = PN_NIL;
  if (fmt == NULL) return PN_NIL; // no signature, arg check off
  if (fmt[0] == '\0') return PN_FALSE; // empty signature, no args

  GREG *G = YY_NAME(parse_new)(P);
  P->yypos = 0;
  P->input = potion_byte_str(P, fmt);
  P->source = out = PN_TUP0();
  P->pbuf = NULL;
  yydebug = P->flags;

  if (!YY_NAME(parse_from)(G, yy_sig_p5))
    YY_ERROR(G, "** Signature syntax error");
  YY_NAME(parse_free)(G);

  out = P->source;
  P->source = PN_NIL;
  return out;
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
