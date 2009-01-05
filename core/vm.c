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
#ifndef __MINGW32__
#include <sys/mman.h>
#endif
#include <string.h>
#endif

extern u8 potion_op_args[];

PN potion_vm_proto(Potion *P, PN cl, PN self, PN args) {
  return potion_vm(P, PN_CLOSURE(cl)->data[0], args,
    PN_CLOSURE(cl)->extra - 1, &PN_CLOSURE(cl)->data[1]);
}

#define STACK_MAX 72000

#ifdef X86_JIT

#define RBP(x)  (0x100 - ((x + 1) * sizeof(PN)))
#define RBPI(x) (0x100 - ((x + 1) * sizeof(int)))
#define X86(ins) *asmb++ = ins;
#define X86I(pn) *((int *)asmb) = (int)(pn); asmb += sizeof(int)
#define X86N(pn) *((PN *)asmb) = (PN)(pn); asmb += sizeof(PN)

#if PN_SIZE_T == 4
#define X86_PRE_T 0
#define X86_PRE()
#define X86_POST()
#define X86C(op32, op64) op32
#else
#define X86_PRE_T 1
#define X86_PRE()  X86(0x48)
#define X86_POST() X86(0x48); X86(0x98)
#define X86C(op32, op64) op64
#endif

#define X86_MOV_RBP(reg, x) \
        X86_PRE(); X86(reg); X86(0x45); X86(RBP(x))
#define X86_MOVL(reg, x) ({ \
        int i, mreg; \
        for (i = 0, mreg = (reg + 1) * movl; i < movl; i++) { \
          int *xp = (int *)&x; \
          X86(0xC7); /* movl */ \
          X86(0x45); X86(RBPI(--mreg)); /* -A(%rbp) */ \
          X86I(*(xp+i)); \
        } \
})
#define X86_MOVQ(reg, x) \
        X86_PRE(); X86(0xC7); /* movl */ \
        X86(0x45); X86(RBP(reg)); /* -A(%rbp) */ \
        X86I((PN)(x))
#define X86_MATH(op) \
        X86_MOV_RBP(0x8B, pos->a); /* mov -A(%rbp) %eax */ \
        X86(0x89); X86(0xC2); /* mov %eax %edx */ \
        X86(0xD1); X86(0xFA); /* sar %edx */ \
        X86_MOV_RBP(0x8B, pos->b); /* mov -B(%rbp) %eax */ \
        X86(0xD1); X86(0xF8); /* sar %eax */ \
        op; /* add, sub, ... */ \
        X86_POST(); /* cltq */ \
        X86_PRE(); X86(0x01); X86(0xC0); /* add %rax %rax */ \
        X86_PRE(); X86(0x83); X86(0xC8); X86(0x01); /* or 0x1 %eax */ \
        X86_MOV_RBP(0x89, pos->a); /* mov -B(%rbp) %eax */ \

// TODO: pass variables through Potion struct? expand?
const u8 x86_var_regs[] = {0x55, 0x4D};

PN potion_x86_debug(Potion *P, PN cl, PN arg1, PN arg2) {
  PN up1, up2;
  if (PN_IS_CLOSURE(cl)) {
    up1 = PN_CLOSURE(cl)->data[1];
    up2 = PN_CLOSURE(cl)->data[2];
  }
  printf("DEBUG: %p %lu %lu %lu\n", P, cl, arg1, arg2);
  return PN_NUM(10);
}

PN potion_x86_stub(Potion *P, PN cl) {
  jit_t func = (jit_t)PN_CLOSURE(cl)->data[0];
  return func(P, cl); // TODO: use varargs
}

