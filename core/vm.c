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

extern PNTarget potion_target_x86;

// TODO: this is being circumvented right now, but it's broken without varargs.
PN potion_vm_proto(Potion *P, PN cl, PN args) {
  return potion_vm(P, PN_CLOSURE(cl)->data[0], args,
    PN_CLOSURE(cl)->extra - 1, &PN_CLOSURE(cl)->data[1]);
}

#define STACK_MAX 1048576
#define JUMPS_MAX 1024

void potion_vm_init(Potion *P) {
  P->targets[POTION_X86] = potion_target_x86;
}

#define CASE_OP(name, args) case OP_##name: target->op[OP_##name]args; break;

PN_F potion_jit_proto(Potion *P, PN proto, PN target_id) {
  long regs = 0, lregs = 0, need = 0, rsp = 0, argx = 0, protoargs = 4;
  PN_OP *start, *pos, *end;
  PNJumps jmps[JUMPS_MAX]; size_t offs[JUMPS_MAX]; int jmpc = 0, jmpi = 0;
  struct PNProto *f = (struct PNProto *)proto;
  int upc = PN_TUPLE_LEN(f->upvals);
  PNAsm *asmb = potion_asm_new();
  u8 *fn;
  PN_F *jit_protos = NULL;
  PNTarget *target = &P->targets[target_id];
  target->setup(asmb);

  start = pos = ((PN_OP *)PN_STR_PTR(f->asmb));
  end = (PN_OP *)(PN_STR_PTR(f->asmb) + PN_STR_LEN(f->asmb));

  if (PN_TUPLE_LEN(f->protos) > 0) {
    jit_protos = PN_ALLOC_N(PN_F, PN_TUPLE_LEN(f->protos));
    PN_TUPLE_EACH(f->protos, i, proto2, {
      int p2args = 3;
      struct PNProto *f2 = (struct PNProto *)proto2;
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

  target->stack(asmb, rsp);
  target->registers(asmb, need);

  // Read locals
  if (PN_IS_TUPLE(f->sig)) {
    argx = 0;
    PN_TUPLE_EACH(f->sig, i, v, {
      if (PN_IS_STR(v)) {
        PN_SIZE num = PN_GET(f->locals, v);
        target->local(asmb, regs + num, argx);
        argx++;
      }
    });
  }

  // if CL passed in with upvals, load them
  if (upc > 0)
    target->upvals(asmb, lregs, upc);

  while (pos < end) {
    offs[pos - start] = asmb->ptr - asmb->start;
    for (jmpi = 0; jmpi < jmpc; jmpi++) {
      if (jmps[jmpi].to == pos) {
        u8 *asmj = asmb->start + jmps[jmpi].from;
        *((int *)asmj) = (int)((asmb->ptr - asmb->start) - (jmps[jmpi].from + 4));
      }
    }

    switch (pos->code) {
      CASE_OP(MOVE, (asmb, pos))
      CASE_OP(LOADPN, (asmb, pos)) 
      CASE_OP(LOADK, (asmb, pos, f))
      CASE_OP(SELF, (asmb, pos, need))
      CASE_OP(GETLOCAL, (asmb, pos, regs, jit_protos))
      CASE_OP(SETLOCAL, (asmb, pos, regs, jit_protos))
      CASE_OP(GETUPVAL, (asmb, pos, lregs))
      CASE_OP(SETUPVAL, (asmb, pos, lregs))
      CASE_OP(NEWTUPLE, (asmb, pos, need))
      CASE_OP(SETTUPLE, (asmb, pos, need))
      CASE_OP(SEARCH, (asmb, pos, need))
      CASE_OP(SETTABLE, (asmb, pos, need, f))
      CASE_OP(ADD, (asmb, pos))
      CASE_OP(SUB, (asmb, pos))
      CASE_OP(MULT, (asmb, pos))
      CASE_OP(DIV, (asmb, pos))
      CASE_OP(REM, (asmb, pos))
      CASE_OP(POW, (asmb, pos, need))
      CASE_OP(NEQ, (asmb, pos))
      CASE_OP(EQ, (asmb, pos))
      CASE_OP(LT, (asmb, pos))
      CASE_OP(LTE, (asmb, pos))
      CASE_OP(GT, (asmb, pos))
      CASE_OP(GTE, (asmb, pos))
      CASE_OP(BITL, (asmb, pos))
      CASE_OP(BITR, (asmb, pos))
      CASE_OP(BIND, (asmb, pos, need))
      CASE_OP(JMP, (asmb, pos, start, jmps, offs, &jmpc))
      CASE_OP(TEST, (asmb, pos))
      CASE_OP(NOT, (asmb, pos))
      CASE_OP(CMP, (asmb, pos))
      CASE_OP(TESTJMP, (asmb, pos, start, jmps, offs, &jmpc))
      CASE_OP(NOTJMP, (asmb, pos, start, jmps, offs, &jmpc))
      CASE_OP(CALL, (asmb, pos, need))
      CASE_OP(RETURN, (asmb, pos))
      CASE_OP(PROTO, (asmb, P, &pos, jit_protos, f->protos, lregs, need, regs))
    }
    pos++;
  }

  asmb->capa = asmb->ptr - asmb->start;
  fn = PN_ALLOC_FUNC(asmb->capa);

  target->finish(asmb);

#ifdef JIT_DEBUG
  printf("JIT(%p): ", fn);
  long ai = 0;
  for (ai = 0; ai < asmb->capa; ai++) {
    printf("%x ", asmb->start[ai]);
  }
  printf("\n");
#endif
  PN_MEMCPY_N(fn, asmb->start, u8, asmb->capa);
  PN_FREE(asmb->start);
  PN_FREE(asmb);

  return (PN_F)fn;
}

PN potion_vm(Potion *P, PN proto, PN vargs, PN_SIZE upc, PN* upargs) {
  struct PNProto *f = (struct PNProto *)proto;

  // these variables persist as we jump around
  PN *stack = PN_ALLOC_N(PN, STACK_MAX);
  PN val = PN_NIL, self = PN_NIL;

  // these variables change from proto to proto
  // current = upvals | locals | self | reg
  PN_OP *pos, *end;
  long argx = 0;
  PN *args = NULL, *upvals, *locals, *reg;
  PN *current = stack;

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
      case OP_SEARCH:
        reg[pos->a] = potion_tuple_at(P, PN_NIL, reg[pos->a], reg[pos->b]);
      break;
      case OP_SETTABLE:
        potion_table_set(P, reg[pos->a], PN_TUPLE_AT(f->values, pos->b), reg[pos->a+1]);
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
        } else {
          reg[pos->a] = potion_send(reg[pos->b], PN_call);
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
        struct PNClosure *cl;
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
  PN_FREE(stack);
  return val;
}
