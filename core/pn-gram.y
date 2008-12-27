//
// pn-gram.y
// the parser for Potion
//
// (c) 2008 why the lucky stiff, the freelance professor
//
%include {
#include <assert.h>
#include "potion.h"
#include "pn-ast.h"
}
%extra_argument { Potion *P }
%token_type { PN }
%token_prefix PN_TOK_
%parse_accept { printf("-- LEMON END --\n"); }
%parse_failure { printf("-- LEMON FAIL --\n"); }
%name LemonPotion

potion(A) ::= statements(B). {
  A = PN_AST(CODE, B);
  potion_send(A, PN_inspect);
}

statements(A) ::= statements(B) SEP statement(C). { A = potion_tuple_push(P, B, PN_AST(EXPR, C)); }
statements(A) ::= statement(B). { A = potion_tuple_new(P, PN_AST(EXPR, B)); }
statements(A) ::= expr(B) ASSIGN statement(C). { A = potion_tuple_new(P, PN_AST(ASSIGN, potion_tuple_push(P, potion_tuple_new(P, B), C))); }

statement(A) ::= statement(B) expr(C). { A = potion_tuple_push(P, B, C); }
statement(A) ::= expr(B). { A = potion_tuple_new(P, B); }

expr(A) ::= MESSAGE(B). { A = PN_AST(MESSAGE, B); }
expr(A) ::= QUERY(B). { A = PN_AST(QUERY, B); }
expr(A) ::= block(B). { A = B; }
expr(A) ::= table(B). { A = B; }
expr(A) ::= data(B). { A = B; }
expr(A) ::= OPS(B). { A = PN_AST(MESSAGE, B); }
expr(A) ::= value(B). { A = B; }

value(A) ::= PATH(B). { A = PN_AST(PATH, B); }
value(A) ::= NIL(B). { A = PN_AST(VALUE, B); }
value(A) ::= TRUE(B). { A = PN_AST(VALUE, B); }
value(A) ::= FALSE(B). { A = PN_AST(VALUE, B); }
value(A) ::= INT(B). { A = PN_AST(VALUE, B); }
value(A) ::= FLOAT(B). { A = PN_AST(VALUE, B); }
value(A) ::= STRING(B). { A = PN_AST(VALUE, B); }
value(A) ::= STRING2(B). { A = PN_AST(VALUE, B); }

block(A) ::= BEGIN_BLOCK statements(B) END_BLOCK. { A = PN_AST(BLOCK, B); }
block(A) ::= BEGIN_BLOCK END_BLOCK. { A = PN_AST(BLOCK, PN_EMPTY); }

table(A) ::= BEGIN_TABLE statements(B) END_TABLE. { A = PN_AST(TABLE, B); }

//
// the interleaved data language
//
data(A) ::= BEGIN_DATA items(B) END_DATA. { A = PN_AST(DATA, B); }
data(A) ::= BEGIN_DATA END_DATA. { A = PN_AST(DATA, PN_EMPTY); }

items(A) ::= item(B) SEP item(C). { A = potion_tuple_push(P, B, C); }
items(A) ::= item(B). { A = potion_tuple_new(P, B); }

item(A) ::= MESSAGE(B). { A = B; }
item(A) ::= MESSAGE(B) value. { A = B; }
item(A) ::= MESSAGE(B) value table. { A = B; }
item(A) ::= MESSAGE(B) table. { A = B; }
item(A) ::= MESSAGE(B) table value. { A = B; }
item(A) ::= value(B). { A = B; }
item(A) ::= value(B) table. { A = B; }
item(A) ::= table(B). { A = B; }
item(A) ::= table(B) value. { A = B; }
