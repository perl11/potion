//
// vm-x86.c
// the x86 and x86_64 jit
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "potion.h"
#include "internal.h"
#include "opcodes.h"
#include "asm.h"

#define RBP(x)  (0x100 - ((x + 1) * sizeof(PN)))
#define RBPI(x) (0x100 - ((x + 1) * sizeof(int)))

#if __WORDSIZE != 64
#define X86_PRE_T 0
#define X86_PRE()
#define X86_POST()
#define X86C(op32, op64) op32
#else
#define X86_PRE_T 1
#define X86_PRE()  ASM(0x48)
#define X86_POST() ASM(0x48); ASM(0x98)
#define X86C(op32, op64) op64
#endif

#define X86_MOV_RBP(reg, x) \
        X86_PRE(); ASM(reg); ASM(0x45); ASM(RBP(x))
#define X86_MOVL(reg, x) ({ \
        int i, mreg, movl = sizeof(PN) / sizeof(int); \
        for (i = 0, mreg = (reg + 1) * movl; i < movl; i++) { \
          int *xp = (int *)&x; \
          ASM(0xC7); /* movl */ \
          ASM(0x45); ASM(RBPI(--mreg)); /* -A(%rbp) */ \
          ASMI(*(xp+i)); \
        } \
})
#define X86_MOVQ(reg, x) \
        X86_PRE(); ASM(0xC7); /* movl */ \
        ASM(0x45); ASM(RBP(reg)); /* -A(%rbp) */ \
        ASMI((PN)(x))
#define X86_UNBOX() \
        X86_PRE(); ASM(0x8B); ASM(0x55); ASM(RBP(op->a)); /* mov -A(%rbp) %edx */ \
        ASM(0xD1); ASM(0xFA); /* sar %edx */ \
        X86_MOV_RBP(0x8B, op->b); /* mov -B(%rbp) %eax */ \
        ASM(0xD1); ASM(0xF8) /* sar %eax */
#define X86_MATH(do) \
        X86_UNBOX(); \
        do; /* add, sub, ... */ \
        X86_POST(); /* cltq */ \
        X86_PRE(); ASM(0x8D); ASM(0x44); ASM(0x00); ASM(0x01); /* lea 0x1(%eax+%eax*1) %eax */ \
        X86_MOV_RBP(0x89, op->a); /* mov -B(%rbp) %eax */
#define X86_CMP(do) \
        X86_UNBOX(); \
        ASM(0x39); ASM(0xC2); /*  cmp %eax %edx */ \
        ASM(do); ASM(0x9 + X86_PRE_T); /*  jle +10 */ \
        X86_MOVQ(op->a, PN_TRUE); /*  -A(%rbp) = TRUE */ \
        ASM(0xEB); ASM(0x7 + X86_PRE_T); /*  jmp +7 */ \
        X86_MOVQ(op->a, PN_FALSE) /*  -A(%rbp) = FALSE */
#define X86_ARGO(regn, argn) potion_x86_c_arg(P, asmb, 1, regn, argn)
#define X86_ARGI(regn, argn) potion_x86_c_arg(P, asmb, 0, regn, argn)
#define TAG_JMP(jpos) \
        ASM(0xE9); \
        if (jpos >= op) { \
          jmps[*jmpc].from = asmb->len; \
          ASMI(0); \
          jmps[*jmpc].to = jpos + 1; \
          *jmpc = *jmpc + 1; \
        } else if (jpos < op) { \
          ASMI(offs[(jpos + 1) - start] - ((asmb->len) + 4)); \
        } else { \
          ASMI(0); \
        }

PN potion_x86_debug(Potion *P, PN cl, PN v) {
  printf("\nDEBUG: %lu\n", v);
  return (PN)v;
}

