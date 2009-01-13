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

extern char *potion_op_names[];
extern u8 potion_op_args[];

PN potion_vm_proto(Potion *P, PN cl, PN self, PN args) {
  return potion_vm(P, PN_CLOSURE(cl)->data[0], args,
    PN_CLOSURE(cl)->extra - 1, &PN_CLOSURE(cl)->data[1]);
}

#define STACK_MAX 1048576

#ifdef X86_JIT

typedef struct {
  u8 *start, *ptr;
  long capa;
} PNAsm;

static void potion_x86_put(PNAsm *asmb, PN val, size_t len) {
  if (asmb->capa - (asmb->ptr - asmb->start) < len) {
    size_t dist = asmb->ptr - asmb->start;
    asmb->capa += 4096;
    asmb->start = PN_REALLOC_N(asmb->start, u8, asmb->capa);
    asmb->ptr = asmb->start + dist;
  }

  if (len == sizeof(u8))
    *asmb->ptr = (u8)val;
  else if (len == sizeof(int))
    *((int *)asmb->ptr) = (int)val;
  else if (len == sizeof(PN))
    *((PN *)asmb->ptr) = val;
  asmb->ptr += len;
}

#define RBP(x)  (0x100 - ((x + 1) * sizeof(PN)))
#define RBPI(x) (0x100 - ((x + 1) * sizeof(int)))
#define X86(ins) potion_x86_put(asmb, (PN)ins, sizeof(u8))
#define X86I(pn) potion_x86_put(asmb, (PN)pn, sizeof(int))
#define X86N(pn) potion_x86_put(asmb, (PN)pn, sizeof(PN))

#if __WORDSIZE != 64
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
        X86_PRE(); X86(0x8B); X86(0x55); X86(RBP(pos->a)); /* mov -A(%rbp) %edx */ \
        X86(0xD1); X86(0xFA); /* sar %edx */ \
        X86_MOV_RBP(0x8B, pos->b); /* mov -B(%rbp) %eax */ \
        X86(0xD1); X86(0xF8); /* sar %eax */ \
        op; /* add, sub, ... */ \
        X86_POST(); /* cltq */ \
        X86_PRE(); X86(0x01); X86(0xC0); /* add %rax %rax */ \
        X86_PRE(); X86(0x83); X86(0xC8); X86(0x01); /* or 0x1 %eax */ \
        X86_MOV_RBP(0x89, pos->a); /* mov -B(%rbp) %eax */
#define X86_CMP(op) \
        X86_PRE(); X86(0x8B); X86(0x55); X86(RBP(pos->a)); /*  mov -A(%rbp) %edx */ \
        X86(0xD1); X86(0xFA); /*  sar %edx */ \
        X86_MOV_RBP(0x8B, pos->b); /*  mov -B(%rbp) %eax */ \
        X86(0xD1); X86(0xF8); /*  sar %eax */ \
        X86(0x39); X86(0xC2); /*  cmp %eax %edx */ \
        X86(op); X86(0x9 + X86_PRE_T); /*  jle +10 */ \
        X86_MOVQ(pos->a, PN_TRUE); /*  -A(%rbp) = TRUE */ \
        X86(0xEB); X86(0x7 + X86_PRE_T); /*  jmp +7 */ \
        X86_MOVQ(pos->a, PN_FALSE) /*  -A(%rbp) = FALSE */
#define X86_ARGO(regn, argn) potion_x86_c_arg(asmb, 1, regn, argn)
#define X86_ARGI(regn, argn) potion_x86_c_arg(asmb, 0, regn, argn)
#define TAG_JMP(jpos) \
        X86(0xE9); \
        if (jpos >= pos) { \
          jmps[jmpc].from = asmb->ptr - asmb->start; \
          X86I(0); \
          jmps[jmpc].to = jpos + 1; \
          jmpc++; \
        } else if (jpos < pos) { \
          X86I(offs[(jpos + 1) - start] - ((asmb->ptr - asmb->start) + 4)); \
        } else { \
          X86I(0); \
        }

PN potion_x86_debug(Potion *P, PN cl, PN v) {
  printf("\nDEBUG: %lu\n", v);
  return (PN)v;
}

struct PNJumps {
  size_t from;
  PN_OP *to;
};

#define MAX_JUMPS 1024

