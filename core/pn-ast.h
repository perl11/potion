//
// pn-ast.h
// the ast for Potion code in-memory
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#ifndef POTION_AST_H
#define POTION_AST_H

typedef struct {
  PN v, b;
} PNArg;

enum PN_AST {
  AST_CODE,
  AST_VALUE,
  AST_ASSIGN,
  AST_OR,
  AST_AND,
  AST_CMP,
  AST_EQ,
  AST_NEQ,
  AST_GT,
  AST_GTE,
  AST_LT,
  AST_LTE,
  AST_PIPE,
  AST_CARET,
  AST_AMP,
  AST_BITL,
  AST_BITR,
  AST_PLUS,
  AST_MINUS,
  AST_TIMES,
  AST_DIV,
  AST_REM,
  AST_POW,
  AST_MESSAGE,
  AST_PATH,
  AST_QUERY,
  AST_PATHQ,
  AST_EXPR,
  AST_TABLE,
  AST_BLOCK,
  AST_DATA,
  AST_PROTO
};

struct PNSource {
  PN_OBJECT_HEADER
  u8 part;
  PN a[0];
};

#define PN_AST(T, A)  potion_source(P, AST_##T, A, PN_NIL, PN_NIL)
#define PN_AST2(T, A, B)  potion_source(P, AST_##T, A, B, PN_NIL)
#define PN_OP(T, A, B)    potion_source(P, T, A, B, PN_NIL)
#define PN_AST3(T, A, B, C)  potion_source(P, AST_##T, A, B, C)
#define PN_S(S, N, X) (((struct PNSource *)S)->a[N] = X)

PN potion_source(Potion *, u8, PN, PN, PN);

#endif