// mimick c calling convention
static void potion_x86_c_arg(Potion *P, PNAsm *asmb, int out, int regn, int argn) {
#if __WORDSIZE != 64
  if (argn == 0) {
    // OPT: the first argument is always (Potion *)
    if (!out) {
      X86_PRE(); ASM(0x8b); ASM(0x45); ASM(2 * sizeof(PN));
      X86_PRE(); ASM(0x89); ASM(0x04); ASM(0x24);
    }
  } else {
    if (out) { X86_MOV_RBP(0x8b, regn); }
    if (!out) argn += 2;
    if (out) {
      X86_PRE(); ASM(0x89); ASM(0x44); ASM(0x24); ASM(argn * sizeof(PN));
    } else {
      X86_PRE(); ASM(0x8b); ASM(0x45); ASM(argn * sizeof(PN));
    }
    if (!out) { X86_MOV_RBP(0x89, regn); }
  }
#else
  switch (argn) {
    case 0:
      X86_PRE(); ASM(out ? 0x8b : 0x89); ASM(0x7d); ASM(RBP(regn));
    break;
    case 1:
      X86_PRE(); ASM(out ? 0x8b : 0x89); ASM(0x75); ASM(RBP(regn));
    break;
    case 2:
      X86_PRE(); ASM(out ? 0x8b : 0x89); ASM(0x55); ASM(RBP(regn));
    break;
    case 3:
      X86_PRE(); ASM(out ? 0x8b : 0x89); ASM(0x4d); ASM(RBP(regn));
    break;
    case 4:
      ASM(0x4c); ASM(out ? 0x8b : 0x89); ASM(0x85); ASM(RBP(regn));
      ASM(0xff); ASM(0xff); ASM(0xff);
    break;
    case 5:
      ASM(0x4c); ASM(out ? 0x8b : 0x89); ASM(0x8d); ASM(RBP(regn));
      ASM(0xff); ASM(0xff); ASM(0xff);
    break;
    default:
      if (out) X86_MOV_RBP(0x8b, regn);
      X86_PRE(); ASM(out ? 0x89 : 0x8b); ASM(0x44); ASM(0x24); ASM((argn - 4) * sizeof(PN));
      if (!out) X86_MOV_RBP(0x89, regn);
    break;
  }
#endif
}

void potion_x86_setup(Potion *P, PNAsm *asmb) {
  ASM(0x55); // push %rbp
  X86_PRE(); ASM(0x89); ASM(0xE5); // mov %rsp,%rbp
}

void potion_x86_stack(Potion *P, PNAsm *asmb, long rsp) {
  /* maintain 16-byte stack alignment.  OS X in particular requires it, because
   * it expects to be able to use movdqa on things on the stack.
   * we factor in the offset from our saved ebp and return address, so that
   * adds 8 for x86 and 0 (mod 16) for x86_64.  */
  rsp = X86C(8,0)+((rsp-X86C(8,0)+15)&~(15));
  if (rsp >= 0x80) {
    X86_PRE(); ASM(0x81); ASM(0xEC); ASMI(rsp); /* sub rsp, %esp */
  } else {
    X86_PRE(); ASM(0x83); ASM(0xEC); ASM(rsp); /* sub rsp, %esp */
  }
}

void potion_x86_registers(Potion *P, PNAsm *asmb, long start) {
  // (Potion *, self) in the first argument slot, self in the first register 
  X86_ARGI(start - 2, 0);
  X86_ARGI(start - 1, 2);
  X86_ARGI(0, 2);
}

void potion_x86_local(Potion *P, PNAsm *asmb, long reg, long arg) {
  X86_ARGI(reg, 3 + arg);
}

void potion_x86_upvals(Potion *P, PNAsm *asmb, long lregs, int upc) {
  int upi;
  X86_ARGI(1, 1);
  for (upi = 0; upi < upc; upi++) {
    X86_MOV_RBP(0x8B, 1);
    X86_PRE(); ASM(0x8B); ASM(0x40);
      ASM(sizeof(struct PNClosure) + (upi * sizeof(PN))); // 0x30(%rax)
    X86_MOV_RBP(0x89, lregs + upi);
  }
}

void potion_x86_jmpedit(Potion *P, PNAsm *asmb, unsigned char *asmj, int dist) {
  *((int *)asmj) = dist;
}

void potion_x86_move(Potion *P, PNAsm *asmb, PN_OP *op) {
  X86_MOV_RBP(0x8B, op->b);
  X86_MOV_RBP(0x89, op->a);
}

void potion_x86_loadpn(Potion *P, PNAsm *asmb, PN_OP *op) {
  X86_MOVQ(op->a, op->b);
}

