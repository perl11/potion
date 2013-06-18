/**\file vm-x86.c
the x86 and x86_64 jit.
\see core/vm.c and INTERNALS.md

(c) 2008 why the lucky stiff, the freelance professor
(c) 2013 perl11 org
*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "p2.h"
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
#if __WORDSIZE != 64
# define X86_MOVQ(reg, x) \
        ASM(0xC7); 			/* movl */ \
        ASM(0x45); ASM(RBP(reg)); 	/* -A(%rbp) */ \
        ASMI((PN)(x))
#else
# define X86_MOVQ(reg, x) \
        X86_PRE(); ASM(0xb8); ASMN((PN)(x)); 			/* movq x, %rax */ \
        X86_PRE(); ASM(0x89); 					/* movq */ \
        ASM(0x45); ASM(RBP(reg)); 				/* %rax, -A(%rbp) */
#endif
#define X86_MATH(two, func, ops) ({ \
        int asmpos = 0; \
        X86_MOV_RBP(0x8B, op.a); 					/* mov -A(%rbp) %eax */ \
        if (two) { X86_PRE(); ASM(0x8B); ASM(0x55); ASM(RBP(op.b)); } 	/* mov -B(%rbp) %edx */ \
        ASM(0xF6); ASM(0xC0); ASM(0x01); 				/* test 0x1 %al */ \
        asmpos = (*asmp)->len; \
        ASM(0x74); ASM(0); 						/* je [a] */ \
        if (two) { ASM(0xF6); ASM(0xC2); ASM(0x01); 			/* test 0x1 %dl */ } \
        if (two) { ASM(0x74); ASM(0); 					/* je [a] */ } \
        ops; /* add, sub, ... */ \
        (*asmp)->ptr[asmpos + 1] = ((*asmp)->len - asmpos); \
        if (two) { (*asmp)->ptr[asmpos + 6] = ((*asmp)->len - asmpos) - 5; } \
        asmpos = (*asmp)->len; \
        ASM(0xEB); ASM(0); 						/*  jmp [b] */ \
        X86_ARGO(start - 3, 0); 				   /* [a]: mov &P  0(%esp) */ \
        X86_ARGO(op.a, 1);  					        /* mov &CL 1(%esp) */ \
        X86_ARGO(op.b, 2);  					        /* mov B   2(%esp) */ \
        X86_PRE(); ASM(0xB8); ASMN(func); 				/* mov &func %rax */ \
        ASM(0xFF); ASM(0xD0); 						/* callq %rax */ \
        (*asmp)->ptr[asmpos + 1] = ((*asmp)->len - asmpos) - 2; \
        X86_MOV_RBP(0x89, op.a); 					/* mov -B(%rbp) %eax */ \
})
#define X86_CMP(ops) \
        X86_PRE(); ASM(0x8B); ASM(0x55); ASM(RBP(op.a));/* mov -A(%rbp) %edx */ \
        X86_MOV_RBP(0x8B, op.b); 			/* mov -B(%rbp) %eax */ \
        X86_PRE(); ASM(0x39); ASM(0xC2); 		/* cmp %rax %rdx */ \
        ASM(ops); ASM(X86C(9, 16)); 			/* jle +10 */ \
        X86_MOVQ(op.a, PN_TRUE); 			/* -A(%rbp) = TRUE */ \
        ASM(0xEB); ASM(X86C(7, 14)); 			/* jmp +7 */ \
        X86_MOVQ(op.a, PN_FALSE) 			/* -A(%rbp) = FALSE */
#define X86_ARGO(regn, argn) potion_x86_c_arg(P, asmp, 1, regn, argn)
#define X86_ARGO_IMM(regn, argn) potion_x86_c_arg(P, asmp, 2, regn, argn)
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

/** mimick c calling convention
  \see http://en.wikipedia.org/wiki/X86_calling_conventions
  \param asmp: jit buffer
  \param out: 1 out-param only, 2 out+immediate, 0 in-param also
  \param regn: value (usually indirect stack ptr, relative to ebp)
  \param argn: argument index (0-5 in regs on 64bit) */
