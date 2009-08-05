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
#include "khash.h"
#include "table.h"

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
#define X86_MOVQ(reg, x) \
        X86_PRE(); ASM(0xC7); /* movl */ \
        ASM(0x45); ASM(RBP(reg)); /* -A(%rbp) */ \
        ASMI((PN)(x))
#define X86_MATH(two, func, ops) ({ \
        int asmpos = 0; \
        X86_MOV_RBP(0x8B, op.a); /* mov -A(%rbp) %eax */ \
        if (two) { X86_PRE(); ASM(0x8B); ASM(0x55); ASM(RBP(op.b)); } /* mov -B(%rbp) %edx */ \
        ASM(0xF6); ASM(0xC0); ASM(0x01); /* test 0x1 %al */ \
        asmpos = (*asmp)->len; \
        ASM(0x74); ASM(0); /* je [a] */ \
        if (two) { ASM(0xF6); ASM(0xC2); ASM(0x01); /* test 0x1 %dl */ } \
        if (two) { ASM(0x74); ASM(0); /* je [a] */ } \
        ops; /* add, sub, ... */ \
        (*asmp)->ptr[asmpos + 1] = ((*asmp)->len - asmpos); \
        if (two) { (*asmp)->ptr[asmpos + 6] = ((*asmp)->len - asmpos) - 5; } \
        asmpos = (*asmp)->len; \
        ASM(0xEB); ASM(0); /*  jmp [b] */ \
        X86_ARGO(start - 3, 0); /* [a] */ \
        X86_ARGO(op.a, 1); \
        X86_ARGO(op.b, 2); \
        X86_PRE(); ASM(0xB8); ASMN(func); /* mov &func %rax */ \
        ASM(0xFF); ASM(0xD0); /* callq %rax */ \
        (*asmp)->ptr[asmpos + 1] = ((*asmp)->len - asmpos) - 2; \
        X86_MOV_RBP(0x89, op.a); /* mov -B(%rbp) %eax */ \
})
#define X86_CMP(ops) \
        X86_PRE(); ASM(0x8B); ASM(0x55); ASM(RBP(op.a)); /* mov -A(%rbp) %edx */ \
        X86_MOV_RBP(0x8B, op.b); /* mov -B(%rbp) %eax */ \
        ASM(0x39); ASM(0xC2); /*  cmp %eax %edx */ \
        ASM(ops); ASM(0x9 + X86_PRE_T); /*  jle +10 */ \
        X86_MOVQ(op.a, PN_TRUE); /*  -A(%rbp) = TRUE */ \
        ASM(0xEB); ASM(0x7 + X86_PRE_T); /*  jmp +7 */ \
        X86_MOVQ(op.a, PN_FALSE) /*  -A(%rbp) = FALSE */
#define X86_ARGO(regn, argn) potion_x86_c_arg(P, asmp, 1, regn, argn)
#define X86_ARGI(regn, argn) potion_x86_c_arg(P, asmp, 0, regn, argn)
#define TAG_JMP(jpos) \
        ASM(0xE9); \
        if ((int)jpos >= (int)pos) { \
          jmps[*jmpc].from = asmp[0]->len; \
          ASMI(0); \
          jmps[*jmpc].to = jpos + 1; \
          *jmpc = *jmpc + 1; \
        } else if ((int)jpos < (int)pos) { \
          ASMI(offs[jpos + 1] - ((asmp[0]->len) + 4)); \
        } else { \
          ASMI(0); \
        }

#define X86_DEBUG() \
  X86_PRE(); ASM(0xB8); ASMN(potion_x86_debug); \
  ASM(0xFF); ASM(0xD0)

