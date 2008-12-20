//
// pn-gram.y
// the parser for Potion
//
// (c) 2008 why the lucky stiff, the freelance professor
//
%include { #include <assert.h> }
%extra_argument { char *extra }
%token_type { int }
%token_prefix PN_
%parse_accept { printf("-- LEMON END --\n"); }
%name LemonPotion

potion ::= statements.

statements(A) ::= statements(B) SEP statement. { A = B; }
statements(A) ::= statement(B). { A = B; }

statement(A) ::= statement(B) expr(C). { B = C; A = B; }
statement(A) ::= expr(B) ASSIGN expr(C). { A = B = C; } 
statement(A) ::= expr(B). { A = B; }

expr(A) ::= MESSAGE(B). { A = B; printf("-- LEMON(MESSAGE) --\n"); }
expr(A) ::= QUERY(B). { A = B; printf("-- LEMON(QUERY) --\n"); }
expr(A) ::= block(B). { A = B; printf("-- LEMON(BLOCK) --\n"); }
expr(A) ::= table(B). { A = B; }
expr(A) ::= data(B). { A = B; }
expr(A) ::= OPS(B). { A = B; }
expr(A) ::= value(B). { A = B; }

value(A) ::= PATH(B). { A = B; }
value(A) ::= NIL(B). { A = B; }
value(A) ::= TRUE(B). { A = B; }
value(A) ::= FALSE(B). { A = B; }
value(A) ::= INT(B). { A = B; }
value(A) ::= FLOAT(B). { A = B; }
value(A) ::= STRING(B). { A = B; }
value(A) ::= STRING2(B). { A = B; }

block(A) ::= BEGIN_BLOCK statements(B) END_BLOCK. { A = B; }
block(A) ::= BEGIN_BLOCK END_BLOCK. { A = A; }

table(A) ::= BEGIN_TABLE statements(B) END_TABLE. { A = B; printf("-- LEMON(table) --\n"); }

//
// the interleaved data language
//
data(A) ::= BEGIN_DATA items(B) END_DATA. { A = B; }

items(A) ::= item(B) SEP item. { A = B; }
items(A) ::= item(B). { A = B; }

item(A) ::= MESSAGE(B). { A = B; }
item(A) ::= MESSAGE(B) value. { A = B; }
item(A) ::= MESSAGE(B) value table. { A = B; }
item(A) ::= MESSAGE(B) table. { A = B; }
item(A) ::= MESSAGE(B) table value. { A = B; }
item(A) ::= value(B). { A = B; }
item(A) ::= value(B) table. { A = B; }
item(A) ::= table(B). { A = B; }
item(A) ::= table(B) value. { A = B; }