static void potion_x86_c_arg(Potion *P, PNAsm * volatile *asmp, int out, int regn, int argn) {
#if __WORDSIZE != 64
    // IA-32 cdecl ABI, non-microsoft only. TODO: win32 stdcall for the w32api ffi
    if (argn == 0) {
      // OPT: the first argument is always (Potion *)
      if (!out) {
	ASM(0x8b); ASM(0x55); ASM(2 * sizeof(PN));
	ASM(0x89); ASM(0x14); ASM(0x24);
      }
    }
    else {
      if (out == 2) {
	ASM(0xc7); ASM(0x44); ASM(0x24); ASM(argn * sizeof(PN)); ASMI(regn); //mov $regn, argn(%esp)
      } else if (out) {
	ASM(0x8b); ASM(0x55); ASM(RBP(regn)); //mov -0x8(%ebp), %edx
      }
      if (!out) argn += 2;
      if (out == 1) {
	ASM(0x89); ASM(0x54); ASM(0x24); ASM(argn * sizeof(PN)); //mov %edx, argn(%esp)
      } else if (!out) {
	ASM(0x8b); ASM(0x55); ASM(argn * sizeof(PN));
      }
      if (!out) {
	ASM(0x89); ASM(0x55); ASM(RBP(regn));
      }
    }
#else
  // sysv amd64 only (rdi,rsi,rdx,rcx,r8,r9), windows msvc not supported (rcx,rdx,r8,r9)
  // xmm0-7 not yet
  switch (argn) {
  case 0: //rdi Potion *P
    if (out == 2) { // unused - mov $regn, %rdi
      X86_PRE(); ASM(0xc7); ASM(0xc7); ASMI(regn);
    } else { // mov -regn(%rbp), %rdi
      X86_PRE(); ASM(out ? 0x8b : 0x89); ASM(0x7d); ASM(RBP(regn));
    }
    break;
  case 1: //rsi PN cl
    if (out == 2) { // unused - mov $regn, %rsi
      X86_PRE(); ASM(0xc7); ASM(0xc6); ASMI(regn);
    } else { // mov -regn(%rbp), %rsi
      X86_PRE(); ASM(out ? 0x8b : 0x89); ASM(0x75); ASM(RBP(regn));
    }
    break;
  case 2: //rdx self
    if (out == 2) { // unused - mov $regn, %rdx
      X86_PRE(); ASM(0xc7); ASM(0xc2); ASMI(regn);
    } else { // mov -regn(%rbp), %rdx
      X86_PRE(); ASM(out ? 0x8b : 0x89); ASM(0x55); ASM(RBP(regn));
    }
    break;
  case 3: //rcx
    if (out == 2) { // 1st default arg - mov $regn, %rcx
      X86_PRE(); ASM(0xc7); ASM(0xc1); ASMI(regn);
    } else { // mov -regn(%rbp), %rcx
      X86_PRE(); ASM(out ? 0x8b : 0x89); ASM(0x4d); ASM(RBP(regn));
    }
    break;
  case 4: //r8
    if (out == 2) { // 2nd default arg - mov $regn, %r8
      ASM(0x49); ASM(0xc7); ASM(0xc0); ASMI(regn);
    } else { // mov -regn(%rbp), %r8
      ASM(0x4c); ASM(out ? 0x8b : 0x89); ASM(0x45); ASM(RBP(regn));
    }
    break;
  case 5: //r9
    if (out == 2) { // 3rd default arg - mov $regn, %r9
      ASM(0x49); ASM(0xc7); ASM(0xc1); ASMI(regn);
    } else { // mov -regn(%rbp), %r9
      ASM(0x4c); ASM(out ? 0x8b : 0x89); ASM(0x4d); ASM(RBP(regn));
    }
    break;
    default: // can only pass max 6 arg via regs, rest on stack
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
  ASM(0x55); 				// push %rbp
  X86_PRE(); ASM(0x89); ASM(0xE5); 	// mov %rsp,%rbp
}

void potion_x86_stack(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, long need) {
  /* maintain 16-byte stack alignment.  OS X in particular requires it, because
   * it expects to be able to use movdqa on things on the stack.
   * we factor in the offset from our saved ebp and return address, so that
   * adds 8 for x86 and 0 (mod 16) for x86_64.  */
  int rsp = X86C(16,0)+((need-X86C(8,0)+15)&~(15));
  if (rsp >= 0x80) {
    X86_PRE(); ASM(0x81); ASM(0xEC); ASMI(rsp); // sub rsp, %esp
  } else {
    X86_PRE(); ASM(0x83); ASM(0xEC); ASM(rsp);  // sub rsp, %esp
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
  X86_PRE(); ASM(0xB8); ASMN(potion_f_values);		// mov &potion_f_values %rax
  ASM(0xFF); ASM(0xD0); 				// callq %rax
  X86_PRE(); ASM(0x05); ASMI(sizeof(struct PNTuple)
			     + (op.b * sizeof(PN)));	// add N,%rax
  X86_PRE(); ASM(0x8B); ASM(0); 			// mov (%rax) %rax
  X86_MOV_RBP(0x89, op.a);
}

void potion_x86_self(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  // TODO: optimize so that if this is followed by a BIND or MSG, it'll just
  // use the self register directly.
  X86_MOV_RBP(0x8B, start - 1);
  X86_MOV_RBP(0x89, op.a);
}

void potion_x86_getlocal(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long regs) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PN_HAS_UPVALS(up);
  X86_MOV_RBP(0x8B, regs + op.b); 		// mov %rsp(B) %rax
  if (up) {
    ASM(0xF6); ASM(0xC0); ASM(0x01); 		// test 0x1 %al
    ASM(0x75); ASM(X86C(19, 20)); 		// jnz [b]
    ASM(0xF7); ASM(0xC0); ASMI(PN_REF_MASK); 	// test REFMASK %eax
    ASM(0x74); ASM(X86C(11, 12)); 		// jz [b]
    ASM(0x81); ASM(0x38); ASMI(PN_TWEAK); 	// cmpq WEAK (%rax)
    ASM(0x75); ASM(X86C(3, 4)); 		// jnz [a]
    X86_PRE(); ASM(0x8B); ASM(0x40);
               ASM(sizeof(struct PNObject)); 	// mov N(%rax) %rax
  }
  X86_MOV_RBP(0x89, op.a); 		   // [a]: mov %rax %rsp(A)
}

void potion_x86_setlocal(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long regs) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PN_HAS_UPVALS(up);
  X86_PRE(); ASM(0x8B); ASM(0x55); ASM(RBP(op.a)); 	// mov %rsp(A) %rdx
  if (up) {
    X86_MOV_RBP(0x8B, regs + op.b); 			// mov %rsp(B) %rax
    ASM(0xF6); ASM(0xC0); ASM(0x01); 			// test 0x1 %al
    ASM(0x75); ASM(X86C(19, 20)); 			// jnz [b]
    ASM(0xF7); ASM(0xC0); ASMI(PN_REF_MASK); 		// test REFMASK %eax
    ASM(0x74); ASM(X86C(11, 12)); 			// jz [b]
    ASM(0x81); ASM(0x38); ASMI(PN_TWEAK); 		// cmpq WEAK (%rax)
    ASM(0x75); ASM(X86C(3, 4)); 			// jnz [a]
    X86_PRE(); ASM(0x89); ASM(0x50);
               ASM(sizeof(struct PNObject)); 		// mov N(%rax) %rax
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
  X86_PRE(); ASM(0x8B); ASM(0x55); ASM(RBP(op.a)); 		// mov -A(%rbp) %edx
  X86_MOV_RBP(0x8B, lregs + op.b); 				// mov %rsp(B) %rax
  X86_PRE(); ASM(0x89); ASM(0x50); ASM(sizeof(struct PNObject));// mov %rdx %rax.data
}

void potion_x86_global(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0);
  X86_ARGO(op.a, 1);
  X86_ARGO(op.b, 2);
  X86_PRE(); ASM(0xB8); ASMN(potion_define_global);
  ASM(0xFF); ASM(0xD0);
  X86_MOV_RBP(0x8B, op.b); 				// mov -B(%rbp) %eax
  X86_PRE(); ASM(0x89); ASM(0x45); ASM(RBP(op.a)); 	// mov %eax -A(%rbp)
}

