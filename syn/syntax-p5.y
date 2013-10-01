# -*- mode: antlr; tab-width:8 -*-
#
# syntax-p5.y
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

#undef PN_AST
#undef PN_AST2
#undef PN_AST3
#undef PN_OP
#define PN_AST(T, A)        potion_source(P, AST_##T, A, PN_NIL, PN_NIL, G->lineno, P->line)
#define PN_AST2(T, A, B)    potion_source(P, AST_##T, A, B, PN_NIL, G->lineno, P->line)
#define PN_AST3(T, A, B, C) potion_source(P, AST_##T, A, B, C, G->lineno, P->line)
#define PN_OP(T, A, B)      potion_source(P, T, A, B, PN_NIL, G->lineno, P->line)

#define YYSTYPE PN
#define YY_XTYPE Potion *
#define YY_XVAR P

#define YY_INPUT(buf, result, max) { \
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

// -Dp: GC in the parser in potion_send fails in moved PNSource objects.
// we may still hold refs in the parser to old objects, G->ss not on the stack
# define YY_SET1(G, text, count, thunk, P) \
  yyprintf((stderr, "%s %d %p:<%s>\n", thunk->name, count,(void*)yy,\
           PN_STR_PTR(potion_send(yy, PN_string, 0)))); \
  G->val[count]= yy;
#endif

#define DEF_PSRC	(P->source?P->source:PN_TUP0())
//const char *Nullch = '\0';
#define SRC_TPL1(x)     P->source = PN_PUSH(DEF_PSRC, (x))
#define SRC_TPL2(x,y)   P->source = PN_PUSH(PN_PUSH(DEF_PSRC, (x)), (y))
#define SRC_TPL3(x,y,z) P->source = PN_PUSH(PN_PUSH(PN_PUSH(DEF_PSRC, (x)), (y)), (z))

static PN yylastline(struct _GREG *G, int pos);
%}

perl5 = -- s:statements end-of-file
   { $$ = P->source = PN_AST(CODE, s);
     s = (PN)(G->buf+G->pos);
     if (yyleng) YY_ERROR("** Syntax error");
     else if (*(char*)s) YY_ERROR("** Internal parser error: Couldn't parse all statements") }

# AST BLOCK captures lexicals
# Note that if/else blocks (mblock) do not capture lexicals
# block = '{' s:lineseq '}' { $$ = PN_AST(BLOCK, s) }

statements =
    s1:stmt           { $$ = s1 = PN_IS_TUPLE(s1) ? s1 : PN_TUP(s1) }
        (sep? s2:stmt { $$ = s1 = PN_PUSH(s1, s2) } )* sep?
    | ''              { $$ = PN_NIL }

stmt = pkgdecl
    | BEGIN b:block           { p2_eval(P, b) }
    | subrout
    | u:use sep?              { $$ = PN_TUP0() }
    | i:ifstmt                { $$ = PN_AST(EXPR, i) }
    | forlist
    | a:assigndecl IF e:ifnexpr sep?
      { $$ = PN_OP(AST_AND, e, a) }
    | a:assigndecl UNLESS e:ifnexpr sep?
      { $$ = PN_OP(AST_AND, PN_AST(NOT, e), a) }
    | assigndecl sep?
    | block
    | a:sets IF e:ifnexpr sep?
      { $$ = PN_OP(AST_AND, e, a) }
    | a:sets UNLESS e:ifnexpr sep?
      { $$ = PN_OP(AST_AND, PN_AST(NOT, e), a) }
    | s:sets
        ( or x:sets           { s = PN_OP(AST_OR, s, x) }
        | and x:sets          { s = PN_OP(AST_AND, s, x) })* sep?
                              { $$ = s }
    | s:sets sep?             { $$ = s }
    | l:list sep?             { $$ = PN_AST(EXPR, l) }

listexprs = e1:eqs           { $$ = e1 = PN_IS_TUPLE(e1) ? e1 : PN_TUP(e1) }
        ( - comma - e2:eqs   { $$ = e1 = PN_PUSH(e1, e2) } )*
