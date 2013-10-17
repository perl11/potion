/** \file/ opcodes.h
 the Potion VM instruction set (heavily based on Lua's)

 (c) 2008 why the lucky stiff, the freelance professor */
#ifndef POTION_OPCODES_H
#define POTION_OPCODES_H

#if defined(__GNUC__)
#pragma pack(1)
#else
#pragma pack(push, 1)
#endif

/// PN_OP - a compressed three-address op (as 32bit int bitfield)
/// TODO: expand to 64bit, check jit then
typedef struct {
  u8 code:8; ///< the op. See vm.c http://www.lua.org/doc/jucs05.pdf
  int a:12;  ///< the data (i.e the register)
  int b:12;  ///< optional arg, the message
} PN_OP;

#if defined(__GNUC__)
#pragma pack()
#else
#pragma pack(pop)
#endif

#define PN_OP_AT(asmb, n) ((PN_OP *)((PNFlex *)asmb)->ptr)[n]
#define PN_OP_LEN(asmb)   (PN_FLEX_SIZE(asmb) / sizeof(PN_OP))

enum PN_OPCODE {
  OP_NONE,
  OP_MOVE,
  OP_LOADK,
  OP_LOADPN,
  OP_SELF,
  OP_NEWTUPLE,
  OP_SETTUPLE,
  OP_GETLOCAL,
  OP_SETLOCAL,
  OP_GETUPVAL,
  OP_SETUPVAL,
  OP_GLOBAL,
  OP_GETTABLE,
  OP_SETTABLE,
  OP_NEWLICK,
  OP_GETPATH,
  OP_SETPATH,
  OP_ADD,
  OP_SUB,
  OP_MULT,
  OP_DIV,
  OP_REM,
  OP_POW,
  OP_NOT,
  OP_CMP,
  OP_EQ,
  OP_NEQ,
  OP_LT,
  OP_LTE,
  OP_GT,
  OP_GTE,
  OP_BITN,
  OP_BITL,
  OP_BITR,
  OP_DEF,
  OP_BIND,
  OP_MSG,
  OP_JMP,
  OP_TEST,
  OP_TESTJMP,
  OP_NOTJMP,
  OP_NAMED,
  OP_CALL,
  OP_CALLSET,
  OP_TAILCALL,
  OP_RETURN,
  OP_PROTO,
  OP_CLASS,
  OP_DEBUG
};

#endif