void potion_x86_loadk(Potion *P, PNAsm *asmb, PN_OP *op, PN values) {
  PN val = PN_TUPLE_AT(values, op->b);
  X86_MOVL(op->a, val);
}

void potion_x86_self(Potion *P, PNAsm *asmb, PN_OP *op, long start) {
  // TODO: optimize so that if this is followed by a BIND, it'll just
  // use the self register directly.
  X86_MOV_RBP(0x8B, start - 1);
  X86_MOV_RBP(0x89, op->a);
}

void potion_x86_getlocal(Potion *P, PNAsm *asmb, PN_OP *op, long regs, PN protos) {
  // TODO: optimize to do the ref check only if there are upvals
  X86_MOV_RBP(0x8B, regs + op->b); // mov %rsp(B) %rax
  if (PN_TUPLE_LEN(protos) > 0) {
    // TODO: optimize to use %rdx rather than jmp
    ASM(0x83); ASM(0xE0); ASM(PN_PRIMITIVE); // and PRIM %eax
    ASM(0x83); ASM(0xF8); ASM(PN_TWEAK); // cmp WEAK %eax
    ASM(0x75); ASM(X86C(11, 14)); // jne 13
    X86_MOV_RBP(0x8B, regs + op->b); // mov %rsp(B) %rax
    X86_PRE(); ASM(0x83); ASM(0xF0); ASM(PN_TWEAK); // xor REF %eax
    X86_PRE(); ASM(0x8B); ASM(0x40); ASM(sizeof(struct PNObject)); // mov %rax.data %rax
    ASM(0xEB); ASM(X86C(3, 4)); //  jmp 4
    X86_MOV_RBP(0x8B, regs + op->b); // mov %rsp(B) %rax
  }
  X86_MOV_RBP(0x89, op->a);
}

void potion_x86_setlocal(Potion *P, PNAsm *asmb, PN_OP *op, long regs, PN protos) {
  X86_PRE(); ASM(0x8B); ASM(0x55); ASM(RBP(op->a)); /*  mov -A(%rbp) %edx */
  if (PN_TUPLE_LEN(protos) > 0) {
    // TODO: optimize to use %rdx rather than jmp
    X86_MOV_RBP(0x8B, regs + op->b); // mov %rsp(B) %rax
    ASM(0x83); ASM(0xE0); ASM(PN_PRIMITIVE); // and PRIM %eax
    ASM(0x83); ASM(0xF8); ASM(PN_TWEAK); // cmp WEAK %eax
    ASM(0x75); ASM(X86C(11, 14)); // jne 13
    X86_MOV_RBP(0x8B, regs + op->b); // mov %rsp(B) %rax
    X86_PRE(); ASM(0x83); ASM(0xF0); ASM(PN_TWEAK); // xor REF %eax
    X86_PRE(); ASM(0x89); ASM(0x50); ASM(sizeof(struct PNObject)); // mov %rdx %rax.data
    ASM(0xEB); ASM(X86C(3, 4)); //  jmp 4
  }
  X86_PRE(); ASM(0x89); ASM(0x55); ASM(RBP(regs + op->b)); // mov %rdx %rsp(B)
}

void potion_x86_getupval(Potion *P, PNAsm *asmb, PN_OP *op, long lregs) {
  X86_MOV_RBP(0x8B, lregs + op->b);
  X86_PRE(); ASM(0x8B); ASM(0x40); ASM(sizeof(struct PNObject));
  X86_MOV_RBP(0x89, op->a);
}

void potion_x86_setupval(Potion *P, PNAsm *asmb, PN_OP *op, long lregs) {
  X86_PRE(); ASM(0x8B); ASM(0x55); ASM(RBP(op->a)); /*  mov -A(%rbp) %edx */
  X86_MOV_RBP(0x8B, lregs + op->b); // mov %rsp(B) %rax
  X86_PRE(); ASM(0x89); ASM(0x50); ASM(sizeof(struct PNObject)); // mov %rdx %rax.data
}

