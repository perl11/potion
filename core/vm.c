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

PN potion_vm(Potion *P, PN proto, PN args) {
  struct PNProto *f = (struct PNProto *)proto;
  long stack = PN_INT(f->stack);
  int argx = 0;
  u8 *pos, *end;
  PN *reg = PN_ALLOC_N(PN, stack + 1);
  PN *locals = NULL;
  if (f->locals != PN_EMPTY)
    locals = PN_ALLOC_N(PN, PN_TUPLE_LEN(f->locals));

  PN_TUPLE_EACH(f->sig, i, v, {
    if (PN_IS_STR(v)) {
      unsigned long num = PN_GET(f->locals, v);
      locals[num] = PN_TUPLE_AT(args, argx++);
    }
  });

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
      case OP_NEWTABLE:
        reg[pos[1]] = PN_EMPTY;
      break;
      case OP_SETTABLE:
        reg[pos[1]] = PN_PUSH(reg[pos[1]], reg[pos[2]]);
      break;
      case OP_LOADK:
        reg[pos[1]] = PN_TUPLE_AT(f->values, pos[2]);
      break;
      case OP_BIND:
        reg[pos[1]] = potion_bind(P, reg[pos[2]], reg[pos[1]]);
      break;
      case OP_CALL:
        if (PN_TYPE(reg[pos[2]]) == PN_TCLOSURE) {
          reg[pos[1]] = 
            ((struct PNClosure *)reg[pos[2]])->method(P, reg[pos[2]], reg[pos[1]], reg[pos[1]+1]);
        } else {
          reg[pos[1]] = potion_vm(P, reg[pos[2]], reg[pos[1]]);
        }
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