# listexprs + named args: $x=1 (i.e. assignment)
callexprs = e1:sets           { $$ = e1 = PN_IS_TUPLE(e1) ? e1 : PN_TUP(e1) }
        ( - comma - e2:sets   { $$ = e1 = PN_PUSH(e1, e2) } )*

BEGIN   = "BEGIN" space+
PACKAGE = "package" space+
USE     = "use" space+
NO      = "no" space+
SUB     = "sub" space+
IF      = "if" space+
UNLESS  = "unless" space+
ELSIF   = "elsif" space+
ELSE    = "else" space+
MY      = "my" space+
FOR     = "for" space+
FOREACH = "foreach" space+

p5-siglist = list-start args2* list-end { $$ = PN_AST(LIST, P->source); P->source = PN_NIL }
#TODO: store name globally
subrout = SUB n:id - l:p5-siglist b:block
          { $$ = PN_AST2(ASSIGN, PN_AST(EXPR, PN_TUP(PN_AST(MSG, n))),
                                 PN_AST(EXPR, PN_TUP(PN_AST2(PROTO, l, b)))) }
        | SUB n:id - b:block
          { $$ = PN_AST2(ASSIGN, PN_AST(EXPR, PN_TUP(PN_AST(MSG, n))),
                                 PN_AST(EXPR, PN_TUP(PN_AST2(PROTO, PN_AST(LIST, PN_NIL), b)))) }
anonsub = SUB l:p5-siglist? b:block
        { $$ = PN_AST2(PROTO, l, b) }
# so far no difference in global or lex assignment
#subrout = SUB n:id - l:p5-siglist? a:subattrlist? b:block
#lexsubrout = MY - SUB n:subname p:proto? a:subattrlist? b:subbody
#        { $$ = PN_AST2(ASSIGN, n, PN_AST2(PROTO, p, b)) }
#subattrlist = ':' -? arg-name

# TODO: parse-time sideeffs: require + import, in the compiler its too late
use = (u:USE|u:NO) v:version
        { p2_eval(P, PN_AST(BLOCK, PN_TUP(PN_AST2(MSG, PN_use, PN_AST(LIST, PN_PUSH(PN_TUP(u), v)))))) }
    | u:USE n:id - "p2"          { P->flags = (Potion_Flags)((int)P->flags | MODE_P2); }
    | u:NO n:id - "p2"           { P->flags = (Potion_Flags)((int)P->flags & ~MODE_P2); }
    | (u:USE|u:NO) n:id
        { p2_eval(P, PN_AST(BLOCK, PN_TUP(PN_AST2(MSG, PN_use, PN_AST(LIST, PN_PUSH(PN_TUP(u), n)))))) }
    | (u:USE|u:NO) n:id fatcomma l:atom
        { p2_eval(P, PN_AST(BLOCK, PN_TUP(PN_AST2(MSG, PN_use, PN_AST(LIST, PN_PUSH(u,PN_PUSH(PN_PUSH(PN_TUP(u),n),l))))))) }

pkgdecl = PACKAGE n:arg-name semi          {} # TODO: set namespace
        | PACKAGE n:arg-name v:version? b:block

ifstmt = IF e:ifexpr s:block !"els"   { $$ = PN_TUP(PN_OP(AST_AND, e, s)) }
       | IF e:ifexpr s1:block         { $$ = e = PN_AST3(MSG, PN_if, PN_AST(LIST, PN_TUP(e)), s1) }
         (ELSIF e1:ifexpr f:block     { $$ = e = PN_PUSH(PN_TUPIF(e), PN_AST3(MSG, PN_elsif, PN_AST(LIST, PN_TUP(e1)), f)) } )*
         (ELSE s2:block               { $$ = PN_PUSH(PN_TUPIF(e), PN_AST3(MSG, PN_else, PN_NIL, s2)) } )?
ifexpr = list-start eqs - list-end
ifnexpr = ifexpr | eqs

