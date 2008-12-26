//
// pn-ast.h
// the ast for Potion code in-memory
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#ifndef POTION_AST_H
#define POTION_AST_H

typedef unsigned char PNByte;

enum PN_AST {
  AST_CODE,
  AST_VALUE,
  AST_TABLE,
  AST_MESSAGE
};

#endif
