//
// compile.c
// the vm execution loop
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include "potion.h"
#include "internal.h"
#include "opcodes.h"

extern u8 potion_op_args[];

PN potion_vm(Potion *P, PN proto) {
  struct PNProto *f = (struct PNProto *)proto;
  long stack = PN_INT(f->stack);
  u8 *pos, *end;
  PN *reg = PN_ALLOC_N(PN, stack + 1);
  PN *locals = NULL;
  if (f->locals != PN_EMPTY)
    locals = PN_ALLOC_N(PN, PN_TUPLE_LEN(f->locals));

  reg[0] = PN_NIL;
  pos = (u8 *)PN_STR_PTR(f->asmb);
  end = (u8 *)PN_STR_PTR(f->asmb) + PN_STR_LEN(f->asmb);
  while (pos < end) {
    switch (pos[0]) {
      case OP_GETLOCAL:
        reg[pos[1]] = locals[pos[2]]; 
      break;
      case OP_SETLOCAL:
        locals[pos[1]] = reg[pos[2]]; 
      break;
      case OP_LOADK:
        reg[pos[1]] = PN_TUPLE_AT(f->values, pos[2]);
      break;
      case OP_BIND:
        reg[pos[1]] = potion_bind(P, reg[pos[2]], reg[pos[1]]);
      break;
      case OP_CALL:
        reg[pos[1]] = 
          ((struct PNClosure *)reg[pos[1]+1])->method(P, reg[pos[1]+1], reg[pos[1]], reg[pos[2]]);
      break;
      case OP_RETURN:
        return reg[pos[1]];
      break;
      case OP_PROTO:
        reg[pos[1]] = PN_TUPLE_AT(f->protos, pos[2]);
      break;
    }
    pos += 1 + potion_op_args[pos[0]];
  }
  return reg[0];
}
