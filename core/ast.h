///
/// ast.h
/// the ast for Potion code in-memory
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#ifndef POTION_AST_H
#define POTION_AST_H

/// PNArg - call a function (unused). See now macro PN_S(name,1), PN_S(name,2)
typedef struct {
  PN v; ///< args
  PN b; ///< block
} PNArg;


// PN_AST - tree-types, now in potion.h
//enum PN_AST {
//};

#define PN_TOK_MISSING 0x10000

#define PN_AST(T, A)      potion_source(P, AST_##T, A, PN_NIL, PN_NIL)
#define PN_AST2(T, A, B)  potion_source(P, AST_##T, A, B, PN_NIL)
#define PN_AST3(T, A, B, C)  potion_source(P, AST_##T, A, B, C)
#define PN_OP(T, A, B)    potion_source(P, T, A, B, PN_NIL)
#define PN_TUPIF(T)   PN_IS_TUPLE(T) ? T : PN_TUP(T)
#define PN_SRC(S)     ((struct PNSource *)S)
#define PN_PART(S)    ((struct PNSource *)S)->part
#define PN_S_(S, N)   ((struct PNSource *)S)->a[N] //lvalue
#define PN_S(S, N)    (PN)(((struct PNSource *)S)->a[N])
#define PN_CLOSE(B) ({ \
    PN endname = B; \
    if (PN_IS_TUPLE(endname)) endname = PN_TUPLE_AT(endname, 0); \
    if (endname != PN_NIL) { \
      if (PN_PART(endname) == AST_EXPR) endname = PN_TUPLE_AT(PN_S(endname, 0), 0); \
      if (PN_PART(endname) == AST_MSG || PN_PART(endname) == AST_PATH) \
        endname = PN_S(endname, 0); \
      if (P->unclosed == endname) { P->unclosed = PN_NIL; } \
    } \
  })

PN potion_source(Potion *, u8, PN, PN, PN);

#endif