void potion_x86_newtuple(Potion *P, PNAsm *asmb, PN_OP *op, long start) {
  X86_ARGO(start - 2, 0);
  X86_PRE(); ASM(0xB8); ASMN(potion_tuple_empty); // mov &potion_tuple_push %rax
  ASM(0xFF); ASM(0xD0); // callq %rax
  X86_MOV_RBP(0x89, op->a); // mov %rax local
}

void potion_x86_settuple(Potion *P, PNAsm *asmb, PN_OP *op, long start) {
  X86_ARGO(start - 2, 0);
  X86_ARGO(op->a, 1);
  X86_ARGO(op->b, 2);
  X86_PRE(); ASM(0xB8); ASMN(potion_tuple_push); // mov &potion_tuple_push %rax
  ASM(0xFF); ASM(0xD0); // callq %rax
  X86_MOV_RBP(0x89, op->a); // mov %rax local
}

void potion_x86_settable(Potion *P, PNAsm *asmb, PN_OP *op, long start, PN values) {
  X86_ARGO(start - 2, 0);
  X86_ARGO(op->a, 1);
  X86_ARGO(op->b, 2);
  X86_ARGO(op->a + 1, 3);
  X86_PRE(); ASM(0xB8); ASMN(potion_table_set); // mov &potion_tuple_push %rax
  ASM(0xFF); ASM(0xD0); // callq %rax
  X86_MOV_RBP(0x89, op->a); // mov %rax local
}

void potion_x86_add(Potion *P, PNAsm *asmb, PN_OP *op) {
  X86_MATH({
    ASM(0x89); ASM(0xD1); // mov %rdx %rcx
    ASM(0x01); ASM(0xC1); // add %rax %rcx
    ASM(0x89); ASM(0xC8); // mov %rcx %rax
  });
}

void potion_x86_sub(Potion *P, PNAsm *asmb, PN_OP *op) {
  X86_MATH({
    ASM(0x89); ASM(0xD1); // mov %rdx %rcx
    ASM(0x29); ASM(0xC1); // sub %rax %rcx
    ASM(0x89); ASM(0xC8); // mov %rcx %rax
  });
}

void potion_x86_mult(Potion *P, PNAsm *asmb, PN_OP *op) {
  X86_MATH({
    ASM(0x0F); ASM(0xAF); ASM(0xC2); // imul %rdx %rax
  });
}

void potion_x86_div(Potion *P, PNAsm *asmb, PN_OP *op) {
  X86_MATH({
    ASM(0x89); ASM(0xC1); // mov %eax %ecx
    ASM(0x89); ASM(0xD0); // mov %edx %eax
    ASM(0xC1); ASM(0xFA); ASM(0x1F); // sar 0x1f %edx
    ASM(0xF7); ASM(0xF9); // idiv %ecx
  });
}

void potion_x86_rem(Potion *P, PNAsm *asmb, PN_OP *op) {
  X86_MATH({
    ASM(0x89); ASM(0xC1); // mov %eax %ecx
    ASM(0x89); ASM(0xD0); // mov %edx %eax
    ASM(0xC1); ASM(0xFA); ASM(0x1F); // sar 0x1f %edx
    ASM(0xF7); ASM(0xF9); // idiv %ecx
    ASM(0x89); ASM(0xD0); // mov %edx %eax
  });
}

void potion_x86_pow(Potion *P, PNAsm *asmb, PN_OP *op, long start) {
  X86_ARGO(start - 2, 0);
  X86_ARGO(op->a, 2);
  X86_ARGO(op->b, 3);
  X86_PRE(); ASM(0xB8); ASMN(potion_pow); // mov &potion_tuple_push %rax
  ASM(0xFF); ASM(0xD0); // callq %rax
  X86_MOV_RBP(0x89, op->a); // mov %rax local
}

void potion_x86_neq(Potion *P, PNAsm *asmb, PN_OP *op) {
  X86_CMP(0x74); // je
}

void potion_x86_eq(Potion *P, PNAsm *asmb, PN_OP *op) {
  X86_CMP(0x75); // jne
}

void potion_x86_lt(Potion *P, PNAsm *asmb, PN_OP *op) {
  X86_CMP(0x7D); // jge
}

void potion_x86_lte(Potion *P, PNAsm *asmb, PN_OP *op) {
  X86_CMP(0x7F); // jg
}