// TODO: finish jit backtraces using this
void potion_x86_debug() {
  Potion *P;
  int n = 0;
  _PN rax, *rbp, *sp;

#if POTION_X86 == POTION_JIT_TARGET
#if __WORDSIZE != 64
  __asm__ ("mov %%eax, %0;"
#else
  __asm__ ("mov %%rax, %0;"
#endif
           :"=r"(rax)
          );

  printf("RAX = %lx (%u)\n", rax, potion_type(rax));
#if __WORDSIZE != 64
  __asm__ ("mov %%ebp, %0;"
#else
  __asm__ ("mov %%rbp, %0;"
#endif
           :"=r"(sp)
          );
#endif

  P = (Potion *)sp[2];
  printf("Potion: %p (%p)\n", P, &P);

again:
  n = 0;
  rbp = (unsigned long *)*sp;
  if (rbp > sp - 2 && sp[2] == (PN)P) {
    printf("RBP = %lx (%lx), SP = %lx\n", (PN)rbp, *rbp, (PN)sp);
    while (sp < rbp) {
      printf("STACK[%d] = %lx\n", n++, *sp);
      sp++;
    }
    goto again;
  }
}

// mimick c calling convention
static void potion_x86_c_arg(Potion *P, PNAsm * volatile *asmp, int out, int regn, int argn) {
#if __WORDSIZE != 64
  if (argn == 0) {
    // OPT: the first argument is always (Potion *)
    if (!out) {
      ASM(0x8b); ASM(0x55); ASM(2 * sizeof(PN));
      ASM(0x89); ASM(0x14); ASM(0x24);
    }
  } else {
    if (out) {
      ASM(0x8b); ASM(0x55); ASM(RBP(regn));
    }
    if (!out) argn += 2;
    if (out) {
      ASM(0x89); ASM(0x54); ASM(0x24); ASM(argn * sizeof(PN));
    } else {
      ASM(0x8b); ASM(0x55); ASM(argn * sizeof(PN));
    }
    if (!out) {
      ASM(0x89); ASM(0x55); ASM(RBP(regn));
    }
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
      ASM(0x4c); ASM(out ? 0x8b : 0x89); ASM(0x45); ASM(RBP(regn));
    break;
    case 5:
      ASM(0x4c); ASM(out ? 0x8b : 0x89); ASM(0x4d); ASM(RBP(regn));
    break;
    default:
      if (out) {
        X86_PRE(); ASM(0x8B); ASM(0x5d); ASM(RBP(regn)); // mov %rbp(A) %rbx
        if (argn == 6) {
          X86_PRE(); ASM(0x89); ASM(0x1c); ASM(0x24);    // mov %rbx (%rsp)
        } else {
          X86_PRE(); ASM(0x89); ASM(0x5c); ASM(0x24); ASM((argn - 6) * sizeof(PN)); // mov %rbx N(%rsp)
        }
      } else {
        X86_PRE(); ASM(0x8b); ASM(0x5d); ASM((argn - 4) * sizeof(PN));
        X86_PRE(); ASM(0x89); ASM(0x5d); ASM(RBP(regn)); // mov %rbp(A) %rbx
      }
    break;
  }
#endif
}

void potion_x86_setup(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp) {
  ASM(0x55); // push %rbp
  X86_PRE(); ASM(0x89); ASM(0xE5); // mov %rsp,%rbp
}

void potion_x86_stack(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, long need) {
  /* maintain 16-byte stack alignment.  OS X in particular requires it, because
   * it expects to be able to use movdqa on things on the stack.
   * we factor in the offset from our saved ebp and return address, so that
   * adds 8 for x86 and 0 (mod 16) for x86_64.  */
  int rsp = X86C(16,0)+((need-X86C(8,0)+15)&~(15));
  if (rsp >= 0x80) {
    X86_PRE(); ASM(0x81); ASM(0xEC); ASMI(rsp); /* sub rsp, %esp */
  } else {
    X86_PRE(); ASM(0x83); ASM(0xEC); ASM(rsp); /* sub rsp, %esp */
  }
}

void potion_x86_registers(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, long start) {
  PN_HAS_UPVALS(up);
  // (Potion *, self) in the first argument slot, self in the first register 
  X86_ARGI(start - 3, 0);
  X86_ARGI(start - 2, 1);
  X86_ARGI(start - 1, 2);
  X86_ARGI(0, 2);
  // empty locals, since use of setlocal requires something there
  if (up) {
    int argx = 0, regs = PN_INT(f->stack);
    for (argx = 0; argx < PN_TUPLE_LEN(f->locals); argx++) {
      X86_MOVQ(regs + argx, PN_NIL);
    }
  }
}

void potion_x86_local(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, long reg, long arg) {
  X86_ARGI(reg, 3 + arg);
}

void potion_x86_upvals(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, long lregs, long start, int upc) {
  int upi;
  for (upi = 0; upi < upc; upi++) {
    X86_MOV_RBP(0x8B, start - 2);
    X86_PRE(); ASM(0x8B); ASM(0x40);
      ASM(sizeof(struct PNClosure) + ((upi + 1) * sizeof(PN))); // 0x30(%rax)
    X86_MOV_RBP(0x89, lregs + upi);
  }
}

void potion_x86_jmpedit(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, unsigned char *asmj, int dist) {
  *((int *)asmj) = dist;
}

void potion_x86_move(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_MOV_RBP(0x8B, op.b);
  X86_MOV_RBP(0x89, op.a);
}

void potion_x86_loadpn(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_MOVQ(op.a, op.b);
}

PN potion_f_values(Potion *P, PN cl) {
  return potion_fwd(PN_PROTO(PN_CLOSURE(cl)->data[0])->values);
}

void potion_x86_loadk(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0);
  X86_ARGO(start - 2, 1);
  X86_PRE(); ASM(0xB8); ASMN(potion_f_values); // mov &potion_f_values %rax
  ASM(0xFF); ASM(0xD0); // callq %rax
  X86_PRE(); ASM(0x05); ASMI(sizeof(struct PNTuple) + (op.b * sizeof(PN))); // add N,%rax
  X86_PRE(); ASM(0x8B); ASM(0); // mov (%rax) %rax
  X86_MOV_RBP(0x89, op.a);
}

void potion_x86_self(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  // TODO: optimize so that if this is followed by a BIND or MESSAGE, it'll just
  // use the self register directly.
  X86_MOV_RBP(0x8B, start - 1);
  X86_MOV_RBP(0x89, op.a);
}

void potion_x86_getlocal(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long regs) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PN_HAS_UPVALS(up);
  X86_MOV_RBP(0x8B, regs + op.b); // mov %rsp(B) %rax
  if (up) {
    ASM(0xF6); ASM(0xC0); ASM(0x01); // test 0x1 %al
    ASM(0x75); ASM(X86C(19, 20)); // jne [a]
    ASM(0xF7); ASM(0xC0); ASMI(PN_REF_MASK); // test REFMASK %eax
    ASM(0x74); ASM(X86C(11, 12)); // je [a]
    ASM(0x81); ASM(0x38); ASMI(PN_TWEAK); // cmpq WEAK (%rax)
    ASM(0x75); ASM(X86C(3, 4)); // jne [a]
    X86_PRE(); ASM(0x8B); ASM(0x40); ASM(sizeof(struct PNObject)); // mov N(%rax) %rax
  }
  X86_MOV_RBP(0x89, op.a); // [b] mov %rax %rsp(A)
}