void potion_x86_newtuple(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0);
  X86_PRE(); ASM(0xB8); ASMN(potion_tuple_empty);// mov &potion_tuple_empty %rax
  ASM(0xFF); ASM(0xD0); 			 // callq %rax
  X86_MOV_RBP(0x89, op.a); 			 // mov %rax local
}

void potion_x86_settuple(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0);
  X86_ARGO(op.a, 1);
  X86_ARGO(op.b, 2);
  X86_PRE(); ASM(0xB8); ASMN(potion_tuple_push);// mov &potion_tuple_push %rax
  ASM(0xFF); ASM(0xD0); 			// callq %rax
  X86_MOV_RBP(0x89, op.a); 			// mov %rax local
}

void potion_x86_settable(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0);
  X86_ARGO(op.a, 1);
  X86_ARGO(op.a + 1, 2);
  X86_ARGO(op.b, 3);
  X86_PRE(); ASM(0xB8); ASMN(potion_table_set); // mov &potion_table_set %rax
  ASM(0xFF); ASM(0xD0); 			// callq %rax
  X86_MOV_RBP(0x89, op.a); 			// mov %rax local
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
  X86_PRE(); ASM(0xB8); ASMN(potion_lick); 	// mov &potion_lick %rax
  ASM(0xFF); ASM(0xD0); 			// callq %rax
  X86_MOV_RBP(0x89, op.a); 			// mov %rax local
}