// mimick c calling convention
static void potion_x86_c_arg(PNAsm *asmb, int out, int regn, int argn) {
#if __WORDSIZE != 64
  if (argn == 0) {
    // OPT: the first argument is always (Potion *)
    if (!out) {
      X86_PRE(); X86(0x8b); X86(0x45); X86(2 * sizeof(PN));
      X86_PRE(); X86(0x89); X86(0x04); X86(0x24);
    }
  } else {
    if (out) { X86_MOV_RBP(0x8b, regn); }
    if (!out) argn += 2;
    if (out) {
      X86_PRE(); X86(0x89); X86(0x44); X86(0x24); X86(argn * sizeof(PN));
    } else {
      X86_PRE(); X86(0x8b); X86(0x45); X86(argn * sizeof(PN));
    }
    if (!out) { X86_MOV_RBP(0x89, regn); }
  }
#else
  switch (argn) {
    case 0:
      X86_PRE(); X86(out ? 0x8b : 0x89); X86(0x7d); X86(RBP(regn));
    break;
    case 1:
      X86_PRE(); X86(out ? 0x8b : 0x89); X86(0x75); X86(RBP(regn));
    break;
    case 2:
      X86_PRE(); X86(out ? 0x8b : 0x89); X86(0x55); X86(RBP(regn));
    break;
    case 3:
      X86_PRE(); X86(out ? 0x8b : 0x89); X86(0x4d); X86(RBP(regn));
    break;
    case 4:
      X86(0x4c); X86(out ? 0x8b : 0x89); X86(0x85); X86(RBP(regn));
      X86(0xff); X86(0xff); X86(0xff);
    break;
    case 5:
      X86(0x4c); X86(out ? 0x8b : 0x89); X86(0x8d); X86(RBP(regn));
      X86(0xff); X86(0xff); X86(0xff);
    break;
    default:
      if (out) X86_MOV_RBP(0x8b, regn);
      X86_PRE(); X86(out ? 0x89 : 0x8b); X86(0x44); X86(0x24); X86((argn - 4) * sizeof(PN));
      if (!out) X86_MOV_RBP(0x89, regn);
    break;
  }
#endif
}