void potion_x86_gt(Potion *P, PNAsm *asmb, PN_OP *op) {
  X86_CMP(0x7E); // jle
}

void potion_x86_gte(Potion *P, PNAsm *asmb, PN_OP *op) {
  X86_CMP(0x7C); // jl
}

void potion_x86_bitl(Potion *P, PNAsm *asmb, PN_OP *op) {
  X86_MATH({
    ASM(0x89); ASM(0xC1); // mov %eax %ecx
    ASM(0x89); ASM(0xD0); // mov %edx %eax
    ASM(0xD3); ASM(0xE0); // sar %cl %eax
  });
}

void potion_x86_bitr(Potion *P, PNAsm *asmb, PN_OP *op) {
  X86_MATH({
    ASM(0x89); ASM(0xC1); // mov %eax %ecx
    ASM(0x89); ASM(0xD0); // mov %edx %eax
    ASM(0xD3); ASM(0xF8); // sar %cl %eax
  });
}

void potion_x86_bind(Potion *P, PNAsm *asmb, PN_OP *op, long start) {
#ifdef JIT_ICACHE
  u8 *ictype, *icname;
  // place the receiver's class in %eax
  ASM(0xB8); ASMI(1); // mov 0x1 %rax
  X86_PRE(); ASM(0x8B); ASM(0x55); ASM(RBP(op->b)); // mov %rbp(B) %rdx
  ASM(0xF6); ASM(0xC2); ASM(0x01); // test 0x1 %dl
  ASM(0x75); ASM(X86C(21, 24)); // jne [b]
  ASM(0xF7); ASM(0xC2); ASMI(PN_REF_MASK); // test REFMASK %edx
  ASM(0x75); ASM(X86C(7, 8)); // jne [a]
  X86_PRE(); ASM(0x89); ASM(0xD0); // mov %rdx %rax
  ASM(0x83); ASM(0xE0); ASM(PN_PRIMITIVE); // and 0x7 %eax
  ASM(0xEB); ASM(X86C(6, 8)); // jmp [b]
  X86_PRE(); ASM(0x83); ASM(0xE2); ASM(0xF8); // [a] and ~PRIMITIVE %edx
  X86_PRE(); ASM(0x8B); ASM(0x42); ASM(0); // %rdx.vt %rax
  // [b] compare to TYPE 
  X86_PRE(); ASM(0x89); ASM(0xC2); // mov %rax %rdx
  X86_PRE(); ASM(0xB8); ictype = asmb->ptr; ASMN(0); // mov TYPE %rax
  X86_PRE(); ASM(0x39); ASM(0xC2); // cmp %rax %rdx
  ASM(0x75); ASM(X86C(14, 21)); // jne [c]
  // compare %rbp(A) and NAME
  X86_PRE(); ASM(0x8B); ASM(0x55); ASM(RBP(op->a)); // mov %rbp(A) %rdx
  X86_PRE(); ASM(0xB8); icname = asmb->ptr; ASMN(0); // mov NAME %rax
  X86_PRE(); ASM(0x39); ASM(0xC2); // cmp %rax %rdx
  ASM(0x75); ASM(X86C(10, 12)); // jne [d]
  ASM(0xEB); ASM(X86C(46, 55)); // jmp [e]
  // [c] cache new type
  X86_PRE(); ASM(0x89); ASM(0xD0); // mov %rdx %rax
  X86_PRE(); ASM(0x89); ASM(0x05); ASMI(ictype - (asmb->ptr + 4)); // mov %rax TYPE
  // [d] cache new method
  X86_MOV_RBP(0x8B, op->a); // mov %rbp(A) %rax
  X86_PRE(); ASM(0x89); ASM(0x05); ASMI(icname - (asmb->ptr + 4)); // mov %rax NAME 
#endif
  X86_ARGO(start - 2, 0); // (0, 3)
  X86_ARGO(op->b, 1); // (7, 3)
  X86_ARGO(op->a, 2); // (7, 3)
  X86_PRE(); ASM(0xB8); ASMN(potion_bind); // mov &potion_bind %rax
  ASM(0xFF); ASM(0xD0); // callq %rax
#ifdef JIT_ICACHE
  X86_PRE(); ASM(0x89); ASM(0x05); ASMI(X86C(3, 4)); // mov %rax CLO
  ASM(0xEB); ASM(X86C(1, 2) + sizeof(PN)); // jmp over [e]
  // [e] load cached method
  X86_PRE(); ASM(0xB8); ASMN(0); // mov CLO %rax
#endif
  X86_MOV_RBP(0x89, op->a); // mov %rax local
}