void potion_x86_getpath(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0);
  X86_ARGO(op.a, 2);
  X86_ARGO(op.b, 3);
  X86_PRE(); ASM(0xB8); ASMN(potion_obj_get); 	// mov &potion_obj_get %rax
  ASM(0xFF); ASM(0xD0); 			// callq %rax
  X86_MOV_RBP(0x89, op.a); 			// mov %rax local
}

void potion_x86_setpath(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0);
  X86_ARGO(op.a, 2);
  X86_ARGO(op.a + 1, 3);
  X86_ARGO(op.b, 4);
  X86_PRE(); ASM(0xB8); ASMN(potion_obj_set); 	// mov &potion_obj_set %rax
  ASM(0xFF); ASM(0xD0); 			// callq %rax
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
    X86_PRE(); ASM(0xD1); ASM(0xFA); 		// sar %rdx
    X86_PRE(); ASM(0xFF); ASM(0xC8); 		// dec %rax
    X86_PRE(); ASM(0x0F); ASM(0xAF); ASM(0xC2); // imul %rdx %rax
    X86_PRE(); ASM(0xFF); ASM(0xC0); 		// inc %rax
  });
}

void potion_x86_div(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_MATH(1, potion_obj_div, {
    ASM(0xD1); ASM(0xF8); 			// sar %rax
    ASM(0xD1); ASM(0xFA); 			// sar %edx
    ASM(0x89); ASM(0xD1); 			// mov %edx %ecx
    ASM(0x89); ASM(0xC2); 			// mov %eax %edx
    ASM(0xC1); ASM(0xFA); ASM(0x1F); 		// sar 0x1f %edx
    ASM(0xF7); ASM(0xF9); 			// idiv %ecx
    ASM(0x8D); ASM(0x44); ASM(0x00); ASM(0x01); // lea 0x1(%eax,%eax,1),%eax
  });
}