void potion_x86_setlocal(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long regs) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PN_HAS_UPVALS(up);
  X86_PRE(); ASM(0x8B); ASM(0x55); ASM(RBP(op.a)); // mov %rsp(A) %rdx
  if (up) {
    X86_MOV_RBP(0x8B, regs + op.b); // mov %rsp(B) %rax
    ASM(0xF6); ASM(0xC0); ASM(0x01); // test 0x1 %al
    ASM(0x75); ASM(X86C(19, 20)); // jne [a]
    ASM(0xF7); ASM(0xC0); ASMI(PN_REF_MASK); // test REFMASK %eax
    ASM(0x74); ASM(X86C(11, 12)); // je [a]
    ASM(0x81); ASM(0x38); ASMI(PN_TWEAK); // cmpq WEAK (%rax)
    ASM(0x75); ASM(X86C(3, 4)); // jne [a]
    X86_PRE(); ASM(0x89); ASM(0x50); ASM(sizeof(struct PNObject)); // mov N(%rax) %rax
  }
  X86_PRE(); ASM(0x89); ASM(0x55); ASM(RBP(regs + op.b)); // mov %rdx %rsp(B)
}

void potion_x86_getupval(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long lregs) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_MOV_RBP(0x8B, lregs + op.b);
  X86_PRE(); ASM(0x8B); ASM(0x40); ASM(sizeof(struct PNObject));
  X86_MOV_RBP(0x89, op.a);
}

