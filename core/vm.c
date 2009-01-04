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
  PN proto = PN_TUPLE_AT(PN_CLOSURE(cl)->data, 0);
  return potion_vm(P, proto, args, PN_CLOSURE(cl)->data);
}

#define STACK_MAX 72000

#ifdef X86_JIT

#define RBP(x)  (0x100 - ((x + 1) * sizeof(PN)))
#define RBPI(x) (0x100 - ((x + 1) * sizeof(int)))
#define X86(ins) *asmb++ = ins;
#define X86I(pn) *((int *)asmb) = (int)(pn); asmb += sizeof(int)

#if PN_SIZE_T == 4
#define X86_PRE_T 0
#define X86_PRE()
#define X86_POST()
#define X86_CALL(reg, preg) \
        X86(0x8B); X86(0x45); X86(RBP(preg)); /* mov -preg(%ebp) %eax */ \
        X86(0xFF); X86(0xD0); /* callq *%eax */ \
        X86(0x89); X86(0x45); X86(RBP(reg)); /* mov %eax -reg(%ebp) */
#else
#define X86_PRE_T 1
#define X86_PRE()  X86(0x48)
#define X86_POST() X86(0x48); X86(0x98)
#define X86_CALL(reg, preg) \
        X86(0x48); X86(0x8B); X86(0x55); X86(RBP(preg)); /* mov -preg(%ebp) %rax */ \
        X86(0xB8); X86I(0); /* mov 0x0 %eax */ \
        X86(0xFF); X86(0xD2); /* callq *%rdx */ \
        X86(0x48); X86(0x89); X86(0x45); X86(RBP(reg)); /* mov -preg(%ebp) %rax */
#endif

#define X86_MOV_RBP(reg, x) \
        X86_PRE(); X86(reg); X86(0x45); X86(RBP(x))
#define X86_MOVL(reg, x) ({ \
        int i, mreg, movl = sizeof(PN) / sizeof(int); \
        for (i = 0, mreg = (reg + 1) * movl; i < movl; i++) { \
          X86(0xC7); /* movl */ \
          X86(0x45); X86(RBPI(--mreg)); /* -A(%rbp) */ \
          X86I(*(((int *)&x)+i)); \
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

jit_t potion_x86_proto(Potion *P, PN proto) {
  long regs = 0;
  PN val;
  PN_OP *pos, *end;
  struct PNProto *f = (struct PNProto *)proto;
  u8 *start, *asmb;
  jit_t jit_func;
  jit_t *jit_protos = NULL;

  start = asmb = PN_ALLOC_FUNC(1024);
  jit_func = (jit_t)asmb;
  X86(0x55); // push %rbp
  X86_PRE(); X86(0x89); X86(0xE5); // mov %rsp,%rbp

  pos = ((PN_OP *)PN_STR_PTR(f->asmb));
  end = (PN_OP *)(PN_STR_PTR(f->asmb) + PN_STR_LEN(f->asmb));
  regs = PN_INT(f->stack);

  if (PN_IS_TUPLE(f->protos)) {
    jit_protos = PN_ALLOC_N(jit_t, PN_TUPLE_LEN(f->protos));
    PN_TUPLE_EACH(f->protos, i, proto2, {
      jit_protos[i] = potion_x86_proto(P, proto2);
    });
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
        X86_MOV_RBP(0x8B, pos->b);
        X86_MOV_RBP(0x89, regs + pos->a);
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
      case OP_CALL:
        // TODO: check for PNClosure and dispatch to C funcs / bytecode alike
        X86_CALL(pos->a, pos->b);
      break;
      case OP_RETURN:
        X86_MOV_RBP(0x8B, 0); /* mov -0(%rbp) %eax */ \
        X86(0xC9); X86(0xC3);
      break;
      case OP_PROTO: {
        jit_t func2 = jit_protos[pos->b];
        printf("MOVL %p TO %p: %d\n", asmb, func2, (int)((u8 *)func2 - asmb));
        // TODO: wrap function pointers in PNClosure
        X86_MOVL(pos->a, func2);
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
        PN_TUPLE_COUNT(((struct PNProto *)proto)->upvals, i, {
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