void potion_x86_rem(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_MATH(1, potion_obj_rem, {
    ASM(0xD1); ASM(0xF8); 		// sar %rax
    ASM(0xD1); ASM(0xFA); 		// sar %edx
    ASM(0x89); ASM(0xD1); 		// mov %edx %ecx
    ASM(0x89); ASM(0xC2); 		// mov %eax %edx
    ASM(0xC1); ASM(0xFA); ASM(0x1F); 	// sar 0x1f %edx
    ASM(0xF7); ASM(0xF9); 		// idiv %ecx
    ASM(0x8D); ASM(0x44); ASM(0x12); ASM(0x01); // lea 0x1(%edx,%edx,1),%eax
  });
}

void potion_x86_pow(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0);
  X86_ARGO(op.a, 2);
  X86_ARGO(op.b, 3);
  X86_PRE(); ASM(0xB8); ASMN(potion_pow);// mov &potion_pow %rax
  ASM(0xFF); ASM(0xD0); 		 // callq %rax
  X86_MOV_RBP(0x89, op.a); 		 // mov %rax local
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
    ASM(0xD1); ASM(0xF8); 			// sar %eax
    ASM(0xD1); ASM(0xFA); 			// sar %edx
    ASM(0x89); ASM(0xD1); 			// mov %edx %ecx
    ASM(0x89); ASM(0xC2); 			// mov %eax %edx
    ASM(0xD3); ASM(0xE0); 			// sar %cl %eax
    ASM(0x8D); ASM(0x44); ASM(0x00); ASM(0x01); // lea 0x1(%eax,%eax,1),%eax
  });
}

void potion_x86_bitr(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_MATH(1, potion_obj_bitr, {
    ASM(0xD1); ASM(0xF8); 			// sar %rax
    ASM(0xD1); ASM(0xFA); 			// sar %edx
    ASM(0x89); ASM(0xD1); 			// mov %edx %ecx
    ASM(0x89); ASM(0xC2); 			// mov %eax %edx
    ASM(0xD3); ASM(0xF8); 			// sar %cl %eax
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
  X86_ARGO(start - 3, 0); 			// mov &P 0(%esp)           (0, 3)
  X86_ARGO(op.b, 1);				// mov B  1(%esp) - new env (7, 3)
  X86_ARGO(op.a, 2);				// mov A  2(%esp)           (7, 3)
  X86_PRE(); ASM(0xB8); ASMN(potion_bind); 	// mov &potion_bind %rax
  ASM(0xFF); ASM(0xD0); 			// callq %rax
  X86_MOV_RBP(0x89, op.a); 			// mov %rax local
}

void potion_x86_message(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0); 			// mov &P 0(%esp)           (0, 3)
  X86_ARGO(op.b, 1); 	  			// mov B  1(%esp) - new env (7, 3)
  X86_ARGO(op.a, 2); 	  			// mov A  2(%esp)           (7, 3)
  X86_PRE(); ASM(0xB8); ASMN(potion_message); 	// mov &potion_message %rax
  ASM(0xFF); ASM(0xD0); 			// callq %rax
  X86_MOV_RBP(0x89, op.a); 			// mov %rax local
}

void potion_x86_jmp(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, PNJumps *jmps, size_t *offs, int *jmpc) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  TAG_JMP(pos + op.a);
}

void potion_x86_test_asm(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, int test) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_MOV_RBP(0x8B, op.a); 				// mov -A(%rbp) %rax
#ifdef P2
  X86_PRE(); ASM(0x83); ASM(0xF8); ASM(PN_NUM(0)); 	// cmp 0 %rax
#else
  X86_PRE(); ASM(0x83); ASM(0xF8); ASM(PN_FALSE); 	// cmp FALSE %rax