void potion_x86_jmp(Potion *P, PNAsm *asmb, PN_OP *op, PN_OP *start, PNJumps *jmps, size_t *offs, int *jmpc) {
  TAG_JMP(op + op->a);
}

void potion_x86_test_asm(Potion *P, PNAsm *asmb, PN_OP *op, int test) {
  X86_PRE(); ASM(0x8B); ASM(0x55); ASM(RBP(op->a)); /*  mov -A(%rbp) %edx */
  ASM(0xB8); ASMI(PN_FALSE); /* mov FALSE %eax */
  ASM(0x39); ASM(0xC2); /*  cmp %eax %edx */
  ASM(test ? 0x75 : 0x74); ASM(0x9 + X86_PRE_T); /*  jne +10 */ \
  X86_MOVQ(op->a, PN_TRUE); /*  -A(%rbp) = TRUE */ \
  ASM(0xEB); ASM(0x7 + X86_PRE_T); /*  jmp +7 */ \
  X86_MOVQ(op->a, PN_FALSE); /*  -A(%rbp) = FALSE */
}

void potion_x86_test(Potion *P, PNAsm *asmb, PN_OP *op) {
  potion_x86_test_asm(P, asmb, op, 0);
}

void potion_x86_not(Potion *P, PNAsm *asmb, PN_OP *op) {
  potion_x86_test_asm(P, asmb, op, 1);
}

void potion_x86_cmp(Potion *P, PNAsm *asmb, PN_OP *op) {
  potion_x86_test_asm(P, asmb, op, 0);
}

void potion_x86_testjmp(Potion *P, PNAsm *asmb, PN_OP *op, PN_OP *start, PNJumps *jmps, size_t *offs, int *jmpc) {
  X86_PRE(); ASM(0x8B); ASM(0x55); ASM(RBP(op->a)); /*  mov -A(%rbp) %edx */
  ASM(0xB8); ASMI(PN_FALSE); /* mov FALSE %eax */
  ASM(0x39); ASM(0xC2); /*  cmp %eax %edx */
  ASM(0x74); ASM(0x5); /*  je +10 */
  TAG_JMP(op + op->b);
}

void potion_x86_notjmp(Potion *P, PNAsm *asmb, PN_OP *op, PN_OP *start, PNJumps *jmps, size_t *offs, int *jmpc) {
  X86_PRE(); ASM(0x8B); ASM(0x55); ASM(RBP(op->a)); /*  mov -A(%rbp) %edx */
  ASM(0xB8); ASMI(PN_FALSE); /* mov FALSE %eax */
  ASM(0x39); ASM(0xC2); /* cmp %eax %edx */
  ASM(0x75); ASM(0x5); /* jne +10 */
  TAG_JMP(op + op->b);
}