// TODO: place the upval in the write barrier (or have stack scanning handle weak refs)
void potion_x86_setupval(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long lregs) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_PRE(); ASM(0x8B); ASM(0x55); ASM(RBP(op.a)); /*  mov -A(%rbp) %edx */
  X86_MOV_RBP(0x8B, lregs + op.b); // mov %rsp(B) %rax
  X86_PRE(); ASM(0x89); ASM(0x50); ASM(sizeof(struct PNObject)); // mov %rdx %rax.data
}

void potion_x86_newtuple(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0);
  X86_PRE(); ASM(0xB8); ASMN(potion_tuple_empty); // mov &potion_tuple_empty %rax
  ASM(0xFF); ASM(0xD0); // callq %rax
  X86_MOV_RBP(0x89, op.a); // mov %rax local
}

void potion_x86_settuple(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0);
  X86_ARGO(op.a, 1);
  X86_ARGO(op.b, 2);
  X86_PRE(); ASM(0xB8); ASMN(potion_tuple_push); // mov &potion_tuple_push %rax
  ASM(0xFF); ASM(0xD0); // callq %rax
  X86_MOV_RBP(0x89, op.a); // mov %rax local
}

void potion_x86_settable(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0);
  X86_ARGO(op.a, 1);
  X86_ARGO(op.a + 1, 2);
  X86_ARGO(op.b, 3);
  X86_PRE(); ASM(0xB8); ASMN(potion_table_set); // mov &potion_table_set %rax
  ASM(0xFF); ASM(0xD0); // callq %rax
  X86_MOV_RBP(0x89, op.a); // mov %rax local
}

void potion_x86_newlick(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  int nnil = 0;
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0);
  X86_ARGO(op.a, 1);
  if (op.b > op.a) {
    X86_ARGO(op.a + 1, 2);
  } else {
    nnil = 1;
    X86_MOVQ(op.a, PN_NIL);
    X86_ARGO(op.a, 2);
  }
  if (op.b > op.a + 1) {
    X86_ARGO(op.b, 3);
  } else {
    if (!nnil) { X86_MOVQ(op.a, PN_NIL); }
    X86_ARGO(op.a, 3);
  }
  X86_PRE(); ASM(0xB8); ASMN(potion_lick); // mov &potion_lick %rax
  ASM(0xFF); ASM(0xD0); // callq %rax
  X86_MOV_RBP(0x89, op.a); // mov %rax local
}

void potion_x86_getpath(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0);
  X86_ARGO(op.a, 2);
  X86_ARGO(op.b, 3);
  X86_PRE(); ASM(0xB8); ASMN(potion_obj_get); // mov &potion_obj_get %rax
  ASM(0xFF); ASM(0xD0); // callq %rax
  X86_MOV_RBP(0x89, op.a); // mov %rax local
}

void potion_x86_setpath(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0);
  X86_ARGO(op.a, 2);
  X86_ARGO(op.a + 1, 3);
  X86_ARGO(op.b, 4);
  X86_PRE(); ASM(0xB8); ASMN(potion_obj_set); // mov &potion_obj_set %rax
  ASM(0xFF); ASM(0xD0); // callq %rax
}

void potion_x86_add(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_MATH(1, potion_obj_add, {
    X86_PRE(); ASM(0x8D); ASM(0x44); ASM(0x10); ASM(0xFF); // lea -1(%eax,%edx,1),%eax
  });
}

void potion_x86_sub(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_MATH(1, potion_obj_sub, {
    X86_PRE(); ASM(0x29); ASM(0xD0); // sub %edx %eax
    X86_PRE(); ASM(0xFF); ASM(0xC0); // inc %eax
  });
}

void potion_x86_mult(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_MATH(1, potion_obj_mult, {
    X86_PRE(); ASM(0xD1); ASM(0xFA); // sar %rdx
    X86_PRE(); ASM(0xFF); ASM(0xC8); // dec %rax
    X86_PRE(); ASM(0x0F); ASM(0xAF); ASM(0xC2); // imul %rdx %rax
    X86_PRE(); ASM(0xFF); ASM(0xC0); // inc %rax
  });
}