#endif
  ASM(0x74); ASM(X86C(13, 21)); 			// je +10
  X86_PRE(); ASM(0x85); ASM(0xC0); 			// test %rax %rax
  ASM(0x74); ASM(X86C(9, 16)); 				// je +5
  X86_MOVQ(op.a, test ? PN_FALSE : PN_TRUE); 		// -A(%rbp) = TRUE
  ASM(0xEB); ASM(X86C(7, 14)); 				// jmp +7
  X86_MOVQ(op.a, test ? PN_TRUE : PN_FALSE); 		// -A(%rbp) = FALSE
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
  X86_MOV_RBP(0x8B, op.a); 				// mov -A(%rbp) %rax
#ifdef P2
  //TODO besides 0 and also check PN_STR("") and maybe FALSE for backcompat with potion also
  X86_PRE(); ASM(0x83); ASM(0xF8); ASM(PN_NUM(0)); 	// cmp 0 %rax
#else
  X86_PRE(); ASM(0x83); ASM(0xF8); ASM(PN_FALSE); 	// cmp FALSE %rax
#endif
  ASM(0x74); ASM(X86C(9, 10)); 				// jz +10
  X86_PRE(); ASM(0x85); ASM(0xC0); 			// test %rax %rax
  ASM(0x74); ASM(5);					// jz +5
  TAG_JMP(pos + op.b);
}

void potion_x86_notjmp(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, PNJumps *jmps, size_t *offs, int *jmpc) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  DBG_t("; notjmp %d => %d\n", op.a, op.b);
  X86_MOV_RBP(0x8B, op.a);				// mov -A(%rbp) %rax
#ifdef P2
  X86_PRE(); ASM(0x83); ASM(0xF8); ASM(PN_NUM(0)); 	// cmp 0 %rax
#else
  X86_PRE(); ASM(0x83); ASM(0xF8); ASM(PN_FALSE);	// cmp FALSE %rax
#endif
  ASM(0x74); ASM(X86C(4, 5));				// jz +5
  X86_PRE(); ASM(0x85); ASM(0xC0);			// test %rax %rax
  ASM(0x75); ASM(5);					// jnz +5
  TAG_JMP(pos + op.b);
}

void potion_x86_named(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  DBG_t("; named %d %d\n", op.a, op.b);
  X86_ARGO(start - 3, 0); 				//P
  X86_ARGO(op.a, 1);      				//cl
  X86_ARGO(op.b - 1, 2);  				//name
  X86_PRE(); ASM(0xB8); ASMN(potion_sig_find); 		// mov &potion_sig_find %rax
  ASM(0xFF); ASM(0xD0);					// callq %eax
  ASM(0x85); ASM(0xC0);					// test %eax %eax
  ASM(0x78); ASM(X86C(9, 12));				// js +12
  X86_PRE(); ASM(0xF7); ASM(0xD8);			// neg %rax
  X86_PRE(); ASM(0x8B); ASM(0x55); ASM(RBP(op.b));	// mov -B(%rbp) %rdx
#if __WORDSIZE != 64
  ASM(0x89); ASM(0x54); ASM(0x85); ASM(RBP(op.a + 2));	// mov %edx -A(%ebp,%eax,4)
#else
  X86_PRE(); ASM(0x89); ASM(0x54); ASM(0xC5); ASM(RBP(op.a + 2)); // mov %rdx -A(%rbp,%rax,8)
#endif
}