// TODO: check for bytecode nodes and jit them as well?
void potion_x86_call(Potion *P, PNAsm *asmb, PN_OP *op, long start) {
  int argc = op->b - op->a;
  // (Potion *, CL) as the first argument
  X86_ARGO(start - 2, 0);
  X86_ARGO(op->b, 1);
  while (--argc >= 0) X86_ARGO(op->a + argc, argc + 2);

  // check type of the closure
  X86_PRE(); ASM(0x8B); ASM(0x45); ASM(RBP(op->b)); // mov %rbp(B) %rax
  ASM(0xF6); ASM(0xC0); ASM(0x01); // test 0x1 %al
  ASM(0x75); ASM(X86C(27, 30)); // jne [a]
  ASM(0xF7); ASM(0xC0); ASMI(PN_REF_MASK); // test REFMASK %eax
  ASM(0x74); ASM(X86C(19, 22)); // je [a]
  X86_PRE(); ASM(0x83); ASM(0xE0); ASM(0xF8); // and ~PRIMITIVE %rax
  ASM(0x8B); ASM(0x40); ASM(0); // mov N(%rax) %rax
  ASM(0x83); ASM(0xF8); ASM(PN_TCLOSURE); // cmp CLOSURE %rax
  ASM(0x75); ASM(X86C(8, 10)); // jne [a]

  // if a closure, load the function pointer
  X86_PRE(); ASM(0x8B); ASM(0x45); ASM(RBP(op->b)); // mov %rbp(B) %rax
  X86_PRE(); ASM(0x8B); ASM(0x40); ASM(sizeof(struct PNObject)); // mov N(%rax) %rax
  ASM(0xEB); ASM(X86C(19, 22)); // jmp [b]

  // if not a closure, send to potion_jit_callout
  X86_MOVQ(op->a, op->b - op->a - 1); // mov -A(%rbp) NUM
  X86_ARGO(op->a, 2);
  X86_PRE(); ASM(0xB8); ASMN(potion_obj_call); // mov &potion_obj_call %rax

  ASM(0xFF); ASM(0xD0); // [b] callq *%rax
  X86_PRE(); ASM(0x89); ASM(0x45); ASM(RBP(op->a)); /* mov %rbp(A) %rax */
}

void potion_x86_return(Potion *P, PNAsm *asmb, PN_OP *op) {
  X86_MOV_RBP(0x8B, 0); /* mov -0(%rbp) %eax */ \
  ASM(0xC9); ASM(0xC3); /* leave; ret */
}

void potion_x86_method(Potion *P, PNAsm *asmb, PN_OP **pos, PN protos, long lregs, long start, long regs) {
  vPN(Closure) cl;
  PN_OP *op = *pos;
  PN proto = PN_TUPLE_AT(protos, op->b);
  cl = (struct PNClosure *)potion_closure_new(P, NULL, PN_NIL,
    PN_TUPLE_LEN(PN_PROTO(proto)->upvals));
  // func2 = &potion_x86_debug;
  cl->method = PN_PROTO(proto)->jit;
  X86_MOVL(op->a, cl);
  PN_TUPLE_COUNT(PN_PROTO(proto)->upvals, i, {
    op = *pos = *pos + 1;
    if (op->code == OP_GETUPVAL) {
      X86_PRE(); ASM(0x8B); ASM(0x55); ASM(RBP(lregs + op->b)); // mov upval %rdx
    } else if (op->code == OP_GETLOCAL) {
      X86_ARGO(start - 2, 0);
      X86_ARGO(regs + op->b, 1);
      X86_PRE(); ASM(0xB8); ASMN(potion_ref); // mov &potion_ref %rax
      ASM(0xFF); ASM(0xD0); // callq %rax
      X86_MOV_RBP(0x89, regs + op->b); // mov %rax local
      X86_PRE(); ASM(0x83); ASM(0xF0); ASM(0x04); // xor REF %rax
      X86_PRE(); ASM(0x89); ASM(0xC2); // mov %rax %rdx
    } else {
      fprintf(stderr, "** missing an upval to proto %p\n", (void *)proto);
    }
    X86_MOV_RBP(0x8B, op->a); // mov cl %rax
    X86_PRE(); ASM(0x89); ASM(0x50); // mov %rdx N(%rax)
      ASM(sizeof(struct PNClosure) + (sizeof(PN) * i));
  });
}

void potion_x86_finish(Potion *P, PNAsm *asmb) {
#ifdef JIT_ICACHE
#if __WORDSIZE != 64
  if (1) { // TODO: only scan if relative jumps are used
    long ici = 0; int *ipos, ival; 
    for (ici = 0; ici < asmb->capa; ici++) {
      if (asmb->start[ici] != 0x89) continue;
      if (asmb->start[ici+1] != 0x05) continue;
      if (asmb->start[ici+5] != 0 && asmb->start[ici+5] != 0xff) continue;
      ipos = (int *)(asmb->start+ici+2);
      ival = *ipos;
      asmb->start[ici] = 0xA3;   // mov %eax MEMOFF
      asmb->start[ici+5] = 0x90; // nop
      ipos = (int *)(asmb->start+ici+1);
      *ipos = fn+ici+6+ival;
    }
  }
#endif
#endif
}

MAKE_TARGET(x86);
