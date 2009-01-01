//
// compile.c
// the vm execution loop
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "potion.h"
#include "internal.h"
#include "opcodes.h"

extern u8 potion_op_args[];

PN potion_vm(Potion *P, PN proto, PN args) {
  struct PNProto *f = (struct PNProto *)proto;
  long stack = PN_INT(f->stack);
  int argx = 0;
  PN_OP *pos, *end;
  PN *locals = PN_ALLOC_N(PN, PN_TUPLE_LEN(f->locals) + stack + 1);
  PN *self = locals + PN_TUPLE_LEN(f->locals);
  PN *reg = self + 1;

  if (!PN_IS_TUPLE(args)) args = PN_EMPTY;
  PN_TUPLE_EACH(f->sig, i, v, {
    if (PN_IS_STR(v)) {
      unsigned long num = PN_GET(f->locals, v);
      locals[num] = PN_TUPLE_AT(args, argx++);
    }
  });

  reg[0] = PN_NIL;
  pos = (PN_OP *)PN_STR_PTR(f->asmb);
  end = (PN_OP *)PN_STR_PTR(f->asmb) + PN_STR_LEN(f->asmb);
  while (pos < end) {
    switch (pos->code) {
      case OP_MOVE:
        reg[pos->a] = reg[pos->b];
      break;
      case OP_LOADK:
        reg[pos->a] = PN_TUPLE_AT(f->values, pos->b);
      break;
      case OP_LOADPN:
        reg[pos->a] = (PN)pos->b;
      break;
      case OP_GETLOCAL:
        reg[pos->a] = locals[pos->b]; 
      break;
      case OP_SETLOCAL:
        locals[pos->a] = reg[pos->b]; 
      break;
      case OP_SETTABLE:
        reg[pos->a] = PN_PUSH(reg[pos->a], reg[pos->b]);
      break;
      case OP_ADD:
        reg[pos->a] = PN_NUM(PN_INT(reg[pos->a]) + PN_INT(reg[pos->b]));
      break;
      case OP_SUB:
        reg[pos->a] = PN_NUM(PN_INT(reg[pos->a]) - PN_INT(reg[pos->b]));
      break;
      case OP_MULT:
        reg[pos->a] = PN_NUM(PN_INT(reg[pos->a]) * PN_INT(reg[pos->b]));
      break;
      case OP_DIV:
        reg[pos->a] = PN_NUM(PN_INT(reg[pos->a]) / PN_INT(reg[pos->b]));
      break;
      case OP_REM:
        reg[pos->a] = PN_NUM(PN_INT(reg[pos->a]) % PN_INT(reg[pos->b]));
      break;
      case OP_POW:
        reg[pos->a] = PN_NUM((int)pn_pow((double)PN_INT(reg[pos->a]),
          (double)PN_INT(reg[pos->b])));
      break;
      case OP_NOT:
        reg[pos->a] = PN_BOOL(!PN_TEST(reg[pos->a]));
      break;
      case OP_NEQ:
        reg[pos->a] = PN_BOOL(reg[pos->a] != PN_INT(reg[pos->b]));
      break;
      case OP_EQ:
        reg[pos->a] = PN_BOOL(reg[pos->a] == PN_INT(reg[pos->b]));
      break;
      case OP_LT:
        reg[pos->a] = PN_BOOL(PN_INT(reg[pos->a]) < PN_INT(reg[pos->b]));
      break;
      case OP_LTE:
        reg[pos->a] = PN_BOOL(PN_INT(reg[pos->a]) <= PN_INT(reg[pos->b]));
      break;
      case OP_GT:
        reg[pos->a] = PN_BOOL(PN_INT(reg[pos->a]) > PN_INT(reg[pos->b]));
      break;
      case OP_GTE:
        reg[pos->a] = PN_BOOL(PN_INT(reg[pos->a]) >= PN_INT(reg[pos->b]));
      break;
      case OP_BIND:
        reg[pos->a] = potion_bind(P, reg[pos->b], reg[pos->a]);
      break;
      case OP_JMP:
        pos += pos->a;
      break;
      case OP_TEST:
        reg[pos->a] = PN_BOOL(PN_TEST(reg[pos->a]));
      break;
      case OP_CALL:
        if (PN_TYPE(reg[pos->b]) == PN_TCLOSURE) {
          reg[pos->a] = 
            ((struct PNClosure *)reg[pos->b])->method(P, reg[pos->b], reg[pos->a], reg[pos->a+1]);
        } else {
          reg[pos->a] = potion_vm(P, reg[pos->b], reg[pos->a]);
        }
      break;
      case OP_RETURN:
        return reg[pos->a];
      break;
      case OP_PROTO:
        reg[pos->a] = PN_TUPLE_AT(f->protos, pos->b);
      break;
    }
    pos++;
  }
  return reg[0];
}