forlist = (FOR | FOREACH) i:lexglobal l:list b:block {
            yyerror(G,"forlist iterator nyi") }

assigndecl =
        MY t:name l:listvar assign r:list { PN_SRC(l)->a[2] = PN_SRC(t); $$ = PN_AST2(ASSIGN, l, r) }
      | MY? l:listvar assign r:list       { $$ = PN_AST2(ASSIGN, l, r) }
      | MY t:name l:list assign r:list    # typed lists
          { PN s1 = PN_TUP0(); PN_TUPLE_EACH(PN_S(l,0), i, v, {
            PN_SRC(v)->a[2] = PN_SRC(t);
            s1 = PN_PUSH(s1, PN_AST2(ASSIGN, v, potion_tuple_at(P,0,PN_S(r,0),PN_NUM(i))));
          }); $$ = PN_AST(EXPR, s1) }
      | MY? l:list assign r:list          # aasign
          { PN s1 = PN_TUP0(); PN_TUPLE_EACH(PN_S(l,0), i, v, {
            s1 = PN_PUSH(s1, PN_AST2(ASSIGN, v, potion_tuple_at(P,0,PN_S(r,0),PN_NUM(i))));
          }); $$ = PN_AST(EXPR, s1) }
      | l:lexglobal assign e:eqs -  { $$ = PN_AST2(ASSIGN, l, e) }
      | l:global assign r:list      { YY_ERROR("** Assignment error") } # @x = () nyi

#TODO most of these stack-like assign-expr cases can probably go away
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

# always a list
expr = c:method  	        { $$ = PN_AST(EXPR, c) }
    | m:special l:list b:block  { PN_SRC(m)->a[1] = PN_SRC(l);
            PN_SRC(m)->a[2] = PN_SRC(b);
            $$ = PN_AST(EXPR, PN_TUP(m)) }
    | c:calllist		{ $$ = PN_AST(EXPR, c) }
    | c:call e:expr 		{ $$ = PN_AST(EXPR, PN_PUSH(PN_TUPIF(PN_S(e,0)),
                                                            PN_TUPLE_AT(c,0))); }
    | c:call l:listexprs 	{ $$ = PN_SHIFT(PN_S(l,0));
            if (!PN_S(l, 0)) { PN_SRC(c)->a[1] = PN_SRC($$); }
            $$ = PN_PUSH(PN_TUP($$), c); }
    | e:opexpr			{ $$ = e }
    | c:call			{ $$ = PN_AST(EXPR, c) }
    | e:eatom

eatom = e:atom                  { $$ = PN_AST(EXPR, PN_TUPIF(e)) }

opexpr = not e:expr		{ $$ = PN_AST(NOT, e) }
    | bitnot e:expr		{ $$ = PN_AST(WAVY, e) }
    | minus  e:expr		{ $$ = PN_OP(AST_MINUS, PN_AST(VALUE, PN_ZERO), e) }
    | l:eatom times !times r:eatom { $$ = PN_OP(AST_TIMES, l, r) }
    | l:eatom div   !div r:eatom   { $$ = PN_OP(AST_DIV,  l, r) }
    | l:eatom minus !minus r:eatom { $$ = PN_OP(AST_MINUS, l, r) }
    | l:eatom plus !plus r:eatom   { $$ = PN_OP(AST_PLUS,  l, r) }
    | mminus e:mvalue		{ $$ = PN_OP(AST_INC, e, PN_NUM(-1) ^ 1) }
    | pplus e:mvalue		{ $$ = PN_OP(AST_INC, e, PN_NUM(1) ^ 1) }
    | e:mvalue (pplus		{ $$ = PN_OP(AST_INC, e, PN_NUM(1)) }
             | mminus		{ $$ = PN_OP(AST_INC, e, PN_NUM(-1)) }) {}

atom = e:value | e:list | e:anonsub

special = < ( "foreach"|"for"|"while"|"class"|"if"|"elseif" ) > - { $$ = PN_AST(MSG, PN_STRN(yytext, yyleng)) }

