//
// pn-gram.y
// the parser for Potion
//
// (c) 2008 why the lucky stiff, the freelance professor
//
%include {
#include <assert.h>
#include <string.h>
#include "potion.h"
#include "pn-ast.h"
}
%extra_argument { Potion *P }
%token_type { PN }
%token_prefix PN_TOK_
%token_destructor { if (potion_is_ref($$)) { P->xast++; } }
%parse_accept { printf("-- LEMON END --\n"); }
%parse_failure { printf("-- LEMON FAIL --\n"); }
%name LemonPotion

potion(A) ::= all(B). { A = P->source = PN_AST(CODE, B); }

all(A) ::= statements(B). { A = B; }
all(A) ::= statements(B) SEP. { A = B; }

statements(A) ::= statements(B) SEP statement(C). { A = PN_PUSH(B, PN_AST(EXPR, C)); }
statements(A) ::= statement(B). { A = PN_TUP(PN_AST(EXPR, B)); }
statements(A) ::= name(B) ASSIGN statement(C). { A = PN_TUP(PN_AST(ASSIGN, PN_PUSH(PN_TUP(B), C))); }
statements(A) ::= statements(B) SEP name(C) ASSIGN statement(D).
{ A = PN_PUSH(B, PN_AST(ASSIGN, PN_PUSH(PN_TUP(C), D))); }

statement(A) ::= statement(B) expr(C). { A = PN_PUSH(B, C); }
statement(A) ::= expr(B). { A = PN_TUP(B); }

expr(A) ::= block(B). { A = B; }
expr(A) ::= table(B). { A = B; }
expr(A) ::= OPS(B). { A = PN_AST(MESSAGE, B); }
expr(A) ::= name(B). { A = B; }
expr(A) ::= value(B). { A = B; }

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