PN_F potion_x86_proto(Potion *P, PN proto) {
  long regs = 0, lregs = 0, need = 0, rsp = 0, argx = 0, protoargs = 4;
  PN val;
  PN_OP *start, *pos, *end;
  struct PNJumps jmps[MAX_JUMPS]; size_t offs[MAX_JUMPS]; int jmpc = 0, jmpi = 0;
  struct PNProto *f = (struct PNProto *)proto;
  int upi, upc = PN_TUPLE_LEN(f->upvals);
  int movl = sizeof(PN) / sizeof(int);
  PNAsm *asmb = PN_ALLOC(PNAsm);
  u8 *fn;
  PN_F *jit_protos = NULL;
  long icsize = 0;

  asmb->start = asmb->ptr = PN_ALLOC_N(u8, (asmb->capa = 4096));
  X86(0x55); // push %rbp
  X86_PRE(); X86(0x89); X86(0xE5); // mov %rsp,%rbp

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
      jit_protos[i] = potion_x86_proto(P, proto2);
      if (p2args > protoargs)
        protoargs = p2args;
    });
  }

  regs = PN_INT(f->stack);
  lregs = regs + PN_TUPLE_LEN(f->locals);
  need = lregs + upc + 3;
  rsp = (need + protoargs) * sizeof(PN);

  /* maintain 16-byte stack alignment.  OS X in particular requires it, because
   * it expects to be able to use movdqa on things on the stack.
   * we factor in the offset from our saved ebp and return address, so that
   * adds 8 for x86 and 0 (mod 16) for x86_64.  */
  rsp = X86C(8,0)+((rsp+15)&~(15)); 
  if (rsp >= 0x80) {
    X86_PRE(); X86(0x81); X86(0xEC); X86I(rsp); /* sub rsp, %esp */
  } else {
    X86_PRE(); X86(0x83); X86(0xEC); X86(rsp); /* sub rsp, %esp */
  }

  // (Potion *, self) in the first argument slot 
  X86_ARGI(need - 2, 0);
  X86_ARGI(need - 1, 2);

  // Read locals
  for (argx = regs; argx < lregs + upc; argx++) {
    X86_MOVQ(argx, PN_NIL);
  }
  if (PN_IS_TUPLE(f->sig)) {
    argx = 0;
    PN_TUPLE_EACH(f->sig, i, v, {
      if (PN_IS_STR(v)) {
        PN_SIZE num = PN_GET(f->locals, v);
        X86_ARGI(regs + num, 3 + argx);
        argx++;
      }
    });
  }

  // if CL passed in with upvals, load them
  // TODO: optimize the PN_IS_CLOSURE call here
  if (upc > 0) {
    for (upi = 0; upi < upc; upi++) {
      X86_ARGI(0, 1);
      X86_MOV_RBP(0x8B, 0);
      X86_PRE(); X86(0x8B); X86(0x40);
        X86(sizeof(struct PNClosure) + (upi * sizeof(PN))); // 0x30(%rax)
      X86_MOV_RBP(0x89, lregs + upi);
      upc--;
    }
  }

  while (pos < end) {
    offs[pos - start] = asmb->ptr - asmb->start;
    for (jmpi = 0; jmpi < jmpc; jmpi++) {
      if (jmps[jmpi].to == pos) {
        u8 *asmj = asmb->start + jmps[jmpi].from;
        *((int *)asmj) = (int)((asmb->ptr - asmb->start) - (jmps[jmpi].from + 4));
      }
    }

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
        // TODO: optimize to do the ref check only if there are upvals
        X86_MOV_RBP(0x8B, regs + pos->b); // mov %rsp(B) %rax
        if (jit_protos != NULL) {
          // TODO: optimize to use %rdx rather than jmp
          X86(0x83); X86(0xE0); X86(PN_PRIMITIVE); // and PRIM %eax
          X86(0x83); X86(0xF8); X86(PN_REF_FLAG); // cmp WEAK %eax
          X86(0x75); X86(X86C(11, 13)); // jne 13
          X86_MOV_RBP(0x8B, regs + pos->b); // mov %rsp(B) %rax
          X86(0x83); X86(0xF0); X86(PN_REF_FLAG); // xor REF %eax
          X86_PRE(); X86(0x8B); X86(0x40); X86(sizeof(PN_GC)); // mov %rax.data %rax
          X86(0xEB); X86(X86C(3, 4)); //  jmp 4
          X86_MOV_RBP(0x8B, regs + pos->b); // mov %rsp(B) %rax
        }
        X86_MOV_RBP(0x89, pos->a);
      break;
      case OP_SETLOCAL:
        X86_PRE(); X86(0x8B); X86(0x55); X86(RBP(pos->a)); /*  mov -A(%rbp) %edx */
        if (jit_protos != NULL) {
          // TODO: optimize to use %rdx rather than jmp
          X86_MOV_RBP(0x8B, regs + pos->b); // mov %rsp(B) %rax
          X86(0x83); X86(0xE0); X86(PN_PRIMITIVE); // and PRIM %eax
          X86(0x83); X86(0xF8); X86(PN_REF_FLAG); // cmp WEAK %eax
          X86(0x75); X86(X86C(11, 13)); // jne 13
          X86_MOV_RBP(0x8B, regs + pos->b); // mov %rsp(B) %rax
          X86(0x83); X86(0xF0); X86(PN_REF_FLAG); // xor REF %eax
          X86_PRE(); X86(0x89); X86(0x50); X86(sizeof(PN_GC)); // mov %rdx %rax.data
          X86(0xEB); X86(X86C(3, 4)); //  jmp 4
        }
        X86_PRE(); X86(0x89); X86(0x55); X86(RBP(regs + pos->b)); // mov %rdx %rsp(B)
      break;
      case OP_GETUPVAL:
        X86_MOV_RBP(0x8B, lregs + pos->b);
        X86_PRE(); X86(0x8B); X86(0x40); X86(sizeof(PN_GC));
        X86_MOV_RBP(0x89, pos->a);
      break;
      case OP_SETUPVAL:
        X86_PRE(); X86(0x8B); X86(0x55); X86(RBP(pos->a)); /*  mov -A(%rbp) %edx */
        X86_MOV_RBP(0x8B, lregs + pos->b); // mov %rsp(B) %rax
        X86_PRE(); X86(0x89); X86(0x50); X86(sizeof(PN_GC)); // mov %rdx %rax.data
      break;
      case OP_NEWTUPLE:
        X86_ARGO(need - 2, 0);
        X86_PRE(); X86(0xB8); X86N(potion_tuple_empty); // mov &potion_tuple_push %rax
        X86(0xFF); X86(0xD0); // callq %rax
        X86_MOV_RBP(0x89, pos->a); // mov %rax local
      break;
      case OP_SETTUPLE:
        X86_ARGO(need - 2, 0);
        X86_ARGO(pos->a, 1);
        X86_ARGO(pos->b, 2);
        X86_PRE(); X86(0xB8); X86N(potion_tuple_push); // mov &potion_tuple_push %rax
        X86(0xFF); X86(0xD0); // callq %rax
        X86_MOV_RBP(0x89, pos->a); // mov %rax local
      break;
      case OP_SETTABLE:
        val = PN_TUPLE_AT(f->values, pos->b);
        X86_ARGO(need - 2, 0);
        X86_ARGO(pos->a, 1);
        X86_MOVQ(pos->a, val);
        X86_ARGO(pos->a, 2);
        X86_ARGO(pos->a + 1, 3);
        X86_PRE(); X86(0xB8); X86N(potion_table_set); // mov &potion_tuple_push %rax
        X86(0xFF); X86(0xD0); // callq %rax
        X86_MOV_RBP(0x89, pos->a); // mov %rax local
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
      case OP_DIV:
        X86_MATH({
          X86(0x89); X86(0xC1); // mov %eax %ecx
          X86(0x89); X86(0xD0); // mov %edx %eax
          X86(0xC1); X86(0xFA); X86(0x1F); // sar 0x1f %edx
          X86(0xF7); X86(0xF9); // idiv %ecx
        });
      break;
      case OP_REM:
        X86_MATH({
          X86(0x89); X86(0xC1); // mov %eax %ecx
          X86(0x89); X86(0xD0); // mov %edx %eax
          X86(0xC1); X86(0xFA); X86(0x1F); // sar 0x1f %edx
          X86(0xF7); X86(0xF9); // idiv %ecx
          X86(0x89); X86(0xD0); // mov %edx %eax
        });
      break;
      case OP_POW:
        X86_ARGO(need - 2, 0);
        X86_ARGO(need - 1, 1);
        X86_ARGO(pos->a, 2);
        X86_ARGO(pos->b, 3);
        X86_PRE(); X86(0xB8); X86N(potion_pow); // mov &potion_tuple_push %rax
        X86(0xFF); X86(0xD0); // callq %rax
        X86_MOV_RBP(0x89, pos->a); // mov %rax local
      break;
      case OP_NEQ:
        X86_CMP(0x74); // je
      break;
      case OP_EQ:
        X86_CMP(0x75); // jne
      break;
      case OP_LT:
        X86_CMP(0x7D); // jge
      break;
      case OP_LTE:
        X86_CMP(0x7F); // jg
      break;
      case OP_GT:
        X86_CMP(0x7E); // jle
      break;
      case OP_GTE:
        X86_CMP(0x7C); // jl
      break;
      case OP_BITL:
        X86_MATH({
          X86(0x89); X86(0xC1); // mov %eax %ecx
          X86(0x89); X86(0xD0); // mov %edx %eax
          X86(0xD3); X86(0xE0); // sar %cl %eax
        });
      break;
      case OP_BITR:
        X86_MATH({
          X86(0x89); X86(0xC1); // mov %eax %ecx
          X86(0x89); X86(0xD0); // mov %edx %eax
          X86(0xD3); X86(0xF8); // sar %cl %eax
        });
      break;
      case OP_BIND:
#ifdef JIT_ICACHE
#define X86S(N) X86I((icsize + (N * sizeof(PN))) | ICACHE_MASK)
        // place the receiver's class in %eax
        X86_PRE(); X86(0xB8); X86I(1); // mov 0x1 %rax
        X86_PRE(); X86(0x8B); X86(0x55); X86(RBP(pos->b)); // mov %rbp(B) %rdx
        X86(0xF6); X86(0xC2); X86(0x01); // test 0x1 %dl
        X86(0x75); X86(X86C(21, 23)); // jne [b]
        X86(0xF7); X86(0xC2); X86I(PN_REF_MASK); // test REFMASK %edx
        X86(0x75); X86(X86C(7, 8)); // jne [a]
        X86_PRE(); X86(0x89); X86(0xD0); // mov %rdx %rax
        X86(0x83); X86(0xE0); X86(PN_PRIMITIVE); // and 0x7 %eax
        X86(0xEB); X86(X86C(6, 7)); // jmp [b]
        X86(0x83); X86(0xE2); X86(0xF8); // [a] and ~PRIMITIVE %edx
        X86_PRE(); X86(0x8B); X86(0x42); X86(sizeof(PN_GC)); // %rdx.vt %rax
        // [b] compare to %rip(N)+0
        X86_PRE(); X86(0x89); X86(0xC2); // mov %rax %rdx
        X86_PRE(); X86(0x8B); X86(0x05); X86S(0); // mov %rip(N)+0 %rax
        X86_PRE(); X86(0x39); X86(0xC2); // cmp %rax %rdx
        X86(0x75); X86(X86C(15, 18)); // jne [c]
        // compare %rbp(A) and %rip(N)+1
        X86_PRE(); X86(0x8B); X86(0x55); X86(RBP(pos->a)); // mov %rbp(A) %rdx
        X86_PRE(); X86(0x8B); X86(0x05); X86S(1); // mov %rip(N)+1 %rax
        X86_PRE(); X86(0x39); X86(0xC2); // cmp %rax %rdx
        X86(0x75); X86(X86C(10, 12)); // jne [d]
        X86(0xEB); X86(X86C(46, 60)); // jmp [e]
        // [c] cache new type
        X86_PRE(); X86(0x89); X86(0xD0); // mov %rdx %rax
        X86_PRE(); X86(0x89); X86(0x05); X86S(0); // mov %rax %rip(N)+0
        // [d] cache new method
        X86_MOV_RBP(0x8B, pos->a); // mov %rbp(A) %rax
        X86_PRE(); X86(0x89); X86(0x05); X86S(1); // mov %rax %rip(N)+1
#endif
        X86_ARGO(need - 2, 0); // (0, 3)
        X86_ARGO(pos->b, 1); // (7, 3)
        X86_ARGO(pos->a, 2); // (7, 3)
        X86_PRE(); X86(0xB8); X86N(potion_bind); // mov &potion_bind %rax
        X86(0xFF); X86(0xD0); // callq %rax
#ifdef JIT_ICACHE
        X86_PRE(); X86(0x89); X86(0x05); X86S(3); // mov %rax %rip(N)+3
        X86(0xEB); X86(X86C(6, 7)); // jmp over [e]
        // [e] load cached method
        X86_PRE(); X86(0x8B); X86(0x05); X86S(3); // mov %rip(N)+3 %rax
        icsize += sizeof(struct PNInlineCache);
#endif
        X86_MOV_RBP(0x89, pos->a); // mov %rax local
      break;
      case OP_JMP:
        TAG_JMP(pos + pos->a);
      break;
      case OP_TEST:
      case OP_NOT:
        X86_PRE(); X86(0x8B); X86(0x55); X86(RBP(pos->a)); /*  mov -A(%rbp) %edx */
        X86(0xB8); X86I(PN_FALSE); /* mov FALSE %eax */
        X86(0x39); X86(0xC2); /*  cmp %eax %edx */
        X86(pos->code == OP_TEST ? 0x75 : 0x74); X86(0x9 + X86_PRE_T); /*  jne +10 */ \
        X86_MOVQ(pos->a, PN_TRUE); /*  -A(%rbp) = TRUE */ \
        X86(0xEB); X86(0x7 + X86_PRE_T); /*  jmp +7 */ \
        X86_MOVQ(pos->a, PN_FALSE); /*  -A(%rbp) = FALSE */
      break;
      case OP_TESTJMP:
        X86_PRE(); X86(0x8B); X86(0x55); X86(RBP(pos->a)); /*  mov -A(%rbp) %edx */
        X86(0xB8); X86I(PN_FALSE); /* mov FALSE %eax */
        X86(0x39); X86(0xC2); /*  cmp %eax %edx */
        X86(0x74); X86(0x5); /*  je +10 */
        TAG_JMP(pos + pos->b);
      break;
      case OP_NOTJMP:
        X86_PRE(); X86(0x8B); X86(0x55); X86(RBP(pos->a)); /*  mov -A(%rbp) %edx */
        X86(0xB8); X86I(PN_FALSE); /* mov FALSE %eax */
        X86(0x39); X86(0xC2); /* cmp %eax %edx */
        X86(0x75); X86(0x5); /* jne +10 */
        TAG_JMP(pos + pos->b);
      break;
      case OP_CALL: {
        int argc = pos->b - pos->a;
        // (Potion *, CL) as the first argument
        X86_ARGO(need - 2, 0);
        X86_ARGO(pos->b, 1);
        while (--argc >= 0) X86_ARGO(pos->a + argc, argc + 2);
        // TODO: check for bytecode nodes and jit them as well?
        X86_PRE(); X86(0x8B); X86(0x45); X86(RBP(pos->b)); /* mov -preg(%ebp) %rax */
        X86_PRE(); X86(0x8B); X86(0x40); X86(sizeof(struct PNObject)); /* mov N(%rax) %rax */
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
        cl = (struct PNClosure *)potion_closure_new(P, NULL, PN_NIL,
          PN_TUPLE_LEN(PN_PROTO(proto)->upvals));
        // func2 = &potion_x86_debug;
        cl->method = (PN_F)func2;
        X86_MOVL(pos->a, cl);
        PN_TUPLE_COUNT(PN_PROTO(proto)->upvals, i, {
          pos++;
          if (pos->code == OP_GETUPVAL) {
            X86_PRE(); X86(0x8B); X86(0x55); X86(RBP(lregs + pos->b)); // mov upval %rdx
          } else if (pos->code == OP_GETLOCAL) {
            X86_ARGO(need - 2, 0);
            X86_ARGO(regs + pos->b, 1);
            X86_PRE(); X86(0xB8); X86N(potion_ref); // mov &potion_ref %rax
            X86(0xFF); X86(0xD0); // callq %rax
            X86_MOV_RBP(0x89, regs + pos->b); // mov %rax local
            X86(0x83); X86(0xF0); X86(0x04); // xor REF %rax
            X86_PRE(); X86(0x89); X86(0xC2); // mov %rax %rdx
          } else {
            fprintf(stderr, "** missing an upval to proto %p\n", (void *)proto);
          }
          X86_MOV_RBP(0x8B, pos->a); // mov cl %rax
          X86_PRE(); X86(0x89); X86(0x50); // mov %rdx N(%rax)
            X86(sizeof(struct PNClosure) + (sizeof(PN) * i));
        });
      }
      break;
    }
    pos++;
  }

  asmb->capa = asmb->ptr - asmb->start;