void potion_x86_div(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_MATH(1, potion_obj_div, {
    ASM(0xD1); ASM(0xF8); // sar %rax
    ASM(0xD1); ASM(0xFA); // sar %edx
    ASM(0x89); ASM(0xD1); // mov %edx %ecx
    ASM(0x89); ASM(0xC2); // mov %eax %edx
    ASM(0xC1); ASM(0xFA); ASM(0x1F); // sar 0x1f %edx
    ASM(0xF7); ASM(0xF9); // idiv %ecx
    ASM(0x8D); ASM(0x44); ASM(0x00); ASM(0x01); // lea 0x1(%eax,%eax,1),%eax
  });
}

void potion_x86_rem(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_MATH(1, potion_obj_rem, {
    ASM(0xD1); ASM(0xF8); // sar %rax
    ASM(0xD1); ASM(0xFA); // sar %edx
    ASM(0x89); ASM(0xD1); // mov %edx %ecx
    ASM(0x89); ASM(0xC2); // mov %eax %edx
    ASM(0xC1); ASM(0xFA); ASM(0x1F); // sar 0x1f %edx
    ASM(0xF7); ASM(0xF9); // idiv %ecx
    ASM(0x8D); ASM(0x44); ASM(0x12); ASM(0x01); // lea 0x1(%edx,%edx,1),%eax
  });
}

void potion_x86_pow(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0);
  X86_ARGO(op.a, 2);
  X86_ARGO(op.b, 3);
  X86_PRE(); ASM(0xB8); ASMN(potion_pow); // mov &potion_pow %rax
  ASM(0xFF); ASM(0xD0); // callq %rax
  X86_MOV_RBP(0x89, op.a); // mov %rax local
}

void potion_x86_neq(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_CMP(0x74); // je
}

void potion_x86_eq(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_CMP(0x75); // jne
}

void potion_x86_lt(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_CMP(0x7D); // jge
}

void potion_x86_lte(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_CMP(0x7F); // jg
}

void potion_x86_gt(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_CMP(0x7E); // jle
}

void potion_x86_gte(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_CMP(0x7C); // jl
}

void potion_x86_bitn(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_MATH(0, potion_obj_bitn, {
    X86_PRE(); ASM(0xF7); ASM(0xD0); // not %eax
    X86_PRE(); ASM(0xFF); ASM(0xC0); // inc %rax
  });
}

void potion_x86_bitl(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_MATH(1, potion_obj_bitl, {
    ASM(0xD1); ASM(0xF8); // sar %eax
    ASM(0xD1); ASM(0xFA); // sar %edx
    ASM(0x89); ASM(0xD1); // mov %edx %ecx
    ASM(0x89); ASM(0xC2); // mov %eax %edx
    ASM(0xD3); ASM(0xE0); // sar %cl %eax
    ASM(0x8D); ASM(0x44); ASM(0x00); ASM(0x01); // lea 0x1(%eax,%eax,1),%eax
  });
}

void potion_x86_bitr(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_MATH(1, potion_obj_bitr, {
    ASM(0xD1); ASM(0xF8); // sar %rax
    ASM(0xD1); ASM(0xFA); // sar %edx
    ASM(0x89); ASM(0xD1); // mov %edx %ecx
    ASM(0x89); ASM(0xC2); // mov %eax %edx
    ASM(0xD3); ASM(0xF8); // sar %cl %eax
    ASM(0x8D); ASM(0x44); ASM(0x00); ASM(0x01); // lea 0x1(%eax,%eax,1),%eax
  });
}

void potion_x86_def(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0);
  X86_ARGO(op.a, 2);
  X86_ARGO(op.a + 1, 3);
  X86_ARGO(op.b, 4);
  X86_PRE(); ASM(0xB8); ASMN(potion_def_method); // mov &potion_def_method %rax
  ASM(0xFF); ASM(0xD0); // callq %rax
  X86_MOV_RBP(0x89, op.a); // mov %rax local
}

void potion_x86_bind(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0); // (0, 3)
  X86_ARGO(op.b, 1); // (7, 3)
  X86_ARGO(op.a, 2); // (7, 3)
  X86_PRE(); ASM(0xB8); ASMN(potion_bind); // mov &potion_bind %rax
  ASM(0xFF); ASM(0xD0); // callq %rax
  X86_MOV_RBP(0x89, op.a); // mov %rax local
}