#FIXME methods and indirect methods:
#   chr 101  => (expr (value (101), msg ("chr")))
#   chr(101,1) => (expr (value (101), msg ("chr") list (value 1)))
#   print chr 101 => (expr (value (101), msg ("chr"), msg ("print")))
#   obj->meth(args) => (expr (msg obj), msg (meth) list (expr args))
#TODO: if (cond) {block} => expr (if, cond, block)
# callexprs allows assignment for named args
calllist = m:name - list-start - list-end
           { PN_SRC(m)->a[1] = PN_SRC(PN_AST(LIST, PN_NIL)); $$ = PN_TUP(m) }
         | m:name - l:list -
           { PN_SRC(m)->a[1] = PN_SRC(l); $$ = PN_TUP(m) }
         | m:name - list-start l:callexprs list-end -
           { PN_SRC(m)->a[1] = PN_SRC(PN_AST(LIST, l)); $$ = PN_TUP(m) }
call = m:name - { $$ = PN_TUP(m) }
method = v:methlhs - arrow m:name - l:list -
         { PN_SRC(m)->a[1] = PN_SRC(l); $$ = PN_PUSH(PN_TUPIF(v), m) }
       | v:methlhs - arrow m:name -
         { $$ = PN_PUSH(PN_TUPIF(v), m) }

name = !keyword m:id -      { $$ = PN_AST(MSG, m) }
     | !keyword m:funcvar - { $$ = PN_AST(MSG, m) }

#listref-items = i1:listref-item     { $$ = i1 = PN_TUP(i1) }
#            (sep i2:listref-item { $$ = i1 = PN_PUSH(i1, i2) })*
#             sep?
#           | ''               { $$ = PN_NIL }
#
# TODO: unquoted lists
#listref-item = m:msg t:list v:loose { $$ = PN_AST3(LICK, m, v, t) }
#          | m:msg t:list { $$ = PN_AST3(LICK, m, PN_NIL, t) }
#          | m:msg v:loose t:list { $$ = PN_AST3(LICK, m, v, t) }
#          | m:msg v:loose { $$ = PN_AST2(LICK, m, v) }
#          | m:msg         { $$ = PN_AST(LICK, m) }

hash-item = k:value - (fatcomma|comma) v:atom { $$ = PN_AST2(ASSIGN, k, v) }
          | k:unquoted - fatcomma v:atom      { $$ = PN_AST2(ASSIGN, PN_AST(VALUE, k), v) }
hash-items = i1:hash-item      { $$ = i1 = PN_TUP(i1) }
            (sep i2:hash-item  { $$ = i1 = PN_PUSH(i1, i2) })*
             sep?
           | ''                { $$ = PN_NIL }

#loose = value
#      | v:unquoted { $$ = PN_AST(VALUE, v) }
#
# anonymous sub, w or w/o proto (aka list)
#sub = SUB n:arg-name - t:list? b:block       { $$ = PN_AST2(ASSIGN, n, PN_AST2(PROTO, t, b)) }
block = block-start s:statements - block-end  { $$ = PN_AST(BLOCK, s) }
list = list-start s:listexprs - list-end      { $$ = PN_AST(LIST, s) }
     | list-start list-end                    { $$ = PN_AST(LIST, PN_NIL) }
listref = listref-start s:listexprs - listref-end { $$ = PN_AST(LIST, s) }
     | listref-start listref-end              { $$ = PN_AST(LIST, PN_NIL) }
hash = hash-start h:hash-items - hash-end     { $$ = PN_AST(LIST, h) }
     | hash-start hash-end                    { $$ = PN_AST(LIST, PN_NIL) }

#path = '/' < utfw+ > - { $$ = PN_STRN(yytext, yyleng) }
#path    = < utfw+ > -  { $$ = PN_STRN(yytext, yyleng) }
#msg = < utfw+ > -   	{ $$ = PN_STRN(yytext, yyleng) }