jit_t potion_x86_proto(Potion *P, PN proto) {
  long regs = 0, lregs = 0, need = 0, argx = 0;
  PN val;
  PN_OP *pos, *end;
  struct PNProto *f = (struct PNProto *)proto;
  int upi, upc = PN_TUPLE_LEN(f->upvals);
  int movl = sizeof(PN) / sizeof(int);
  u8 *start, *asmb;
  jit_t jit_func;
  jit_t *jit_protos = NULL;

  start = asmb = PN_ALLOC_FUNC(1024);
  jit_func = (jit_t)asmb;
  X86(0x55); // push %rbp
  X86_PRE(); X86(0x89); X86(0xE5); // mov %rsp,%rbp

  pos = ((PN_OP *)PN_STR_PTR(f->asmb));
  end = (PN_OP *)(PN_STR_PTR(f->asmb) + PN_STR_LEN(f->asmb));

  if (PN_TUPLE_LEN(f->protos) > 0) {
    jit_protos = PN_ALLOC_N(jit_t, PN_TUPLE_LEN(f->protos));
    PN_TUPLE_EACH(f->protos, i, proto2, {
      jit_protos[i] = potion_x86_proto(P, proto2);
    });
  }

  regs = PN_INT(f->stack);
  lregs = regs + PN_TUPLE_LEN(f->locals);
  need = lregs + upc + 2;
  if (jit_protos != NULL) {
    // move the stack pointer if we need registers
    X86_PRE(); X86(0x83); X86(0xEC); X86(need * sizeof(PN));
  }

  // (Potion *, CL) in the last "register"
  X86_PRE(); X86(0x89); X86(0x7d); X86(RBP(need - 2));
  X86_PRE(); X86(0x89); X86(0x75); X86(RBP(need - 1));

  // Read locals
  if (PN_IS_TUPLE(f->sig)) {
    PN_TUPLE_EACH(f->sig, i, v, {
      if (PN_IS_STR(v)) {
        unsigned long num = PN_GET(f->locals, v);
        X86_PRE(); X86(0x89); X86(x86_var_regs[argx++]); X86(RBP(regs + num));
      }
    });
  }

  // if CL passed in with upvals, load them
  // TODO: optimize the PN_IS_CLOSURE call here
  if (upc > 0) {
    X86_PRE(); X86(0x8B); X86(0x45); X86(RBP(need - 1)); // mov cl %rax
    X86(0x83); X86(0xE0); X86(0x01); // and 0x1 %eax
    X86(0x85); X86(0xC0); // test %eax %eax
    X86(0x75); X86(X86C(21, 25) + (X86C(9, 12) * upc)); // jne 12(u)+25
    X86_PRE(); X86(0x8B); X86(0x45); X86(RBP(need - 1)); // mov cl %rax
    X86_PRE(); X86(0x83); X86(0xE0); X86(0xF8); // and ^0x7 %eax
    X86_PRE(); X86(0x85); X86(0xC0); // test %rax %rax
    X86(0x74); X86(X86C(11, 12) + (X86C(9, 12) * upc)); // je 12(u)+12
    X86_PRE(); X86(0x8B); X86(0x45); X86(RBP(need - 1)); // mov cl %rax
    X86(0x8B); X86(0x40); X86(sizeof(PN_GC)); // mov (%rax).vt %eax
    X86(0x83); X86(0xF8); X86(PN_TCLOSURE); // cmp 0x5 %eax
    X86(0x75); X86(X86C(9, 12) * upc); // jne 12(u)

    for (upi = 0; upi < upc; upi++) {
      X86_PRE(); X86(0x8B); X86(0x45); X86(RBP(need - 1)); // mov cl %rax
      X86_PRE(); X86(0x8B); X86(0x40);
        X86(sizeof(struct PNClosure) + ((upi + 1) * sizeof(PN))); // 0x30(%rax)
      X86_MOV_RBP(0x89, lregs + upi);
      upc--;
    }
  }

  while (pos < end) {
    switch (pos->code) {
      case OP_MOVE:
        X86_MOV_RBP(0x8B, pos->b);
        X86_MOV_RBP(0x89, pos->a);
      break;
      case OP_LOADPN:
        X86_MOVQ(pos->a, pos->b);
      break;
      case OP_LOADK:
        val = PN_TUPLE_AT(f->values, pos->b);
        X86_MOVQ(pos->a, val);
      break;
      case OP_GETLOCAL:
        X86_MOV_RBP(0x8B, regs + pos->b);
        X86_MOV_RBP(0x89, pos->a);
      break;
      case OP_SETLOCAL:
        X86_MOV_RBP(0x8B, pos->a);
        X86_MOV_RBP(0x89, regs + pos->b);
      break;
      case OP_ADD:
        X86_MATH({
          X86(0x89); X86(0xD1); // mov %rdx %rcx
          X86(0x01); X86(0xC1); // add %rax %rcx
          X86(0x89); X86(0xC8); // mov %rcx %rax
        });
      break;
      case OP_SUB:
        X86_MATH({
          X86(0x89); X86(0xD1); // mov %rdx %rcx
          X86(0x29); X86(0xC1); // sub %rax %rcx
          X86(0x89); X86(0xC8); // mov %rcx %rax
        });
      break;
      case OP_MULT:
        X86_MATH({
          X86(0x0F); X86(0xAF); X86(0xC2); // imul %rdx %rax
        });
      break;
      case OP_GT:
        X86_MATH({
          X86(0x39); X86(0xC2); // cmp %eax %edx
          X86(0x7E); X86(0x09 + X86_PRE_T); // jle +10
          X86_PRE(); X86(0xC7); // movl
          X86(0x45); X86(RBP(pos->a)); // -A(%rbp) = TRUE
          X86I(PN_TRUE);
          X86(0); // noop
          X86(0xEB); X86(0x7 + X86_PRE_T); // jmp +8
          X86(0x45); X86(RBP(pos->a)); // -A(%rbp) = FALSE
          X86I(PN_FALSE);
        });
      break;
      case OP_CALL: {
        int argc = pos->b - pos->a;
        // (Potion *, CL) as the first argument
        X86_PRE(); X86(0x8B); X86(0x7d); X86(RBP(need - 1));
        X86_PRE(); X86(0xBE); X86N(proto); // TODO: send closure obj
        while (--argc > 0) {
          X86_PRE(); X86(0x8B); X86(x86_var_regs[argc - 1]); X86(RBP(pos->a + argc));
        }
        // TODO: check for PNClosure and dispatch to C funcs / bytecode alike
        X86_PRE(); X86(0x8B); X86(0x45); X86(RBP(pos->b)); /* mov -preg(%ebp) %rax */
        X86_PRE(); X86(0x8B); X86(0x40); X86(sizeof(struct PNClosure)); /* mov N(%rax) %rax */
        X86(0xFF); X86(0xD0); /* callq *%rax */
        X86_PRE(); X86(0x89); X86(0x45); X86(RBP(pos->a)); /* mov -preg(%ebp) %rax */
      }
      break;
      case OP_RETURN:
        X86_MOV_RBP(0x8B, 0); /* mov -0(%rbp) %eax */ \
        X86(0xC9); X86(0xC3);
      break;
      case OP_PROTO: {
        struct PNClosure *cl;
        PN func2 = (PN)jit_protos[pos->b];
        PN proto = PN_TUPLE_AT(f->protos, pos->b);
        cl = (struct PNClosure *)potion_closure_new(P, (imp_t)potion_x86_stub, PN_NIL,
          PN_TUPLE_LEN(PN_PROTO(proto)->upvals) + 1);
        // func2 = &potion_x86_debug;
        cl->data[0] = func2;
        X86_MOVL(pos->a, cl);
        PN_TUPLE_COUNT(PN_PROTO(proto)->upvals, i, {
          pos++;
          if (pos->code == OP_GETUPVAL) {
            X86_PRE(); X86(0x8B); X86(0x50); X86(RBP(lregs + pos->b)); // mov upval %rdx
          } else if (pos->code == OP_GETLOCAL) {
            X86_PRE(); X86(0x8B); X86(0x7d); X86(RBP(need - 1));
            X86_PRE(); X86(0x8B); X86(0x75); X86(RBP(regs + pos->b));
            X86_PRE(); X86(0xB8); X86N(&potion_ref); // mov &potion_ref %rax
            X86(0xFF); X86(0xD0); // callq %rax
            X86_MOV_RBP(0x89, regs + pos->b); // mov %rax local
            X86_PRE(); X86(0x89); X86(0xC2); // mov %rax %rdx
          } else {
            fprintf(stderr, "** missing an upval to proto %p\n", (void *)proto);
          }
          X86_MOV_RBP(0x8B, pos->a); // mov cl %rax
          X86_PRE(); X86(0x89); X86(0x50); // mov %rdx N(%rax)
            X86(sizeof(struct PNClosure) + (sizeof(PN) * (i + 1)));
        });
      }
      break;
    }
    pos++;
  }

#if DEBUG
  printf("jit(%p): ", jit_func);
  while (start < asmb) {
    printf("%x ", start[0]);
    start++;
  }
  printf("\n\n");
#endif

  return jit_func;
}
#endif

PN potion_vm(Potion *P, PN proto, PN vargs, unsigned int upc, PN* upargs) {
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
    if (upc > 0 && upargs != NULL) {
      unsigned int i;
      for (i = 0; i < upc; i++) {
        upvals[i] = upargs[i];
      }
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
        cl = (struct PNClosure *)potion_closure_new(P, (imp_t)potion_vm_proto, PN_NIL,
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