void potion_x86_message(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0); // (0, 3)
  X86_ARGO(op.b, 1); // (7, 3)
  X86_ARGO(op.a, 2); // (7, 3)
  X86_PRE(); ASM(0xB8); ASMN(potion_message); // mov &potion_message %rax
  ASM(0xFF); ASM(0xD0); // callq %rax
  X86_MOV_RBP(0x89, op.a); // mov %rax local
}

void potion_x86_jmp(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, PNJumps *jmps, size_t *offs, int *jmpc) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  TAG_JMP(pos + op.a);
}

void potion_x86_test_asm(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, int test) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_MOV_RBP(0x8B, op.a); // mov -A(%rbp) %rax
  X86_PRE(); ASM(0x83); ASM(0xF8); ASM(PN_FALSE); // cmp FALSE %rax
  ASM(0x74); ASM(X86C(13, 15)); // je +10
  X86_PRE(); ASM(0x85); ASM(0xC0); // test %rax %rax
  ASM(0x74); ASM(X86C(9, 10)); // je +5
  X86_MOVQ(op.a, test ? PN_FALSE : PN_TRUE); // -A(%rbp) = TRUE
  ASM(0xEB); ASM(X86C(7, 8)); // jmp +7
  X86_MOVQ(op.a, test ? PN_TRUE : PN_FALSE); // -A(%rbp) = FALSE
}

void potion_x86_test(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  potion_x86_test_asm(P, f, asmp, pos, 0);
}

void potion_x86_not(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  potion_x86_test_asm(P, f, asmp, pos, 1);
}

void potion_x86_cmp(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  potion_x86_test_asm(P, f, asmp, pos, 0);
}

void potion_x86_testjmp(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, PNJumps *jmps, size_t *offs, int *jmpc) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_MOV_RBP(0x8B, op.a); // mov -A(%rbp) %rax
  X86_PRE(); ASM(0x83); ASM(0xF8); ASM(PN_FALSE); // cmp FALSE %rax
  ASM(0x74); ASM(X86C(9, 10)); // je +10
  X86_PRE(); ASM(0x85); ASM(0xC0); // test %rax %rax
  ASM(0x74); ASM(5);
  TAG_JMP(pos + op.b);
}

void potion_x86_notjmp(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, PNJumps *jmps, size_t *offs, int *jmpc) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_MOV_RBP(0x8B, op.a); // mov -A(%rbp) %rax
  X86_PRE(); ASM(0x83); ASM(0xF8); ASM(PN_FALSE); // cmp FALSE %rax
  ASM(0x74); ASM(X86C(4, 5)); // je +5
  X86_PRE(); ASM(0x85); ASM(0xC0); // test %rax %rax
  ASM(0x75); ASM(5);
  TAG_JMP(pos + op.b);
}

void potion_x86_named(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0);
  X86_ARGO(op.a, 1);
  X86_ARGO(op.b - 1, 2);
  X86_PRE(); ASM(0xB8); ASMN(potion_sig_find); // mov &potion_sig_find %rax
  ASM(0xFF); ASM(0xD0); // callq %eax
  ASM(0x85); ASM(0xC0); // test %eax %eax
  ASM(0x78); ASM(X86C(9, 12)); // js +12
  X86_PRE(); ASM(0xF7); ASM(0xD8); // neg %rax
  X86_PRE(); ASM(0x8B); ASM(0x55); ASM(RBP(op.b)); // mov -B(%rbp) %rdx
#if __WORDSIZE != 64
  ASM(0x89); ASM(0x54); ASM(0x85); ASM(RBP(op.a + 2)); // mov %edx -A(%ebp,%eax,4)
#else
  X86_PRE(); ASM(0x89); ASM(0x54); ASM(0xC5); ASM(RBP(op.a + 2)); // mov %rdx -A(%rbp,%rax,8)
#endif
}

