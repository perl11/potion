//
// vm.c
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

#ifdef X86_JIT
#include <sys/mman.h>
#include <string.h>
#endif

extern u8 potion_op_args[];

PN potion_vm_proto(Potion *P, PN cl, PN self, PN args) {
  PN proto = PN_TUPLE_AT(PN_CLOSURE(cl)->data, 0);
  return potion_vm(P, proto, args, PN_CLOSURE(cl)->data);
}

#define STACK_MAX 72000

#ifdef X86_JIT
jit_t potion_x86_proto(Potion *P, PN proto) {
  struct PNProto *f = (struct PNProto *)proto;
  PN val;
  PN_OP *pos, *end;
  u8 *start, *asmb = (char *)mmap(0, 64, PROT_READ|PROT_WRITE|PROT_EXEC,(MAP_PRIVATE|MAP_ANON), -1, 0);
  jit_t jit_func = (jit_t)asmb;
  start = asmb;
  asmb[0] = 0x55; // push %rbp
  asmb[1] = 0x48; asmb[2] = 0x89; asmb[3] = 0xe5; // mov %rsp,%rbp
  asmb += 4;

  pos = ((PN_OP *)PN_STR_PTR(f->asmb));
  end = (PN_OP *)(PN_STR_PTR(f->asmb) + PN_STR_LEN(f->asmb));

  while (pos < end) {
    switch (pos->code) {
      case OP_LOADK:
        val = PN_TUPLE_AT(f->values, pos->b);
        asmb[0] = 0xc7; // movl
        asmb[1] = 0x45; asmb[2] = 0xfc - (pos->a * 4); // -A(%rbp)
        *((PN *)(asmb + 3)) = val; // const
        asmb += 7;
      break;
      case OP_ADD:
        asmb[0] = 0x8b; // mov
        asmb[1] = 0x45; asmb[2] = 0xfc - (pos->a * 4); // -A(%rbp) %eax
        asmb[3] = 0x01; // add
        asmb[4] = 0x45; asmb[5] = 0xfc - (pos->b * 4); // %eax -B(%rbp)
        asmb[6] = 0x8b; // mov
        asmb[7] = 0x45; asmb[8] = 0xfc - (pos->b * 4); // -B(%rbp) %eax
        asmb += 9;
      break;
    }
    pos++;
  }

  asmb[0] = 0xc9; asmb[1] = 0xc3;
  asmb += 2;

  return jit_func;
}
#endif

