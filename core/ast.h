//
// ast.h
// the ast for Potion code in-memory
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#ifndef POTION_AST_H
#define POTION_AST_H

typedef struct {
  PN v;
  PN b;
} PNArg;

enum PN_AST {
  AST_CODE,
  AST_VALUE,
  AST_ASSIGN,
  AST_NOT,
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
  AST_WAVY,
  AST_BITL,
  AST_BITR,
  AST_PLUS,
  AST_MINUS,
  AST_INC,
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
  AST_LICK,
  AST_PROTO
};

#define PN_TOK_MISSING 0x10000

#define PN_AST(T, A)  potion_source(P, AST_##T, A, PN_NIL, PN_NIL)
#define PN_AST2(T, A, B)  potion_source(P, AST_##T, A, B, PN_NIL)
#define PN_OP(T, A, B)    potion_source(P, T, A, B, PN_NIL)
#define PN_AST3(T, A, B, C)  potion_source(P, AST_##T, A, B, C)
#define PN_PART(S)    ((struct PNSource *)S)->part
#define PN_S(S, N)    ((struct PNSource *)S)->a[N]
#define PN_CLOSE(B) ({ \
    PN endname = B; \
    if (PN_IS_TUPLE(endname)) endname = PN_TUPLE_AT(endname, 0); \
    if (endname != PN_NIL) { \
      if (PN_PART(endname) == AST_EXPR) endname = PN_TUPLE_AT(PN_S(endname, 0), 0); \
      if (PN_PART(endname) == AST_MESSAGE || PN_PART(endname) == AST_PATH) \
        endname = PN_S(endname, 0); \
      if (P->unclosed == endname) { P->unclosed = PN_NIL; } \
    } \
  })

PN potion_source(Potion *, u8, PN, PN, PN);

#endif