// TODO: check for bytecode nodes and jit them as well?
void potion_x86_call(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  int argc = op.b - op.a;

  // check type of the closure
  X86_PRE(); ASM(0x8B); ASM(0x45); ASM(RBP(op.a)); // mov %rbp(A) %rax
  ASM(0xF6); ASM(0xC0); ASM(0x01); // test 0x1 %al
  ASM(0x75); ASM(X86C(56, 68)); // jne [a]
  ASM(0xF7); ASM(0xC0); ASMI(PN_REF_MASK); // test REFMASK %eax
  ASM(0x74); ASM(X86C(48, 60)); // je [a]
  X86_PRE(); ASM(0x83); ASM(0xE0); ASM(0xF8); // and ~PRIMITIVE %rax

  // if a class, pull out the constructor
  ASM(0x81); ASM(0x38); ASMI(PN_TVTABLE); // cmpq VTABLE (%eax)
  ASM(0x75); ASM(X86C(26, 36)); // jne [c]
  X86_ARGO(start - 3, 0);
  X86_ARGO(op.a, 2);
  X86_PRE(); ASM(0xB8); ASMN(potion_object_new); // mov &potion_object_new %rax
  ASM(0xFF); ASM(0xD0); // callq %rax
  X86_MOV_RBP(0x89, op.a + 1); // mov %rax local
  X86_PRE(); ASM(0x8B); ASM(0x45); ASM(RBP(op.a)); // mov %rbp(A) %rax
  X86_PRE(); ASM(0x8B); ASM(0x40);
    ASM((char *)&((struct PNVtable *)P->lobby)->ctor - (char *)P->lobby); // mov N(%rax) %rax
  X86_PRE(); ASM(0x89); ASM(0x45); ASM(RBP(op.a)); // mov %rax %rbp(A)

  // check type of the closure
  ASM(0x81); ASM(0x38); ASMI(PN_TCLOSURE); // cmpq CLOSURE (%eax)
  ASM(0x74); ASM(X86C(22, 30)); // je [a]

  // if not a closure, get the type's closure
  X86_MOV_RBP(0x8B, op.a);
  X86_MOV_RBP(0x89, op.a + 1);
  X86_ARGO(start - 3, 0);
  X86_ARGO(op.a, 1);
  X86_PRE(); ASM(0xB8); ASMN(potion_obj_get_call); // mov &potion_obj_get_call %rax
  ASM(0xFF); ASM(0xD0); // [b] callq *%rax
  ASM(0xEB); ASM(X86C(3, 4)); // jmp [b]

  // get the closure's function
  X86_PRE(); ASM(0x8B); ASM(0x45); ASM(RBP(op.a)); // mov %rbp(A) %rax
  X86_PRE(); ASM(0x8B); ASM(0x40); ASM(sizeof(struct PNObject)); // mov N(%rax) %rax

  // (Potion *, CL) as the first argument
  X86_ARGO(start - 3, 0);
  X86_ARGO(op.a, 1);
  while (--argc >= 0) X86_ARGO(op.a + argc + 1, argc + 2);
  ASM(0xFF); ASM(0xD0); // [b] callq *%rax
  X86_PRE(); ASM(0x89); ASM(0x45); ASM(RBP(op.a)); /* mov %rbp(A) %rax */
}

void potion_x86_callset(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0);
  X86_ARGO(op.b, 1);
  X86_PRE(); ASM(0xB8); ASMN(potion_obj_get_callset); // mov &potion_obj_get %rax
  ASM(0xFF); ASM(0xD0); // callq %rax
  X86_MOV_RBP(0x89, op.a); // mov %rax local
}

void potion_x86_return(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  X86_MOV_RBP(0x8B, 0); // mov -0(%rbp) %eax
  ASM(0xC9); ASM(0xC3); // leave; ret
}

PN potion_f_protos(Potion *P, PN cl, PN i) {
  PN p = PN_PROTO(PN_CLOSURE(cl)->data[0])->protos;
  PN proto = PN_TUPLE_AT(p, i);
  vPN(Closure) c = (struct PNClosure *)potion_closure_new(P, NULL,
    PN_PROTO(proto)->sig, PN_TUPLE_LEN(PN_PROTO(proto)->upvals) + 1);
  c->method = PN_PROTO(proto)->jit;
  c->data[0] = proto;
  return (PN)c;
}