PN potion_vm(Potion *P, PN proto, PN vargs, PN upargs) {
  struct PNProto *f = (struct PNProto *)proto;

  // these variables persist as we jump around
  PN *stack = PN_ALLOC_N(PN, STACK_MAX);
  PN val = PN_NIL;

  // these variables change from proto to proto
  // current = upvals | locals | self | reg
  PN_OP *pos, *end;
  long argx = 0;
  PN *args, *upvals, *locals, *reg;
  PN *current = stack;

  pos = ((PN_OP *)PN_STR_PTR(f->asmb));
  args = PN_GET_TUPLE(vargs)->set;
reentry:
  // TODO: place the stack in P, allow it to allocate as needed
  if (current - stack >= STACK_MAX) {
    fprintf(stderr, "all registers used up!");
    exit(1);
  }

  upvals = current;
  locals = upvals + f->upvalsize;
  reg = locals + f->localsize + 1;

  if (pos == (PN_OP *)PN_STR_PTR(f->asmb)) {
    reg[0] = PN_VTABLE(PN_TLOBBY);
    if (PN_IS_TUPLE(upargs)) {
      PN_TUPLE_EACH(upargs, i, v, {
        if (i > 0) upvals[i-1] = v;
      });
    }

    if (args != NULL) {
      argx = 0;
      PN_TUPLE_EACH(f->sig, i, v, {
        if (PN_IS_STR(v)) {
          unsigned long num = PN_GET(f->locals, v);
          locals[num] = args[argx++];
        }
      });
    }
  }

  end = (PN_OP *)(PN_STR_PTR(f->asmb) + PN_STR_LEN(f->asmb));
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
        locals[pos->b] = reg[pos->a];
      break;
      case OP_GETUPVAL:
        reg[pos->a] = *((PN *)(upvals[pos->b]));
      break;
      case OP_SETUPVAL:
        *((PN *)(upvals[pos->b])) = reg[pos->a];
      break;
      case OP_SETTABLE:
        reg[pos->a] = PN_PUSH(reg[pos->a], reg[pos->b]);
      break;
      case OP_ADD:
        reg[pos->a] = reg[pos->a] + (reg[pos->b]-1);
      break;
      case OP_SUB:
        reg[pos->a] = reg[pos->a] - (reg[pos->b]-1);
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
        reg[pos->a] = PN_NUM((int)pow((double)PN_INT(reg[pos->a]),
          (double)PN_INT(reg[pos->b])));
      break;
      case OP_NOT:
        reg[pos->a] = PN_BOOL(!PN_TEST(reg[pos->a]));
      break;
      case OP_CMP:
        reg[pos->a] = PN_NUM(PN_INT(reg[pos->b]) - PN_INT(reg[pos->a]));
      break;
      case OP_NEQ:
        reg[pos->a] = PN_BOOL(reg[pos->a] != reg[pos->b]);
      break;
      case OP_EQ:
        reg[pos->a] = PN_BOOL(reg[pos->a] == reg[pos->b]);
      break;
      case OP_LT:
        reg[pos->a] = PN_BOOL((long)(reg[pos->a]) < (long)(reg[pos->b]));
      break;
      case OP_LTE:
        reg[pos->a] = PN_BOOL((long)(reg[pos->a]) <= (long)(reg[pos->b]));
      break;
      case OP_GT:
        reg[pos->a] = PN_BOOL((long)(reg[pos->a]) > (long)(reg[pos->b]));
      break;
      case OP_GTE:
        reg[pos->a] = PN_BOOL((long)(reg[pos->a]) >= (long)(reg[pos->b]));
      break;
      case OP_BITL:
        reg[pos->a] = PN_NUM(PN_INT(reg[pos->a]) << PN_INT(reg[pos->b]));
      break;
      case OP_BITR:
        reg[pos->a] = PN_NUM(PN_INT(reg[pos->a]) >> PN_INT(reg[pos->b]));
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
      case OP_TESTJMP:
        if (PN_TEST(reg[pos->a])) pos += pos->b;
      break;
      case OP_NOTJMP:
        if (!PN_TEST(reg[pos->a])) pos += pos->b;
      break;
      case OP_CALL:
        if (PN_TYPE(reg[pos->b]) == PN_TCLOSURE) {
          if (PN_CLOSURE(reg[pos->b])->method != (imp_t)potion_vm_proto) {
            reg[pos->a] =
              ((struct PNClosure *)reg[pos->b])->method(P, reg[pos->b], reg[pos->a], reg[pos->a+1]);
          } else {
            args = &reg[pos->a];
            upargs = PN_CLOSURE(reg[pos->b])->data;
            current = reg + PN_INT(f->stack) + 2;
            current[-2] = (PN)f;
            current[-1] = (PN)pos;

            f = (struct PNProto *)PN_TUPLE_AT(upargs, 0);
            pos = ((PN_OP *)PN_STR_PTR(f->asmb));
            goto reentry;
          }
        } else {
          reg[pos->a] = potion_send(reg[pos->b], PN_call);
        }
      break;
      case OP_RETURN:
        if (current != stack) {
          val = reg[pos->a];

          f = (struct PNProto *)current[-2];
          pos = (PN_OP *)current[-1];
          reg = current - (PN_INT(f->stack) + 2);
          current = reg - (f->localsize + f->upvalsize + 1);
          reg[pos->a] = val;
          pos++;
          goto reentry;
        } else {
          reg[0] = reg[pos->a];
          goto done;
        }
      break;
      case OP_PROTO: {
        unsigned areg = pos->a;
        proto = PN_TUPLE_AT(f->protos, pos->b);
        val = PN_TUP(proto);
        PN_TUPLE_EACH(((struct PNProto *)proto)->upvals, i, v, {
          // TODO: pass in weakrefs as upvals
          pos++;
          if (pos->code == OP_GETUPVAL) {
            val = PN_PUSH(val, upvals[pos->b]);
          } else if (pos->code == OP_GETLOCAL) {
            val = PN_PUSH(val, (PN)&locals[pos->b]);
          }
        });
        reg[areg] = potion_closure_new(P, (imp_t)potion_vm_proto, PN_NIL, val);
      }
      break;
    }
    pos++;
  }

done:
  val = reg[0];
  PN_FREE(stack);
  return val;
}
