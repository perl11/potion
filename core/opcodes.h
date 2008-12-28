//
// opcodes.h
// the Potion VM instruction set (heavily based on Lua's)
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#ifndef POTION_OPCODES_H
#define POTION_OPCODES_H

enum PN_OPCODE {
  OP_MOVE,
  OP_LOADK,
  OP_LOADNIL,
  OP_LOADBOOL,
  OP_GETLOCAL,
  OP_SETLOCAL,
  OP_SELF
};

enum PN_OPARGS {
  OP_ASM1,
  OP_ASM2
};

#endif
