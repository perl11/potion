//
// vm.c
// the vm execution loop
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "potion.h"
#include "internal.h"
#include "opcodes.h"
#include "asm.h"

extern PNTarget potion_target_x86, potion_target_ppc;

// TODO: this is being circumvented right now, but it's broken without varargs.
PN potion_vm_proto(Potion *P, PN cl, PN args) {
  return potion_vm(P, PN_CLOSURE(cl)->data[0], args,
    PN_CLOSURE(cl)->extra - 1, &PN_CLOSURE(cl)->data[1]);
}

#define STACK_MAX 4096
#define JUMPS_MAX 1024

void potion_vm_init(Potion *P) {
  P->targets[POTION_X86] = potion_target_x86;
  P->targets[POTION_PPC] = potion_target_ppc;
}

#define CASE_OP(name, args) case OP_##name: target->op[OP_##name]args; break;

PN_F potion_jit_proto(Potion *P, PN proto, PN target_id) {
  long regs = 0, lregs = 0, need = 0, rsp = 0, argx = 0, protoargs = 4;
  PN_OP *start, *pos, *end;
  PNJumps jmps[JUMPS_MAX]; size_t offs[JUMPS_MAX]; int jmpc = 0, jmpi = 0;
  vPN(Proto) f = (struct PNProto *)proto;
  int upc = PN_TUPLE_LEN(f->upvals);
  PNAsm * volatile asmb = potion_asm_new(P);
  u8 *fn;
  PN_F * volatile jit_protos = NULL;
  PNTarget *target = &P->targets[target_id];
  target->setup(P, asmb);

  start = pos = ((PN_OP *)PN_STR_PTR(f->asmb));
  end = (PN_OP *)(PN_STR_PTR(f->asmb) + PN_STR_LEN(f->asmb));

  if (PN_TUPLE_LEN(f->protos) > 0) {
    jit_protos = PN_ALLOC_N(PN_F, PN_TUPLE_LEN(f->protos));
    PN_TUPLE_EACH(f->protos, i, proto2, {
      int p2args = 3;
      vPN(Proto) f2 = (struct PNProto *)proto2;
      // TODO: i'm repeating this a lot. sad.
      if (PN_IS_TUPLE(f2->sig)) {
        PN_TUPLE_EACH(f2->sig, i, v, {
          if (PN_IS_STR(v)) p2args++;
        });
      }
      jit_protos[i] = potion_jit_proto(P, proto2, target_id);
      if (p2args > protoargs)
        protoargs = p2args;
    });
  }

  regs = PN_INT(f->stack);
  lregs = regs + PN_TUPLE_LEN(f->locals);
  need = lregs + upc + 2;
  rsp = (need + protoargs) * sizeof(PN);

  target->stack(P, asmb, rsp);
  target->registers(P, asmb, need);

  // Read locals
  if (PN_IS_TUPLE(f->sig)) {
    argx = 0;
    PN_TUPLE_EACH(f->sig, i, v, {
      if (PN_IS_STR(v)) {
        PN_SIZE num = PN_GET(f->locals, v);
        target->local(P, asmb, regs + num, argx);
        argx++;
      }
    });
  }

  // if CL passed in with upvals, load them
  if (upc > 0)
    target->upvals(P, asmb, lregs, upc);

  while (pos < end) {
    offs[pos - start] = asmb->len;
    for (jmpi = 0; jmpi < jmpc; jmpi++) {
      if (jmps[jmpi].to == pos) {
        unsigned char *asmj = asmb->ptr + jmps[jmpi].from;
        target->jmpedit(P, asmb, asmj, asmb->len - (jmps[jmpi].from + 4));
      }
    }

    switch (pos->code) {
      CASE_OP(MOVE, (P, asmb, pos))
      CASE_OP(LOADPN, (P, asmb, pos)) 
      CASE_OP(LOADK, (P, asmb, pos, f->values))
      CASE_OP(SELF, (P, asmb, pos, need))
      CASE_OP(GETLOCAL, (P, asmb, pos, regs, jit_protos))
      CASE_OP(SETLOCAL, (P, asmb, pos, regs, jit_protos))
      CASE_OP(GETUPVAL, (P, asmb, pos, lregs))
      CASE_OP(SETUPVAL, (P, asmb, pos, lregs))
      CASE_OP(NEWTUPLE, (P, asmb, pos, need))
      CASE_OP(SETTUPLE, (P, asmb, pos, need))
      CASE_OP(SETTABLE, (P, asmb, pos, need, f->values))
      CASE_OP(ADD, (P, asmb, pos))
      CASE_OP(SUB, (P, asmb, pos))
      CASE_OP(MULT, (P, asmb, pos))
      CASE_OP(DIV, (P, asmb, pos))
      CASE_OP(REM, (P, asmb, pos))
      CASE_OP(POW, (P, asmb, pos, need))
      CASE_OP(NEQ, (P, asmb, pos))
      CASE_OP(EQ, (P, asmb, pos))
      CASE_OP(LT, (P, asmb, pos))
      CASE_OP(LTE, (P, asmb, pos))
      CASE_OP(GT, (P, asmb, pos))
      CASE_OP(GTE, (P, asmb, pos))
      CASE_OP(BITL, (P, asmb, pos))
      CASE_OP(BITR, (P, asmb, pos))
      CASE_OP(BIND, (P, asmb, pos, need))
      CASE_OP(JMP, (P, asmb, pos, start, jmps, offs, &jmpc))
      CASE_OP(TEST, (P, asmb, pos))
      CASE_OP(NOT, (P, asmb, pos))
      CASE_OP(CMP, (P, asmb, pos))
      CASE_OP(TESTJMP, (P, asmb, pos, start, jmps, offs, &jmpc))
      CASE_OP(NOTJMP, (P, asmb, pos, start, jmps, offs, &jmpc))
      CASE_OP(CALL, (P, asmb, pos, need))
      CASE_OP(RETURN, (P, asmb, pos))
      CASE_OP(PROTO, (P, asmb, &pos, jit_protos, f->protos, lregs, need, regs))
    }
    pos++;
  }

  fn = PN_ALLOC_FUNC(asmb->len);

  target->finish(P, asmb);

#ifdef JIT_DEBUG
  printf("JIT(%p): ", fn);
  long ai = 0;
  for (ai = 0; ai < asmb->len; ai++) {
    printf("%x ", asmb->ptr[ai]);
  }
  printf("\n");
#endif
  PN_MEMCPY_N(fn, asmb->ptr, u8, asmb->len);

  return (PN_F)fn;
}