#ifdef JIT_ICACHE
  if (icsize > 0) {
    long ici = 0; int *ipos, ival; 
    for (ici = 0; ici < asmb->capa; ici++) {
      if (asmb->start[ici] != 0x89 && asmb->start[ici] != 0x8b)
        continue;
      if (asmb->start[ici+1] != 0x05) continue;
      ipos = (int *)(asmb->start+ici+2);
      if (!(*ipos & ICACHE_MASK))     continue;
      ival = *ipos ^ ICACHE_MASK;
#if __WORDSIZE != 64
      if (asmb->start[ici] == 0x89)
        asmb->start[ici] = 0xA3;   // mov %eax MEMOFF
      else
        asmb->start[ici] = 0xA1;   // mov MEMOFF %eax
      asmb->start[ici+5] = 0x90; // nop
      ipos = (int *)(asmb->start+ici+1);
#endif
      *ipos = asmb->ptr + asmb->capa + ival;
    }
  }
#endif

  fn = PN_ALLOC_FUNC(asmb->capa + icsize);
#ifdef DEBUG
  printf("JIT(%p): ", fn);
  for (icsize = 0; icsize < asmb->capa; icsize++) {
    printf("%x ", asmb->start[icsize]);
  }
#endif
  PN_MEMCPY_N(fn, asmb->start, u8, asmb->capa);
  PN_FREE(asmb->start);
  PN_FREE(asmb);

  return (PN_F)fn;
}
#endif

PN potion_vm(Potion *P, PN proto, PN vargs, PN_SIZE upc, PN* upargs) {
  struct PNProto *f = (struct PNProto *)proto;

  // these variables persist as we jump around
  PN *stack = PN_ALLOC_N(PN, STACK_MAX);
  PN val = PN_NIL;

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
    reg[0] = PN_VTABLE(PN_TLOBBY);
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
    // printf("CODE: %s\n", potion_op_names[pos->code]);
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
      case OP_NEWTUPLE:
        reg[pos->a] = PN_TUP0();
      break;
      case OP_SETTUPLE:
        reg[pos->a] = PN_PUSH(reg[pos->a], reg[pos->b]);
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