mvalue = i:immed - { $$ = PN_AST(VALUE, i) }
       | global

methlhs = global
        | name

value = i:immed - { $$ = PN_AST(VALUE, i) }
      | global
      | listref
      | hash

immed = undef { $$ = PN_NIL }
#      | true  { $$ = PN_TRUE }
#      | false { $$ = PN_FALSE }
      | hex   { $$ = PN_NUM(PN_ATOI(yytext, yyleng, 16)) }
      | dec   { $$ = ($$ == YY_TNUM) ? PN_NUM(PN_ATOI(yytext, yyleng, 10))
                   : potion_decimal(P, yytext, yyleng) }
      | str1 | str2

lexglobal = MY t:name i:global { PN_SRC(i)->a[2] = PN_SRC(t); $$ = i }
          | MY i:global        { $$ = i }
          | i:global

global  = scalar | listvar | hashvar | listel | hashel | funcvar | globvar
# send the value a msg, every global is a closure (see name)
scalar  = < '$' i:id > - !'[' !'{'
        { $$ = PN_AST(MSG, PN_STRCAT("$", PN_STR_PTR(i))) }
listvar = < '@' i:id > - { $$ = PN_AST(MSG, PN_STRCAT("@", PN_STR_PTR(i))) }
hashvar = < '%' i:id > - { $$ = PN_AST(MSG, PN_STRCAT("%", PN_STR_PTR(i))) }
funcvar = < '&' i:id > - { $$ = PN_AST(MSG, PN_STRCAT("&", PN_STR_PTR(i))) }
globvar = < '*' i:id > - { $$ = PN_AST(MSG, PN_STRCAT("*", PN_STR_PTR(i))) }
listel  = < '$' l:id - '[' - i:value - ']' > -
        { $$ = PN_AST2(MSG, PN_STRCAT("@", PN_STR_PTR(l)),
                            PN_AST(LIST, PN_TUP(i))) }
hashel  = < '$' h:id - '{' - k:value - '}' > -
        { $$ = PN_AST2(MSG, PN_STRCAT("%", PN_STR_PTR(h)),
                            PN_AST(LIST, PN_TUP(k))) }

semi = ';'
comma = ','
fatcomma = '=>' -
arrow = "->" -
block-start = '{' space*
block-end = semi? space* '}' -
list-start = '(' -
list-end = ')' -
listref-start = '[' -
listref-end = ']' -
hash-start = '{' -
hash-end = '}' -
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
dec = < '-'? ('0' | [1-9][0-9]*) { $$ = YY_TNUM }
        ('.' [0-9]+ { $$ = YY_TDEC })?
        ('e' [-+] [0-9]+ { $$ = YY_TDEC })? >
version = 'v'? < ('0' | [1-9][0-9]*) { $$ = YY_TNUM }
          ('.' [0-9]+ { $$ = YY_TDEC })? >
        { $$ = ($$ == YY_TNUM) ? PN_NUM(PN_ATOI(yytext, yyleng, 10))
                               : PN_STRN(yytext, yyleng) }

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
         | !'#' !',' !'=>' !'{' !'[' !'(' !'}' !']' !')' utf8
unq-sep = sep !'#' !',' !'=>' !'{' !'[' !'('
unquoted = < (!unq-sep !listref-end unq-char)+ > { $$ = PN_STRN(yytext, yyleng) }

# lexer rules which are only printed with -DP, not with -Dp:
- = (space | comment)*
-- = (space | comment | semi)*
sep = semi (space | comment | semi)*
comment	= '#' (!end-of-line utf8)*
# PSXSPC
# \240 U+A0 NO-BREAK SPACE
# \205 U+85 NEL
space = ' ' | '\f' | '\v' | '\t' | '\205' | '\240' | end-of-line
end-of-line = ( '\r\n' | '\n' | '\r' )
  { ++G->lineno; if (P->flags & EXEC_DEBUG) { P->line = yylastline(G, thunk->begin); }}
end-of-file = !'\0'
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