PN potion_vm(Potion *P, PN proto, PN vargs, PN_SIZE upc, PN * volatile upargs) {
  vPN(Proto) f = (struct PNProto *)proto;

  // these variables persist as we jump around
  PN volatile * volatile stack = PN_ALLOC_N(PN, STACK_MAX);
  PN val = PN_NIL;
  PN self = P->lobby;

  // these variables change from proto to proto
  // current = upvals | locals | self | reg
  PN_OP *pos, *end;
  long argx = 0;
  PN * volatile args = NULL, * volatile upvals, * volatile locals, * volatile reg;
  PN * volatile current = stack;

  pos = ((PN_OP *)PN_STR_PTR(f->asmb));
  if (vargs != PN_NIL) args = PN_GET_TUPLE(vargs)->set;
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
    reg[-1] = reg[0] = self;
    if (upc > 0 && upargs != NULL) {
      PN_SIZE i;
      for (i = 0; i < upc; i++) {
        upvals[i] = upargs[i];
      }
    }

    if (args != NULL) {
      argx = 0;
      PN_TUPLE_EACH(f->sig, i, v, {
        if (PN_IS_STR(v)) {
          PN_SIZE num = PN_GET(f->locals, v);
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
      case OP_SELF:
        reg[pos->a] = reg[-1];
      break;
      case OP_GETLOCAL:
        if (PN_IS_REF(locals[pos->b]))
          reg[pos->a] = PN_DEREF(locals[pos->b]);
        else
          reg[pos->a] = locals[pos->b];
      break;
      case OP_SETLOCAL:
        if (PN_IS_REF(locals[pos->b]))
          PN_DEREF(locals[pos->b]) = reg[pos->a];
        else
          locals[pos->b] = reg[pos->a];
      break;
      case OP_GETUPVAL:
        reg[pos->a] = PN_DEREF(upvals[pos->b]);
      break;
      case OP_SETUPVAL:
        PN_DEREF(upvals[pos->b]) = reg[pos->a];
      break;
      case OP_NEWTUPLE:
        reg[pos->a] = PN_TUP0();
      break;
      case OP_SETTUPLE:
        reg[pos->a] = PN_PUSH(reg[pos->a], reg[pos->b]);
      break;
      case OP_SETTABLE:
        potion_table_set(P, reg[pos->a], reg[pos->b], reg[pos->a+1]);
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
        switch (PN_TYPE(reg[pos->b])) {
          case PN_TCLOSURE:
            if (PN_CLOSURE(reg[pos->b])->method != (PN_F)potion_vm_proto) {
              reg[pos->a] = potion_call(P, reg[pos->b], pos->b - pos->a, reg + pos->a);
            } else {
              self = reg[pos->a];
              args = &reg[pos->a+1];
              upc = PN_CLOSURE(reg[pos->b])->extra - 1;
              upargs = &PN_CLOSURE(reg[pos->b])->data[1];
              current = reg + PN_INT(f->stack) + 2;
              current[-2] = (PN)f;
              current[-1] = (PN)pos;

              f = PN_PROTO(PN_CLOSURE(reg[pos->b])->data[0]);
              pos = ((PN_OP *)PN_STR_PTR(f->asmb));
              goto reentry;
            }
          break;
          
          default:
            reg[pos->a] = potion_obj_call(P, reg[pos->b], 1, reg[pos->a+1]);
          break;
        }
      break;
      case OP_RETURN:
        if (current != stack) {
          val = reg[pos->a];

          f = PN_PROTO(current[-2]);
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
        vPN(Closure) cl;
        unsigned areg = pos->a;
        proto = PN_TUPLE_AT(f->protos, pos->b);
        cl = (struct PNClosure *)potion_closure_new(P, (PN_F)potion_vm_proto, PN_NIL,
          PN_TUPLE_LEN(PN_PROTO(proto)->upvals) + 1);
        cl->data[0] = proto;
        PN_TUPLE_COUNT(PN_PROTO(proto)->upvals, i, {
          pos++;
          if (pos->code == OP_GETUPVAL) {
            cl->data[i+1] = upvals[pos->b];
          } else if (pos->code == OP_GETLOCAL) {
            cl->data[i+1] = locals[pos->b] = (PN)potion_ref(P, locals[pos->b]);
          } else {
            fprintf(stderr, "** missing an upval to proto %p\n", (void *)proto);
          }
        });
        reg[areg] = (PN)cl;
      }
      break;
    }
    pos++;
  }

done:
  val = reg[0];
  return val;
}
