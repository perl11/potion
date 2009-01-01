//
// opcodes.h
// the Potion VM instruction set (heavily based on Lua's)
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#ifndef POTION_OPCODES_H
#define POTION_OPCODES_H

typedef struct {
  u8 code;
  u8 a;
  u8 b;
  u8 c;
} PN_OP;

enum PN_OPCODE {
  OP_NONE,
  OP_MOVE,
  OP_LOADK,
  OP_LOADPN,
  OP_GETLOCAL,
  OP_SETLOCAL,
  OP_GETTABLE,
  OP_SETTABLE,
  OP_GETPATH,
  OP_SETPATH,
  OP_SELF,
  OP_ADD,
  OP_SUB,
  OP_MULT,
  OP_DIV,
  OP_REM,
  OP_POW,
  OP_NOT,
  OP_EQ,
  OP_NEQ,
  OP_LT,
  OP_LTE,
  OP_GT,
  OP_GTE,
  OP_BIND,
  OP_JMP,
  OP_TEST,
  OP_CALL,
  OP_TAILCALL,
  OP_RETURN,
  OP_PROTO
};

#endif