utf8 = [\t\40-\176]
     | [\302-\337] [\200-\277]
     | [\340-\357] [\200-\277] [\200-\277]
     | [\360-\364] [\200-\277] [\200-\277] [\200-\277]
     | end-of-line

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
                        { SRC_TPL3(n,PN_ZERO,m) }
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
     | m:arg-modifier n:arg2-name 		 	{ SRC_TPL3(n,PN_ZERO,m) }
     | n:arg2-name - assign d:value			{ SRC_TPL3(n,PN_NUM(':'), PN_S(d,0)) }
     | n:arg2-name					{ SRC_TPL1(n) }

%%

PN p2_parse(Potion *P, PN code, char *filename) {
  GREG *G = YY_NAME(parse_new)(P);
  int oldyypos = P->yypos;
  PN oldinput = P->input;
  PN oldsource = P->source;
  P->yypos = 0;
  P->input = code;
  P->source = PN_NIL;
  P->pbuf = potion_asm_new(P);
#ifdef DEBUG
  yydebug = P->flags;
#endif

  G->filename = filename;
  P->fileno = PN_PUT(pn_filenames, PN_STR(filename));
  if (!YY_NAME(parse)(G)) {
    YY_ERROR("** Syntax error");
    fprintf(stderr, "%s", PN_STR_PTR(code));
  }
  YY_NAME(parse_free)(G);

  code = P->source;
  P->source = oldsource;
  P->yypos = oldyypos;
  P->input = oldinput;
  return code;
}

// duplicate but still needed to compile internal methods
PN potion_sig(Potion *P, char *fmt) {
  PN out = PN_NIL;
  if (fmt == NULL) return PN_NIL;
  if (fmt[0] == '\0') return PN_FALSE;

  GREG *G = YY_NAME(parse_new)(P);
  int oldyypos = P->yypos;
  PN oldinput = P->input;
  PN oldsource = P->source;
  P->yypos = 0;
  P->input = potion_byte_str(P, fmt);
  P->source = out = PN_TUP0();
  P->pbuf = NULL;
#ifdef DEBUG
  yydebug = P->flags;
#endif

  if (!YY_NAME(parse_from)(G, yy_sig))
    YY_ERROR("** Signature syntax error");
  YY_NAME(parse_free)(G);

  out = P->source;
  P->source = oldsource;
  P->yypos = oldyypos;
  P->input = oldinput;
  return out;
}

PN p2_sig(Potion *P, char *fmt) {
  PN out = PN_NIL;
  if (fmt == NULL) return PN_NIL; // no signature, arg check off
  if (fmt[0] == '\0') return PN_FALSE; // empty signature, no args

  GREG *G = YY_NAME(parse_new)(P);
  int oldyypos = P->yypos;
  PN oldinput = P->input;
  PN oldsource = P->source;
  P->yypos = 0;
  P->input = potion_byte_str(P, fmt);
  P->source = out = PN_TUP0();
  P->pbuf = NULL;
#ifdef DEBUG
  yydebug = P->flags;
#endif

  if (!YY_NAME(parse_from)(G, yy_sig_p5))
    YY_ERROR("** Signature syntax error");
  YY_NAME(parse_free)(G);

  out = P->source;
  P->source = oldsource;
  P->yypos = oldyypos;
  P->input = oldinput;
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
    if (PN_IS_NUM(v)) idx++;
    else if (i < PN_TUPLE_LEN(sig) && PN_IS_STR(PN_TUPLE_AT(sig, i+1)))
      idx++;
  });

  return -1;
}

/** look back in the line for the prev. \n and back forth for the next \n
  */
static PN yylastline(struct _GREG *G, int pos) {
  char *c, *nl, *s = G->buf;
  int i, l;
  for (i=pos-1; i>=0 && (*(s+i) != 10); i--);
  if (i) nl = s+i+1; else nl = s;
  c = strchr(nl, 10);
  l = c ? c - nl : s + pos - nl;
  return l ? potion_byte_str2(G->data, nl, l) : PN_NIL;
}
