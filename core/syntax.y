# -*- mode: antlr; tab-width:8 -*-
#
# syntax.g
# Potion tokens and grammar
#
# (c) 2009 _why
#

%{
#include "potion.h"
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

#define YY_NAME(N) potion_code_##N

#define YY_TINT 3
#define YY_TDBL 13
#ifdef DEBUG
# define YYDEBUG_PARSE   DEBUG_PARSE
# define YYDEBUG_VERBOSE DEBUG_PARSE_VERBOSE
# define YY_SET(G, text, count, thunk, P) \
  yyprintf((stderr, "%s %d %p:<%s>\n", thunk->name, count,(void*)yy,\
    PN_IS_INT(yy)||PN_IS_PTR(yy) ? PN_STR_PTR(potion_send(yy, PN_string)) : "")); \
  G->val[count]= yy;
#endif

#define SRC_TPL1(x)       P->source = PN_PUSH(P->source, x)
#define SRC_TPL2(x,y)     P->source = PN_PUSH(PN_PUSH(P->source, x), y)
#define SRC_TPL3(x,y,z)   P->source = PN_PUSH(PN_PUSH(PN_PUSH(P->source, x), y), z)

static PN yylastline(struct _GREG *G, int pos);
%}

potion = -- s:statements end-of-file { $$ = P->source = PN_AST(CODE, s) }

statements = s1:stmt { $$ = s1 = PN_TUP(s1) }
        (sep s2:stmt { $$ = s1 = PN_PUSH(s1, s2) })*
         sep?
     | ''            { $$ = PN_NIL; }

stmt = s:sets
       ( or x:sets          { s = PN_OP(AST_OR, s, x) }
       | and x:sets         { s = PN_OP(AST_AND, s, x) })*
       { $$ = s; }