// TODO: check for bytecode nodes and jit them as well?
void potion_x86_call(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  int argc = op.b - op.a; // including self
  int i;

  // check type of the closure
  X86_PRE(); ASM(0x8B); ASM(0x45); ASM(RBP(op.a));	// mov %rbp(A) %rax
  ASM(0xF6); ASM(0xC0); ASM(0x01);			// test 0x1 %al
  ASM(0x75); ASM(X86C(56, 68)); 			// jne [a]
  ASM(0xF7); ASM(0xC0); ASMI(PN_REF_MASK);		// test REFMASK %eax
  ASM(0x74); ASM(X86C(48, 60));				// je [a]
  X86_PRE(); ASM(0x83); ASM(0xE0); ASM(0xF8);		// and ~PRIMITIVE %rax

  // if a class, pull out the constructor
  ASM(0x81); ASM(0x38); ASMI(PN_TVTABLE);		// cmpq VTABLE (%eax)
  ASM(0x75); ASM(X86C(26, 36));				// jnz [c]
  X86_ARGO(start - 3, 0);				// mov &P 0(%esp)
  X86_ARGO(op.a, 2);					// mov A 2(%esp)
  X86_PRE(); ASM(0xB8); ASMN(potion_object_new);	// mov &potion_object_new %rax
  ASM(0xFF); ASM(0xD0); 				// callq %rax

  X86_MOV_RBP(0x89, op.a + 1); 			   // [c]: mov %rax local
  X86_PRE(); ASM(0x8B); ASM(0x45); ASM(RBP(op.a));	// mov %rbp(A) %rax
  X86_PRE(); ASM(0x8B); ASM(0x40);
    ASM((char *)&((struct PNVtable *)P->lobby)->ctor
	- (char *)P->lobby); 				// mov N(%rax) %rax
  X86_PRE(); ASM(0x89); ASM(0x45); ASM(RBP(op.a));	// mov %rax %rbp(A)

  // check type of the closure
  ASM(0x81); ASM(0x38); ASMI(PN_TCLOSURE);		// cmpq CLOSURE (%eax)
  ASM(0x74); ASM(X86C(22, 30));				// jz [a]

  // if not a closure, get the type's closure
  X86_MOV_RBP(0x8B, op.a);
  X86_MOV_RBP(0x89, op.a + 1);
  X86_ARGO(start - 3, 0);				// mov &P 0(%esp)
  X86_ARGO(op.a, 1);					// mov A 1(%esp)
  X86_PRE(); ASM(0xB8); ASMN(potion_obj_get_call); 	// mov &potion_obj_get_call %rax
  ASM(0xFF); ASM(0xD0); 			   // [b]: callq *%rax
  ASM(0xEB); ASM(X86C(3, 4)); 				// jmp [b]

  //[a]: get the closure's function
  X86_PRE(); ASM(0x8B); ASM(0x45); ASM(RBP(op.a)); // [a]: mov %rbp(A) %rax
  //[b]: got the method, call it (first special slot from PNClosure)
  X86_PRE(); ASM(0x8B); ASM(0x40);
             ASM(sizeof(struct PNObject));	   // [b]: mov N(%rax) %rax

  // (Potion *, CL) as the first arguments
  X86_ARGO(start - 3, 0);
  X86_ARGO(op.a, 1);
  DBG_t("; call %ld[0] %d[1] ", start-3, op.a);
  for (i=2; i <= argc+1; i++) {
    DBG_t("%d[%d] ", op.a + i - 1, i);
    X86_ARGO(op.a + i - 1, i);				// mov regn, i(%esp)
  }
  // fill in defaults, arity from protos[0], not f
  if (!PN_IS_EMPTY(f->protos)) {
    vPN(Proto) c = (vPN(Proto)) PN_TUPLE_AT(f->protos, 0);
    int arity = c->arity;
    if (arity && (argc-1 < arity)) {
      for (i = argc+1; i <= arity+1; i++) { //2: [0,1],2,3
	PN sig = potion_sig_at(P, c->sig, i-2);
	if (sig && PN_TUPLE_LEN(sig) == 3) {
	  DBG_t(":=*%s[%d] ", AS_STR(PN_TUPLE_AT(sig, 2)), i+1);
	  X86_ARGO_IMM(PN_TUPLE_AT(sig, 2), i+1); 	// mov $value, i(%esp) - default
	} else if (sig) {
	  DBG_t("|0 ");                                 // mov 0, i(%esp) - optional
	  char type = (char)(PN_TUPLE_LEN(sig) > 1
			     ? PN_INT(PN_TUPLE_AT(sig,1)) : 0);
	  X86_ARGO_IMM(type ? potion_type_default(type) : 0, i+1);
	}}}}
  DBG_t("\n");
  ASM(0xFF); ASM(0xD0); 				// callq *%rax
  X86_PRE(); ASM(0x89); ASM(0x45); ASM(RBP(op.a)); 	// mov %rbp(A) %rax
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
  X86_PRE(); ASM(0xB8); ASMN(potion_f_protos);	// mov &potion_f_values %rax
  ASM(0xFF); ASM(0xD0);				// callq %rax
  X86_MOV_RBP(0x89, op.a);
  X86_MOVQ(start - 3, P);
  PN_TUPLE_COUNT(PN_PROTO(proto)->upvals, i, {
    (*pos)++;
    PN_OP opp = PN_OP_AT(f->asmb, *pos);
    if (opp.code == OP_GETUPVAL) {
      X86_PRE(); ASM(0x8B); ASM(0x55); ASM(RBP(lregs + opp.b)); // mov upval %rdx
    } else if (opp.code == OP_GETLOCAL) {
      X86_ARGO(start - 3, 0);
      X86_ARGO(regs + opp.b, 1);
      X86_PRE(); ASM(0xB8); ASMN(potion_ref); 	// mov &potion_ref %rax
      ASM(0xFF); ASM(0xD0);			// callq %rax
      X86_PRE(); ASM(0x89); ASM(0xC2);		// mov %rax %rdx
      X86_MOV_RBP(0x89, regs + opp.b);		// mov %rax local
    } else {
      fprintf(stderr, "** missing an upval to proto %p\n", (void *)proto);
    }
    X86_MOV_RBP(0x8B, opp.a);			// mov cl %rax
    X86_PRE(); ASM(0x89); ASM(0x50);		// mov %rdx N(%rax)
      ASM(sizeof(struct PNClosure) + (sizeof(PN) * (i + 1)));
  });
}

