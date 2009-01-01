//
// pn-gram.y
// the parser for Potion
//
// (c) 2008 why the lucky stiff, the freelance professor
//
%include {
#define POTION_GRAMMAR_C
#include <assert.h>
#include <string.h>
#include "potion.h"
#include "internal.h"
#include "pn-ast.h"
}
%extra_argument { Potion *P }
%token_type { PN }
%type arg { PNArg }
%token_prefix PN_TOK_
%token_destructor { if (potion_is_ref($$)) { P->xast++; } }
%parse_accept { printf("-- LEMON END --\n"); }
%parse_failure { printf("-- LEMON FAIL --\n"); }
%name LemonPotion

%left  OR AND.
%right ASSIGN.
%nonassoc CMP EQ NEQ.
%left  GT GTE LT LTE. 
%left  PIPE CARET.
%left  AMP.
%left  BITL BITR.
%left  PLUS MINUS.
%left  TIMES DIV REM.
%right POW.
// %right NOT WAVY.

potion(A) ::= all(B). { A = P->source = PN_AST(CODE, B); }

all(A) ::= statements(B). { A = B; }
all(A) ::= statements(B) SEP. { A = B; }

statements(A) ::= statements(B) SEP statement(C). { A = PN_PUSH(B, PN_AST(EXPR, C)); }
statements(A) ::= statement(B). { A = PN_TUP(PN_AST(EXPR, B)); }
statements(A) ::= name(B) ASSIGN statement(C). { A = PN_TUP(PN_AST2(ASSIGN, B, C)); }
statements(A) ::= statements(B) SEP name(C) ASSIGN statement(D).
{ A = PN_PUSH(B, PN_AST2(ASSIGN, C, D)); }
statements(A) ::= statement(B) op(C) statement(D). { A = PN_TUP(PN_OP(C, PN_AST(EXPR, B), PN_AST(EXPR, D))); }

statement(A) ::= statement(B) call(C). { A = PN_PUSH(B, C); }
statement(A) ::= arg(B). { A = PN_TUP(B.b == PN_NIL ? B.v : PN_AST2(PROTO, B.v, B.b)); }
statement(A) ::= call(B). { A = PN_TUP(B); }

op(A) ::= OR. { A = AST_OR; }
op(A) ::= AND. { A = AST_AND; }
op(A) ::= CMP. { A = AST_CMP; }
op(A) ::= EQ. { A = AST_EQ; }
op(A) ::= NEQ. { A = AST_NEQ; }
op(A) ::= GT. { A = AST_GT; }
op(A) ::= GTE. { A = AST_GTE; }
op(A) ::= LT. { A = AST_LT; }
op(A) ::= LTE. { A = AST_LTE; }
op(A) ::= PIPE. { A = AST_PIPE; }
op(A) ::= CARET. { A = AST_CARET; }
op(A) ::= AMP. { A = AST_AMP; }
op(A) ::= BITL. { A = AST_BITL; }
op(A) ::= BITR. { A = AST_BITR; }
op(A) ::= PLUS. { A = AST_PLUS; }
op(A) ::= MINUS. { A = AST_MINUS; }
op(A) ::= TIMES. { A = AST_TIMES; }
op(A) ::= DIV. { A = AST_DIV; }
op(A) ::= REM. { A = AST_REM; }
op(A) ::= POW. { A = AST_POW; }

call(A) ::= name(B). { A = B; }
call(A) ::= name(B) arg(C). { PN_S(B, 1, C.v); PN_S(B, 2, C.b); A = B; }

arg(A) ::= table(B) block(C). { A.v = B; A.b = C; }
arg(A) ::= value(B) block(C). { A.v = B; A.b = C; }
arg(A) ::= table(B). { A.v = B; A.b = PN_NIL; }
arg(A) ::= value(B). { A.v = B; A.b = PN_NIL; }
arg(A) ::= block(B). { A.v = PN_EMPTY; A.b = B; }

name(A) ::= OPS(B). { A = PN_AST(MESSAGE, B); }
name(A) ::= MESSAGE(B). { A = PN_AST(MESSAGE, B); }
name(A) ::= QUERY(B). { A = PN_AST(QUERY, B); }
name(A) ::= PATH(B). { A = PN_AST(PATH, B); }
name(A) ::= PATHQ(B). { A = PN_AST(PATHQ, B); }

value(A) ::= NIL(B). { A = PN_AST(VALUE, B); }
value(A) ::= TRUE(B). { A = PN_AST(VALUE, B); }
value(A) ::= FALSE(B). { A = PN_AST(VALUE, B); }
value(A) ::= INT(B). { A = PN_AST(VALUE, B); }
value(A) ::= FLOAT(B). { A = PN_AST(VALUE, B); }
value(A) ::= STRING(B). { A = PN_AST(VALUE, B); }
value(A) ::= STRING2(B). { A = PN_AST(VALUE, B); }
value(A) ::= data(B). { A = B; }

block(A) ::= BEGIN_BLOCK statements(B) END_BLOCK. { A = PN_AST(BLOCK, B); }
block(A) ::= BEGIN_BLOCK END_BLOCK. { A = PN_AST(BLOCK, PN_EMPTY); }

table(A) ::= BEGIN_TABLE statements(B) END_TABLE. { A = PN_AST(TABLE, B); }
table(A) ::= BEGIN_TABLE END_TABLE. { A = PN_AST(TABLE, PN_EMPTY); }

//
// the interleaved data language
//
data(A) ::= BEGIN_DATA items(B) END_DATA. { A = PN_AST(DATA, B); }
data(A) ::= BEGIN_DATA END_DATA. { A = PN_AST(DATA, PN_EMPTY); }

items(A) ::= items(B) SEP item(C). { A = PN_PUSH(B, C); }
items(A) ::= item(B). { A = PN_TUP(B); }

item(A) ::= name(B). { A = PN_TUP(B); }
item(A) ::= name(B) value(C). { A = PN_PUSH(PN_TUP(B), C); }
item(A) ::= name(B) value(C) table(D). { A = PN_PUSH(PN_PUSH(PN_TUP(B), C), D); }
item(A) ::= name(B) table(C). { A = PN_PUSH(PN_TUP(B), C); }
item(A) ::= name(B) table(C) value(D). { A = PN_PUSH(PN_PUSH(PN_TUP(B), C), D); }
item(A) ::= value(B). { A = PN_TUP(B); }
item(A) ::= value(B) table(C). { A = PN_PUSH(PN_TUP(B), C); }
item(A) ::= table(B). { A = PN_TUP(B); }
item(A) ::= table(B) value(C). { A = PN_PUSH(PN_TUP(B), C); }