sets = e:eqs
       ( assign s:sets       { e = PN_AST2(ASSIGN, e, s) }
       | defassign s:value   { e = PN_AST2(ASSIGN, e, s) }
       | or assign s:sets    { e = PN_AST2(ASSIGN, e, PN_OP(AST_OR, e, s)) }
       | and assign s:sets   { e = PN_AST2(ASSIGN, e, PN_OP(AST_AND, e, s)) }
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
       { $$ = e; }

eqs = c:cmps
      ( cmp x:cmps          { c = PN_OP(AST_CMP, c, x) }
      | eq x:cmps           { c = PN_OP(AST_EQ, c, x) }
      | neq x:cmps          { c = PN_OP(AST_NEQ, c, x) })*
      { $$ = c; }

cmps = o:bitors
       ( gte x:bitors        { o = PN_OP(AST_GTE, o, x) }
       | gt x:bitors         { o = PN_OP(AST_GT, o, x) }
       | lte x:bitors        { o = PN_OP(AST_LTE, o, x) }
       | lt x:bitors         { o = PN_OP(AST_LT, o, x) })*
       { $$ = o; }

bitors = a:bitand
         ( pipe x:bitand       { a = PN_OP(AST_PIPE, a, x) }
         | caret x:bitand      { a = PN_OP(AST_CARET, a, x) })*
         { $$ = a; }

bitand = b:bitshift
         ( amp x:bitshift      { b = PN_OP(AST_AMP, b, x) })*
         { $$ = b; }

bitshift = s:sum
           ( bitl x:sum          { s = PN_OP(AST_BITL, s, x) }
           | bitr x:sum          { s = PN_OP(AST_BITR, s, x) })*
           { $$ = s; }

sum = p:product
      ( plus x:product      { p = PN_OP(AST_PLUS, p, x) }
      | minus x:product     { p = PN_OP(AST_MINUS, p, x) })*
      { $$ = p; }

product = p:power
          ( times x:power           { p = PN_OP(AST_TIMES, p, x) }
          | div x:power             { p = PN_OP(AST_DIV, p, x) }
          | rem x:power             { p = PN_OP(AST_REM, p, x) })*
          { $$ = p; }

power = e:expr
        ( pow x:expr { e = PN_OP(AST_POW, e, x) })*
        { $$ = e; }

expr = ( not a:expr           { a = PN_AST(NOT, a) }
       | wavy a:expr          { a = PN_AST(WAVY, a) }
       | minus !minus a:atom  { a = PN_OP(AST_MINUS, PN_AST(VALUE, PN_ZERO), a) }
       | plus !plus a:atom    { a = PN_OP(AST_PLUS, PN_AST(VALUE, PN_ZERO), a) }
       | mminus a:atom        { a = PN_OP(AST_INC, a, PN_NUM(-1) ^ 1) }
       | pplus a:atom         { a = PN_OP(AST_INC, a, PN_NUM(1) ^ 1) }
       | a:atom (pplus          { a = PN_OP(AST_INC, a, PN_NUM(1)) }
               | mminus         { a = PN_OP(AST_INC, a, PN_NUM(-1)) })?) { a = PN_TUP(a) }
         (c:call { a = PN_PUSH(a, c) })*
       { $$ = PN_AST(EXPR, a) }

atom = e:value | e:closure | e:list | e:extern | e:call

call = (n:name { v = PN_NIL; b = PN_NIL; } (v:value | v:list)? (b:block | b:closure)? |
       (v:value | v:list) { n = PN_AST(MSG, PN_NIL); b = PN_NIL; } b:block?)
         { $$ = n; PN_SRC(n)->a[1] = PN_SRC(v); PN_SRC(n)->a[2] = PN_SRC(b) }

extern = "extern" - n:name list-start { P->source = PN_TUP0() } l:arg-list list-end
         { $$ = PN_AST2(MSG, PN_extern, PN_PUSH(PN_TUP(n), P->source)) }
       | "extern" - n:name
         { $$ = PN_AST2(MSG, PN_extern, PN_TUP(n)) }

name = p:path           { $$ = PN_AST(PATH, p) }
     | quiz ( m:msg -   { $$ = PN_AST(QUERY, m) }
            | p:path    { $$ = PN_AST(PATHQ, p) })
     | t:tuple -        { $$ = PN_AST(VALUE, t) } # like a lick, but only a arrayref
     | !keyword
       m:msg -          { $$ = PN_AST(MSG, m) }

lick-items = i1:lick-item     { $$ = i1 = PN_TUP(i1) }
            (sep i2:lick-item { $$ = i1 = PN_PUSH(i1, i2) })*
             sep?
           | ''               { $$ = PN_NIL; }

lick-item = m:msg - t:list v:loose { $$ = PN_AST3(LICK, m, v, t) }
          | m:msg - t:list  { $$ = PN_AST3(LICK, m, PN_NIL, t) }
          | m:msg - v:loose t:list { $$ = PN_AST3(LICK, m, v, t) }
          | m:msg - v:loose { $$ = PN_AST2(LICK, m, v) }
          | m:msg -         { $$ = PN_AST(LICK, m) }
          | v:loose -       { $$ = PN_AST(LICK, v) }

loose = value
      | v:unquoted { $$ = PN_AST(VALUE, v) }

closure = t:list? b:block { $$ = PN_AST2(PROTO, t, b) }
list  = list-start s:statements list-end   { $$ = PN_AST(LIST, s) }
block = block-start s:statements block-end { $$ = PN_AST(BLOCK, s) }
lick  = lick-start i:lick-items lick-end   { $$ = PN_AST(LIST, i) }
group = group-start s:statements group-end { $$ = PN_AST(EXPR, s) }
tuple = m:msg l:lick  { $$ = PN_AST2(LICK, PN_AST(MSG, m), l) }

path = '/' < utff utfw* > -    { $$ = PN_STRN(yytext, yyleng); }
msg  =
       < utff ( utfw | [.:] )* utfw+ '?'? > { $$ = PN_STRN(yytext, yyleng) }
     | < utff utfw* '?'? >   { $$ = PN_STRN(yytext, yyleng) }

value = i:immed - { $$ = PN_AST(VALUE, i) }
      | t:tuple - { $$ = PN_AST(VALUE, t) } # arrayref
      | lick
      | group

immed = nil   { $$ = PN_NIL; }
      | true  { $$ = PN_TRUE; }
      | false { $$ = PN_FALSE; }
      | hex   { $$ = PN_NUM(PN_ATOI(yytext, yyleng, 16)) }
      | dec   { if ($$ == YY_TINT) {
                  $$ = PN_NUM(PN_ATOI(yytext, yyleng, 10));
                } else {
                  $$ = potion_strtod(P, yytext, yyleng);
              } }
      | str1 | str2

utff = [A-Za-z_$@]
     | '\304' [\250-\277]
     | [\305-\337] [\200-\277]
     | [\340-\357] [\200-\277] [\200-\277]
     | [\360-\364] [\200-\277] [\200-\277] [\200-\277]
utfw = [A-Za-z0-9_$@;`{}]
     | '\304' [\250-\277]
     | [\305-\337] [\200-\277]
     | [\340-\357] [\200-\277] [\200-\277]
     | [\360-\364] [\200-\277] [\200-\277] [\200-\277]
utf8 = [\t\40-\176]
     | [\302-\337] [\200-\277]
     | [\340-\357] [\200-\277] [\200-\277]
     | [\360-\364] [\200-\277] [\200-\277] [\200-\277]
     | end-of-line

comma = ','
block-start = ':' --
block-end = '.' -
list-start = '(' --
list-end = ')' -
lick-start = '[' --
lick-end = ']' -
group-start = '|' --
group-end = '.' -
quiz = '?' --
assign = '=' --
defassign = ":=" --
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
and = ("&&" | "and" !utfw) --
or = ("||" | "or" !utfw) --
not = ("!" | "not" !utfw) --
keyword = ("and" | "or" | "not") !utfw

nil = "nil" !utfw
true = "true" !utfw
false = "false" !utfw
hexl = [0-9A-Fa-f]
hex = '0x' < hexl+ >
# wrong x-1 parsing precedence, whitespace #75
dec = < ('0' | '-'? [1-9][0-9]*) { $$ = YY_TINT; }
        ('.' [0-9]+ { $$ = YY_TDBL; })?
        ('e' [-+] [0-9]+ { $$ = YY_TDBL })? >

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
  if (code < 0x80) {          // 0xxxxxxx
    utfc[nbuf++] = code;
  } else if (code < 0x7ff) {  // 110xxxxx 10xxxxxx
    if (code == 0xC0 || code == 0xC1)
      YY_ERROR("Invalid utf-8 unicode character (U+C0,U+C1)");
    utfc[nbuf++] = (code >> 6) | 0xc0;
    utfc[nbuf++] = (code & 0x3f) | 0x80;
  } else { // 1110xxxx 10xxxxxx 10xxxxxx
    if (code >= 0xD800 && code < 0xDFFF) // utf-16 surrogate halves RFC 3629
      YY_ERROR("Invalid utf-8 unicode character (U+D800-U+DFFF)");
    utfc[nbuf++] = (code >> 12) | 0xe0;
    utfc[nbuf++] = ((code >> 6) & 0x3f) | 0x80;
    utfc[nbuf++] = (code & 0x3f) | 0x80;
  }
  P->pbuf = potion_asm_write(P, P->pbuf, utfc, nbuf);
}
escU       = esc 'U' '{' < hexl+ > '}' {
  int nbuf = 0;
  char utfc[4] = {0, 0, 0, 0};
  unsigned long code = PN_ATOI(yytext, yyleng, 16);
  if (code < 0x80) {          // 0xxxxxxx
    utfc[nbuf++] = code;
  } else if (code < 0x7ff) {  // 110xxxxx 10xxxxxx
    if (code == 0xC0 || code == 0xC1)
      YY_ERROR("Invalid utf-8 unicode character (U+C0,U+C1)");
    utfc[nbuf++] = (code >> 6) | 0xc0;
    utfc[nbuf++] = (code & 0x3f) | 0x80;
  } else if (code < 0xffff) { // 1110xxxx 10xxxxxx 10xxxxxx
    if (code >= 0xD800 && code < 0xDFFF) // utf-16 surrogate halves RFC 3629
      YY_ERROR("Invalid utf-8 unicode character (U+D800-U+DFFF)");
    utfc[nbuf++] = (code >> 12) | 0xe0;
    utfc[nbuf++] = ((code >> 6) & 0x3f) | 0x80;
    utfc[nbuf++] = (code & 0x3f) | 0x80;
  } else if (code < 0x10ffff) { // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    utfc[nbuf++] = (code >> 18) | 0xf0;
    utfc[nbuf++] = ((code >> 12) & 0x3f) | 0x80;
    utfc[nbuf++] = ((code >> 6) & 0x3f) | 0x80;
    utfc[nbuf++] = (code & 0x3f) | 0x80;
  } else {
    YY_ERROR("Invalid utf-8 unicode character (> U+10FFFF)");
  }
  P->pbuf = potion_asm_write(P, P->pbuf, utfc, nbuf);
}
escc = esc < utf8 > { P->pbuf = potion_asm_write(P, P->pbuf, yytext, yyleng) }

q2 = ["]
e2 = '\\' ["] { P->pbuf = potion_asm_write(P, P->pbuf, "\"", 1) }
c2 = < (!q2 !esc utf8)+ > { P->pbuf = potion_asm_write(P, P->pbuf, yytext, yyleng) }
str2 = q2 { P->pbuf = potion_asm_clear(P, P->pbuf) }
       < (e2 | escn | escb | escf | escr | esct | escu | escU | escc | c2)* >
       q2 { $$ = potion_bytes_string(P, PN_NIL, (PN)P->pbuf) }

unq-char = '{' unq-char+ '}'
         | '[' unq-char+ ']'
         | '(' unq-char+ ')'
         | !'{' !'[' !'(' !'}' !']' !')' utf8
unq-sep = sep !'{' !'[' !'('
unquoted = < (!unq-sep !lick-end unq-char)+ > { $$ = PN_STRN(yytext, yyleng); }

- = (space | comment)*
-- = (space | comment | end-of-line)*
sep = (end-of-line | comma) (space | comment | end-of-line | comma)*
comment	= '#' (!end-of-line utf8)*
space = ' ' | '\f' | '\v' | '\t'
end-of-line = ( '\r\n' | '\n' | '\r' )
  { ++G->lineno; P->line = yylastline(G, thunk->begin); }
end-of-file = !.

sig = args+ end-of-file
args = arg-list (arg-sep arg-list)*
arg-list = arg-set (optional arg-set)?
         | optional arg-set
arg-set = arg (comma - arg)*

arg-name = < utff utfw* > -    { $$ = PN_STRN(yytext, yyleng) }
# not with :=, const '-' would make sense, \ and * not
arg-modifier = < ('-' | '\\' | '*' ) >  { $$ = PN_NUM(yytext[0]); }
# for FFIs, map to potion and C types. See potion_type_char()
arg-type = < [NBIDS&oTaubnsFPlkftxrcdm] > - { $$ = PN_NUM(yytext[0]) }
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
    | arg-rest
optional = '|' -        { SRC_TPL1(PN_NUM('|')) }
arg-rest = "..." -      { SRC_TPL1(PN_NUM('.')) }
arg-sep = '.' -         { SRC_TPL1(PN_NUM('.')) } #x,y... ignore rest

%%

PN potion_parse(Potion *P, PN code, char *filename) {
  GREG *G = YY_NAME(parse_new)(P);
  int oldyypos = P->yypos;
  PN oldinput = P->input;
  PN oldsource = P->source;
  P->yypos = 0;
  P->input = code;
  P->source = PN_NIL;
  P->parser = (PN)G;
  P->pbuf = potion_asm_new(P);
#ifdef YY_DEBUG
  yydebug = P->flags;
#endif

  G->filename = filename;
  P->fileno = PN_PUT(pn_filenames, PN_STR(filename));

  if (!YY_NAME(parse)(G) || ((struct PNSource *)(P->source))->part != AST_CODE) {
    vPN(Bytes) s = (vPN(Bytes)) code;
    YY_ERROR("** Syntax error");
    if (s->len > 80) s->chars[80] = '\0';
    fprintf(stderr, "%s", s->chars);
    P->source = PN_NIL;
  }
  YY_NAME(parse_free)(G);

  code = P->source;
  P->source = oldsource;
  P->yypos = oldyypos;
  P->input = oldinput;
  return code;
}

/** convert signature string to sig tuple.
  x,y x=N x:=0 |x=o x,y... -x,y

  Currently:
    (name type|modifier default)
    name = PNString - variable name
    type = NUM of potion_type_char, currently used: oNS&Tau
    modifier = NUM of '|' optional, '.' end, ':' default
    \see potion_sig_arity

  TODO:
    ((name,type,attr,default),...)
    attr:
      |     optional
      :=    default value
      -     declare parameter as const, is ro
      \     reference, alias. modifies caller
      *     slurpy list or hash to consume the rest (i.e. &rest from LISP)
    ...   ignore additional params (varargs)
    name: PNString of variable
    type: PNVTable. Accept old type abbrevs: oNS&
          Num, String, Closure, Any, Bool
    attr: ord of first char
    default: any single value, even nil, a closure, tuple, table, lick, ...
    arity: number of any non-default-value strings
 */
PN potion_sig(Potion *P, char *fmt) {
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
  P->parser = (PN)G;
  P->pbuf = NULL;
#ifdef YY_DEBUG
  yydebug = P->flags;
#endif

  if (!YY_NAME(parse_from)(G, yy_sig))
    YY_ERROR("** Syntax error in signature");
  YY_NAME(parse_free)(G);

  out = P->source;
  P->source = oldsource;
  P->yypos = oldyypos;
  P->input = oldinput;
  return out;
}
/** look for name in sig tuple in the given function and return the position.
  */
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
    PN prev = PN_NIL;
    if (v == name) return idx;
    // count names, not string default values
    if (PN_IS_STR(v) && !(PN_IS_INT(prev) && prev == PN_NUM(':')))
      idx++;
    prev = v;
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