void potion_x86_method(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE *pos, long lregs, long start, long regs) {
  PN_OP op = PN_OP_AT(f->asmb, *pos);
  PN proto = PN_TUPLE_AT(f->protos, op.b);
  X86_ARGO(start - 3, 0);
  X86_ARGO(start - 2, 1);
  X86_MOVQ(op.a, op.b);
  X86_ARGO(op.a, 2);
  X86_PRE(); ASM(0xB8); ASMN(potion_f_protos); // mov &potion_f_values %rax
  ASM(0xFF); ASM(0xD0); // callq %rax
  X86_MOV_RBP(0x89, op.a);
  PN_TUPLE_COUNT(PN_PROTO(proto)->upvals, i, {
    (*pos)++;
    PN_OP opp = PN_OP_AT(f->asmb, *pos);
    if (opp.code == OP_GETUPVAL) {
      X86_PRE(); ASM(0x8B); ASM(0x55); ASM(RBP(lregs + opp.b)); // mov upval %rdx
    } else if (opp.code == OP_GETLOCAL) {
      X86_ARGO(start - 3, 0);
      X86_ARGO(regs + opp.b, 1);
      X86_PRE(); ASM(0xB8); ASMN(potion_ref); // mov &potion_ref %rax
      ASM(0xFF); ASM(0xD0); // callq %rax
      X86_PRE(); ASM(0x89); ASM(0xC2); // mov %rax %rdx
      X86_MOV_RBP(0x89, regs + opp.b); // mov %rax local
    } else {
      fprintf(stderr, "** missing an upval to proto %p\n", (void *)proto);
    }
    X86_MOV_RBP(0x8B, opp.a); // mov cl %rax
    X86_PRE(); ASM(0x89); ASM(0x50); // mov %rdx N(%rax)
      ASM(sizeof(struct PNClosure) + (sizeof(PN) * (i + 1)));
  });
}

void potion_x86_class(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0);
  X86_ARGO(op.b, 1);
  X86_ARGO(op.a, 2);
  X86_PRE(); ASM(0xB8); ASMN(potion_vm_class); // mov &potion_vm_class %rax
  ASM(0xFF); ASM(0xD0); // callq %rax
  X86_MOV_RBP(0x89, op.a); // mov %rax local
}

void potion_x86_finish(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp) {
}

void potion_x86_mcache(Potion *P, vPN(Vtable) vt, PNAsm * volatile *asmp) {
  unsigned k;
#if __WORDSIZE != 64
  ASM(0x55); // push %ebp
  ASM(0x89); ASM(0xE5); // mov %esp %ebp
  ASM(0x8B); ASM(0x55); ASM(0x08); // mov 0x8(%ebp) %edx
#endif
  for (k = kh_end(vt->methods); k > kh_begin(vt->methods); k--) {
    if (kh_exist(PN, vt->methods, k - 1)) {
      ASM(0x81); ASM(X86C(0xFA, 0xFF));
        ASMI(PN_UNIQ(kh_key(PN, vt->methods, k - 1))); // cmp NAME %edi
      ASM(0x75); ASM(X86C(7, 11)); // jne +11
      X86_PRE(); ASM(0xB8); ASMN(kh_val(PN, vt->methods, k - 1)); // mov CL %rax
#if __WORDSIZE != 64
      ASM(0x5D);
#endif
      ASM(0xC3); // retq
    }
  }
  ASM(0xB8); ASMI(0); // mov NIL %eax
#if __WORDSIZE != 64
  ASM(0x5D);
#endif
  ASM(0xC3); // retq
}

void potion_x86_ivars(Potion *P, PN ivars, PNAsm * volatile *asmp) {
#if __WORDSIZE != 64
  ASM(0x55); // push %ebp
  ASM(0x89); ASM(0xE5); // mov %esp %ebp
  ASM(0x8B); ASM(0x55); ASM(0x08); // mov 0x8(%ebp) %edx
#else
#endif
  PN_TUPLE_EACH(ivars, i, v, {
    ASM(0x81); ASM(X86C(0xFA, 0xFF));
      ASMI(PN_UNIQ(v)); // cmp UNIQ %edi
    ASM(0x75); ASM(X86C(7, 6)); // jne +7
    ASM(0xB8); ASMI(i); // mov i %rax
#if __WORDSIZE != 64
    ASM(0x5D);
#endif
    ASM(0xC3); // retq
  });
  X86_PRE(); ASM(0xB8); ASMN(-1); // mov -1 %rax
#if __WORDSIZE != 64
  ASM(0x5D);
#endif
  ASM(0xC3); // retq
}

MAKE_TARGET(x86);