void potion_x86_class(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0);
  X86_ARGO(op.b, 1);
  X86_ARGO(op.a, 2);
  X86_PRE(); ASM(0xB8); ASMN(potion_vm_class);	// mov &potion_vm_class %rax
  ASM(0xFF); ASM(0xD0);				// callq %rax
  X86_MOV_RBP(0x89, op.a);			// mov %rax local
}

void potion_x86_finish(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp) {
}

void potion_x86_mcache(Potion *P, vPN(Vtable) vt, PNAsm * volatile *asmp) {
  unsigned k;
#if __WORDSIZE != 64
  ASM(0x55); 							// push %ebp
  ASM(0x89); ASM(0xE5);						// mov %esp %ebp
  ASM(0x8B); ASM(0x55); ASM(0x08);				// mov 0x8(%ebp) %edx
#endif
  for (k = kh_end(vt->methods); k > kh_begin(vt->methods); k--) {
    if (kh_exist(PN, vt->methods, k - 1)) {
      ASM(0x81); ASM(X86C(0xFA, 0xFF));
        ASMI(PN_UNIQ(kh_key(PN, vt->methods, k - 1)));		// cmp NAME %edi
      ASM(0x75); ASM(X86C(7, 11)); 				// jne +11
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
  ASM(0x55); 				// push %ebp
  ASM(0x89); ASM(0xE5); 		// mov %esp %ebp
  ASM(0x8B); ASM(0x55); ASM(0x08);	// mov 0x8(%ebp) %edx
#else
#endif
  PN_TUPLE_EACH(ivars, i, v, {
    ASM(0x81); ASM(X86C(0xFA, 0xFF));
      ASMI(PN_UNIQ(v));			// cmp UNIQ %edi
    ASM(0x75); ASM(X86C(7, 6));		// jne +7
    ASM(0xB8); ASMI(i);			// mov i %rax
#if __WORDSIZE != 64
    ASM(0x5D);
#endif
    ASM(0xC3);				// retq
  });
  X86_PRE(); ASM(0xB8); ASMN(-1);	// mov -1 %rax
#if __WORDSIZE != 64
  ASM(0x5D);
#endif
  ASM(0xC3);				// retq
}

MAKE_TARGET(x86);
