/**\file vm-x86.c
the x86 and x86_64 jit.
\see core/vm.c and doc/INTERNALS.md

(c) 2008 why the lucky stiff, the freelance professor
(c) 2013-2015 perl11 org */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "potion.h"
#include "internal.h"
#include "opcodes.h"
#include "asm.h"
#include "khash.h"
#include "table.h"

#define RBP(x)   (0x100 - ((x + 1) * sizeof(PN)))
#define RBPN(x)  (- (int)((x + 1) * sizeof(PN)))
#define RBPI(x)  (0x100 - ((x + 1) * sizeof(int)))

#if PN_SIZE_T != 8
#define X86_PRE_T 0
#define X86_PRE()
#define X86_POST()
#define X86C(op32, op64, n, reg) op32+((reg)>15?(n)*3:0)
#ifdef DEBUG
# define ASM_MOV_EBP(op, reg)				\
  if (reg > 31) {					\
    DBG_v("; reg %d > 31, op 0x%x\n", (int)reg, op);	\
    ASM((op)+0x40); ASMI(RBPN(reg)); }			\
  else { ASM(op); ASM(RBP(reg)); }
#else
# define ASM_MOV_EBP(op, reg) /* 2,5 */			\
  if (reg > 31) { ASM((op)+0x40); ASMI(RBPN(reg)); }	\
  else { ASM(op); ASM(RBP(reg)); }
#endif
#else
#define X86_PRE_T 1
#define X86_PRE()  ASM(0x48)
#define X86_POST() ASM(0x48); ASM(0x98)
#define X86C(op32, op64, n, reg) op64+((reg)>31?(n)*3:0)
# ifdef DEBUG
# define ASM_MOV_EBP(op, reg)				\
  if (reg > 15) {					\
    DBG_v("; reg %d > 15, op 0x%x\n", (int)reg, op);	\
    ASM((op)+0x40); ASMI(RBPN(reg)); }		\
  else { ASM(op); ASM(RBP(reg)); }
# else
# define ASM_MOV_EBP(op, reg) /* 2,5 */		        \
  if (reg > 15) { ASM((op)+0x40); ASMI(RBPN(reg)); }	\
  else { ASM(op); ASM(RBP(reg)); }
# endif
#endif

#define X86_MOV_RBP(reg, x) X86_PRE(); ASM(reg); ASM_MOV_EBP(0x45,x)
#if PN_SIZE_T != 8
# define X86_MOVQ(reg, x)                       /* size = 7,10 */ \
        ASM(0xC7); ASM_MOV_EBP(0x45,reg)	/* movl -A(%rbp) */ \
        ASMI((PN)(x))
#else
# define X86_MOVQ(reg, x)                                       /* size = 14,17 */ \
        X86_PRE(); ASM(0xb8); ASMN((PN)(x)); 			/* movq x, %rax */ \
        X86_PRE(); ASM(0x89); ASM_MOV_EBP(0x45,reg)	       	/* movq %rax, -A(%rbp) */
#endif
#define TAG_PREP(tag)    tag = (*asmp)->len + 1
#define TAG_LABEL(tag)   (*asmp)->ptr[tag] = ((*asmp)->len - tag - 1)
// jump back to tag
#define TAG_JMPB(insn, tag)                                   \
        if (tag > (*asmp)->len) {                             \
          potion_fatal("jmp fw");                             \
        } else if ((*asmp)->len - tag > 255)  { /* jx long */ \
          ASM(0x0f);                                          \
          ASM(insn + 0x10);                                   \
          ASMI(tag - (*asmp)->len - 2);                       \
        } else { /* jx short */                               \
          ASM(insn);                                          \
          ASM(tag - (*asmp)->len - 2);                        \
        }
#define TAG_PREP4(tag)   tag = (*asmp)->len
#define TAG_LABEL4(tag)  ({ int* ptr = (int*)((*asmp)->ptr + tag); \
                            *ptr = (*asmp)->len - tag - 4; })

// TODO refactor to use named TAGs
// TODO optimize into seperate int and dbl variants
// TODO check num type for the dbl case
// math binop for 2 numbers, int (inlined) or double (via call)
#define X86_MATH(two, dbl_func, ops) ({ \
        int dbl_a, dbl_a1, end_b;			\
        X86_MOV_RBP(0x8B, op.a); 					/* mov -A(%rbp) %eax */ \
        if (two) { X86_PRE(); ASM(0x8B); ASM_MOV_EBP(0x55,op.b) }	/* mov -B(%rbp) %edx */	\
        ASM(0xF6); ASM(0xC0); ASM(0x01); 				/* test 0x1 %al */ \
        TAG_PREP(dbl_a); ASM(0x74); ASM(0); 				/* je [a] */ \
        if (two) { ASM(0xF6); ASM(0xC2); ASM(0x01); }			/* test 0x1 %dl */ \
        if (two) { TAG_PREP(dbl_a1); ASM(0x74); ASM(0); 		/* je [a] */ } \
        ops; /* add, sub, ... */ \
        TAG_PREP(end_b); ASM(0xEB); ASM(0); 				/* jmp [b] */ \
        TAG_LABEL(dbl_a); if (two) { TAG_LABEL(dbl_a1); }               \
        X86_ARGO(start - 3, 0); 				   /* [a]: mov &P  0(%esp) */ \
        X86_ARGO(op.a, 1);  					        /* mov &CL 1(%esp) */ \
        X86_ARGO(op.b, 2);  					        /* mov B   2(%esp) */ \
        X86_PRE(); ASM(0xB8); ASMN(dbl_func); 				/* mov &dbl_func %rax */ \
        ASM(0xFF); ASM(0xD0); 						/* callq %rax */ \
        TAG_LABEL(end_b); \
        X86_MOV_RBP(0x89, op.a) 				   /* [b]: mov -B(%rbp) %eax */ \
	  })
// cmp 2 numbers, int or double. eq/neq/gt/ge/lt/le, both inlined (requires SSE)
// iop: jl, jg, jle, jge, je, jne for normal cmp comparisons
// xop: jb, jbe", jae, ja, ... for SSE ucomisd comparisons.
// TODO we also need to check jp for PF (parity) if one number is nan, to force false.
// TODO optimize into seperate int and dbl variants
// TODO probe cpu for sse2 at init, and fallback to math calls if not.
//      maybe create seperate so's with and without sse. and load the best at init
// TODO check num type for the dbl case
// TODO optimize j? true, set false, jmp true, set true
//   => movzbl %al,edx;lea 0x2(,%rdx,4),%rdx;mov %rdx,-A(%rbp)
#define X86_NUMCMP(iop, xop, xmms)                                         \
        int dbl_a, dbl_b, cmp_dbl, true_1, true_2, false_;			\
        X86_PRE(); ASM(0x8B); ASM_MOV_EBP(0x55,op.a)	/* mov -A(%rbp) %rdx */ \
        X86_MOV_RBP(0x8B, op.b); 			/* mov -B(%rbp) %rax */ \
        ASMS("\xA8\x01");		 		/* test $1, %al */ \
        TAG_PREP(dbl_b); ASM(0x74);ASM(0); 		/* je [dbl_b] */ \
        ASMS("\xF6\xC2\x01");				/* test $1, %dl */ \
        TAG_PREP(dbl_a); ASM(0x74);ASM(0);		/* je [dbl_a] */ \
	/* cmp both int */						\
        X86_PRE(); ASM(0x39); ASM(0xC2);                /* both_int: cmp %rax, %rdx */ \
        TAG_PREP(true_1); ASM(iop); ASM(0);		/* j? [true] */	\
        TAG_PREP(false_); ASM(0xEB); ASM(0);		/* jmp [false] */ \
                                                                        \
	/* convert 1 or 2 to dbl, only with sse2. all amd64 have sse2 */ \
	/* TODO: so if 32bit without sse2 call the math func instead */ \
        TAG_LABEL(dbl_a);                               /* #dbl_a: b=int + a=dbl */ \
	ASMS("\xf2\x0f\x10\x42");ASM(PN_SIZE_T);	/* movsd 8(%rdx), %xmm0 [a] */ \
        ASMS("\x66\x0f\xef\xc9");			/* pxor %xmm1, %xmm1 */ \
	X86_PRE(); ASMS("\xd1\xf8");	                /* sar %rax */ \
	ASM(0xF2);X86_PRE();ASMS("\x0f\x2a\xc8");       /* cvtsi2sd %rax, %xmm1 [b] */ \
        TAG_PREP(cmp_dbl); ASM(0xEB);ASM(0);		/* jmp [cmp_dbl] */ \
                                                                        \
        TAG_LABEL(dbl_b); 				/* #b dbl, a? */ \
	ASMS("\xf2\x0f\x10\x48");ASM(PN_SIZE_T);	/* movsd 8(%rax), %xmm1 [b] */ \
        ASMS("\xF6\xC2\x01");				/* test $1, %dl */ \
        ASM(0x74);ASM(X86C(12,14, 0,0));                /* je +cvt */ \
        ASMS("\x66\x0f\xef\xc0");			/* pxor %xmm0, %xmm0 */ \
	X86_PRE(); ASMS("\xd1\xfa");	                /* sar %rdx */ \
	ASM(0xF2);X86_PRE();ASMS("\x0f\x2a\xc2");	/* cvtsi2sd %rdx, %xmm0 [a] */ \
        ASM(0xEB); ASM(X86C(5,5, 0,0));			/* jmp [cmp_dbl] */ \
	ASMS("\xf2\x0f\x10\x42");ASM(PN_SIZE_T);	/* movsd 8(%rdx), %xmm0 [a] */ \
        /* cmp dbl */							\
	TAG_LABEL(cmp_dbl); ASMS(xmms);			/* ucomisd xmm0<=>xmm1; */ \
        TAG_PREP(true_2); ASM(xop); ASM(0);        	/* j? [true] */   \
        TAG_LABEL(false_); X86_MOVQ(op.a, PN_FALSE); 	/* false: -A(%rbp) = FALSE */ \
        ASM(0xEB); ASM(X86C(7,14, 1,op.a));		/* jmp [+true] */ \
        TAG_LABEL(true_1); TAG_LABEL(true_2); \
        X86_MOVQ(op.a, PN_TRUE);      			/* true: -A(%rbp) = TRUE */

// eq/neq: cmp 2 atoms. cmp the dbl value if both are double or the immediate words.
// XXX 64bit only!
//#define X86_CMP(iop, wop)
//void X86_CMP(Potion P, PNAsm * asmp, unsigned char iop, unsigned char wop);
void x86_cmp(Potion *P, PNAsm * volatile * asmp, PN_OP op, unsigned char iop, unsigned char wop) {
  int a_int1,a_int2, l50, true_1, /*ret_false,*/ le0, cont;             \
        X86_MOV_RBP(0x8B, op.a); 			/* mov -A(%rbp) %rax */ \
        ASMS("\xA8\x01");		 		/* testb $1, %al */ \
        TAG_PREP(a_int1); ASM(0x75);ASM(0); 		/* jne [a_int] */ \
        X86_MOV_RBP(0x8B, op.a); 			/* mov -a(%rbp) %rax */ \
        ASMS("\x48\xa9\xf8\xff\xff\xff");		/* testq $-8, %rax (32 incompat!) */ \
        TAG_PREP(l50); ASM(0x75);ASM(0); 		/* jne [l50] */ \
        TAG_LABEL(a_int1);TAG_PREP(a_int2); 		/* l30: */      \
        /* a is int, so b must also be int */                           \
        X86_PRE(); ASM(0x8B); ASM_MOV_EBP(0x55,op.a)	/* mov -A(%rbp) %rdx */ \
        X86_MOV_RBP(0x8B, op.b); 			/* mov -B(%rbp) %rax */ \
        X86_PRE(); ASM(0x39); ASM(0xC2);                /* cmpq %rax, %rdx */ \
/*3a*/  ASM(0x0f); ASM(wop); TAG_PREP4(true_1); ASMS("\x0\x0\x0\x0"); /* j? [true] wide e0 */ \
        /* false: */                                                      \
/*43*/  /*TAG_LABEL(ret_false);*/ X86_MOVQ(op.a, PN_FALSE); /* movl FALSE, -A(%rbp) */ \
        TAG_PREP(cont); ASM(0xE9); ASMI(0);		/* jmp [cont] eda */ \
                                                                        \
        TAG_LABEL(l50);                                                 \
        X86_MOV_RBP(0x8B, op.a); 			/* mov -A(%rbp) %rax */ \
        ASMS("\x83\x38\xfe");                           /* cmpl $-2, (%rax) (is_ptr?) */ \
        ASM(0x0f); ASM(0x84); TAG_PREP4(le0); ASMS("\x0\x0\x0\x0"); /* je [le0] wide */ \
                                                                        \
	ASMS("\x8b\x00"					/* movl	(%rax), %eax */ \
             "\x83\xe0\xfd"				/* andl $-0x3, %eax */ \
             "\x3d\x01\x00\x25\x00");      		/* cmpl	$0x250001, %eax */ \
/*68*/  TAG_JMPB(0x75, a_int2);			        /* jne [a_int] */ \
        X86_MOV_RBP(0x8B, op.b); 			/* mov -B(%rbp) %rax */ \
        ASM(0xa8); ASM(0x01);            		/* testb	$0x1, %al */ \
/*71*/  TAG_JMPB(0x75, a_int2);				/* jne [a_int] */ \
        X86_MOV_RBP(0x8B, op.b); 			/* mov -B(%rbp) %rax */ \
        ASMS("\x48\xa9\xf8\xff\xff\xff");  		/* testq	$-0x8, %al */   \
/*7e*/  TAG_JMPB(0x74, a_int2);				/* je [a_int] */ \
        X86_MOV_RBP(0x8B, op.b); 			/* mov -B(%rbp) %rax */ \
        ASMS("\x83\x38\xfe"  				/* cmpl	$-0x2, (%rax) */   \
/*88*/       "\x74\x5f"			                /* je [e9] */ \
/*8a*/       "\x8b\x00"  				/* movl	(%rax), %eax */ \
             "\x83\xe0\xfd"  				/* andl	$-0x3, %eax */   \
             "\x3d\x01\x00\x25\x00");  			/* cmpl	$0x25001, %eax */ \
/*94*/  TAG_JMPB(0x75, a_int2);				/* jne [a_int] */ \
        X86_MOV_RBP(0x8B, op.a); 			/* mov -a(%rbp) %rax */ \
        ASMS("\xa8\x01");  				/* testb $0x1, %al */ \
        X86_MOV_RBP(0x8B, op.a); 			/* mov -a(%rbp) %rax */ \
/*a2*/  ASMS("\x74\x52");  				/* je [f6] */ \
/*a4*/  ASMS("\x48\xd1\xf8"  				/* sarq %rax */ \
             "\x66\x0f\xef\xc9");                       /* pxor %xmm1, %xmm1 */ \
	ASM(0xF2);X86_PRE();ASMS("\x0f\x2a\xc8");	/* cvtsi2sd %rax, %xmm1 [a] */ \
        X86_MOV_RBP(0x8B, op.b); 			/* mov -B(%rbp) %rax */ \
        ASMS("\xa8\x01");  				/* testb $0x1, %al */ \
        X86_MOV_RBP(0x8B, op.b); 			/* mov -B(%rbp) %rax */ \
/*bc*/  ASMS("\x74\x31");  				/* je [ef] */ \
             \
/*be*/  ASMS("\x48\xd1\xf8"  				/* sarq %rax */ \
             "\x66\x0f\xef\xc0");                       /* pxor %xmm0, %xmm0 */ \
	ASM(0xF2);X86_PRE();ASMS("\x0f\x2a\xc0");	/* cvtsi2sd %rax, %xmm0 [a] */ \
/*ca*/	/*TAG_LABEL(cmp_dbl);*/                                 \
        ASMS("\x66\x0f\x2e\xc8"				/* ucomisd xmm0, xmm1; */ \
/*ce*/       "\x0f\x8a\x6f\xff\xff\xff"  		/* jp [ret_false] wide */ \
/*d4*/       "\x0f\x85\x69\xff\xff\xff");  		/* jne [ret_false] wide */ \
                                                                        \
        TAG_LABEL4(true_1);                                             \
        X86_MOVQ(op.a, PN_TRUE);      			/* true: -A(%rbp) = TRUE */ \
        TAG_LABEL4(le0); \
/*e0*/  ASMS("\x48\x8b\x40\x08"  			/* movq 0x8(%rax), %rax */ \
             "\xe9\x75\xff\xff\xff"			/* jmp [5e] w */ \
/*e9*/       "\x48\x8b\x40\x08"  			/* movq 0x8(%rax), %rax */ \
             "\xeb\x97"  				/* jmp [8a] */ \
/*ef*/       "\xf2\x0f\x10\x40\x08"  			/* movsd 0x8(%rax), %xmm0 */ \
             "\xeb\xcc"  				/* jmp [cmp_dbl] */ \
/*f6*/       "\xf2\x0f\x10\x48\x08"  			/* movsd 0x8(%rax), %xmm1 */ \
             "\xeb\xb3");  				/* jmp [b0] */  \
        TAG_LABEL4(cont);
}

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

// ASM(0xcc); int3 trap: __asm__("int3");
#define X86_DEBUG() \
  X86_PRE(); ASM(0xB8); ASMN(potion_x86_debug); \
  ASM(0xFF); ASM(0xD0)

// TODO: finish jit backtraces using this
void potion_x86_debug() {
  Potion *P;
  int n = 0;
  _PN rax, rcx, rdx, *rbp, *sp;

#if POTION_X86 == POTION_JIT_TARGET
#if PN_SIZE_T != 8
  __asm__ ("mov %%eax, %0;"
           :"=r"(rax));
  __asm__ ("mov %%ecx, %0;"
           :"=r"(rcx));
  __asm__ ("mov %%edx, %0;"
           :"=r"(rdx));
  __asm__ ("mov %%ebp, %0;"
           :"=r"(sp));
#else
  __asm__ ("mov %%rax, %0;"
           :"=r"(rax));
  __asm__ ("mov %%rcx, %0;"
           :"=r"(rcx));
  __asm__ ("mov %%rdx, %0;"
           :"=r"(rdx));
  __asm__ ("mov %%rbp, %0;"
           :"=r"(sp));
#endif
  printf("RAX = 0x%lx (0x%x)\n", rax, potion_type(rax));
  printf("RCX = 0x%lx (0x%x)\n", rcx, potion_type(rcx));
  printf("RDX = 0x%lx (0x%x)\n", rdx, potion_type(rdx));
#endif

  P = (Potion *)sp[2];
  printf("Potion: %p (%p)\n", P, &P);

again:
  n = 0;
  rbp = (unsigned long *)*sp;
  if (rbp > sp - 2 && sp[2] == (PN)P) {
    printf("RBP = 0x%lx (0x%lx), SP = 0x%lx\n", (PN)rbp, *rbp, (PN)sp);
    while (sp < rbp) {
      printf("STACK[%d] = 0x%lx (0x%x)\n", n++, *sp, PN_TYPE(*sp));
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
    // need to address -(x)%ebp: max regn=29/14
    // assert(((regn + 1) * sizeof(PN)) < 0x7f);
#if PN_SIZE_T != 8
    // IA-32 cdecl ABI, non-microsoft only. TODO: win32 stdcall for the w32api ffi
    if (argn == 0) {
      // OPT: the first argument is always (Potion *)
      if (!out) {
	ASM(0x8b); ASM(0x55); ASM(2 * sizeof(PN));	//mov argn(%ebp), %edx
	ASMS("\x89\x14\x24");				//mov %edx, (%esp)
      }
    }
    else {
      if (out == 2) {
	ASM(0xc7); ASM(0x44); ASM(0x24); ASM(argn * sizeof(PN)); ASMI(regn); //mov $regn, argn(%esp)
      } else if (out) {
	ASM(0x8b); ASM_MOV_EBP(0x55,regn) 			//mov -0x8(%ebp), %edx
      }
      if (!out) argn += 2;
      if (out == 1) {
	ASMS("\x89\x54\x24"); ASM(argn * sizeof(PN));//mov %edx, argn(%esp)
      } else if (!out) {
	ASM(0x8b); ASM(0x55); ASM(argn * sizeof(PN));		//mov argn(%ebp), %edx
      }
      if (!out) {
	ASM(0x89); ASM_MOV_EBP(0x55,regn)			//mov %edx, regn(%ebp)
      }
    }
#else
  // sysv amd64 only (rdi,rsi,rdx,rcx,r8,r9), windows msvc not supported (rcx,rdx,r8,r9)
  // xmm0-7 not yet
  switch (argn) {
  case 0: //rdi Potion *P
    if (out == 2) { // unused - mov $regn, %rdi
      X86_PRE(); ASM(0xc7); ASM(0xc7); ASMI(regn);
    } else {
      X86_PRE(); ASM(out ? 0x8b : 0x89); ASM_MOV_EBP(0x7d,regn) // mov -regn(%rbp), %rdi
    }
    break;
  case 1: //rsi PN cl
    if (out == 2) { // unused - mov $regn, %rsi
      X86_PRE(); ASM(0xc7); ASM(0xc6); ASMI(regn);
    } else { // mov -regn(%rbp), %rsi
      X86_PRE(); ASM(out ? 0x8b : 0x89); ASM_MOV_EBP(0x75,regn)
    }
    break;
  case 2: //rdx self
    if (out == 2) { // unused - mov $regn, %rdx
      X86_PRE(); ASM(0xc7); ASM(0xc2); ASMI(regn);
    } else { // mov -regn(%rbp), %rdx
      X86_PRE(); ASM(out ? 0x8b : 0x89); ASM_MOV_EBP(0x55,regn)
    }
    break;
  case 3: //rcx
    if (out == 2) { // 1st default arg - mov $regn, %rcx
      X86_PRE(); ASM(0xc7); ASM(0xc1); ASMI(regn);
    } else { // mov -regn(%rbp), %rcx
      X86_PRE(); ASM(out ? 0x8b : 0x89); ASM_MOV_EBP(0x4d,regn)
    }
    break;
  case 4: //r8
    if (out == 2) { // 2nd default arg - mov $regn, %r8
      ASMS("\x49\xc7\xc0"); ASMI(regn);
    } else { // mov -regn(%rbp), %r8
      ASM(0x4c); ASM(out ? 0x8b : 0x89); ASM_MOV_EBP(0x45,regn)
    }
    break;
  case 5: //r9
    if (out == 2) { // 3rd default arg - mov $regn, %r9
      ASMS("\x49\xc7\xc1"); ASMI(regn);
    } else { // mov -regn(%rbp), %r9
      ASM(0x4c); ASM(out ? 0x8b : 0x89); ASM_MOV_EBP(0x4d,regn)
    }
    break;
    default: // can only pass max 6 arg via regs, rest on stack
      if (out) {
	X86_PRE(); ASM(0x8B); ASM_MOV_EBP(0x5d,regn) // mov %rbp(A) %rbx
	if (argn == 6) {
	  X86_PRE(); ASMS("\x89\x1c\x24");    // mov %rbx (%rsp)
	} else {
	  X86_PRE(); ASMS("\x89\x5c\x24"); ASM((argn - 6) * sizeof(PN)); // mov %rbx N(%rsp)
	}
      } else {
	X86_PRE(); ASM(0x8b); ASM(0x5d); ASM((argn - 4) * sizeof(PN));
	X86_PRE(); ASM(0x89); ASM_MOV_EBP(0x5d,regn) // mov %rbp(A) %rbx
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
  int rsp = X86C(16,0,0,0)+((need-X86C(8,0,0,0)+15)&~(15));
  if (rsp >= 0x80) {
    X86_PRE(); ASM(0x81); ASM(0xEC); ASMI(rsp); // sub rsp, %esp
  } else {
    X86_PRE(); ASM(0x83); ASM(0xEC); ASM(rsp);  // sub rsp, %esp
  }
}

void potion_x86_registers(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, long start) {
  PN_HAS_UPVALS(up);
  // (Potion *, self) in the first argument slot, self in the first register
  // DBG_v(";regs start %ld\n", start);
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
    int n = sizeof(struct PNClosure) + ((upi + 1) * sizeof(PN));
    X86_MOV_RBP(0x8B, start - 2); 	// mov -0x8(%ebp), %eax
    X86_PRE(); ASM(0x8B);  		// mov n(%eax), %eax
    if (n >= 0x80) { ASM(0x80); ASMI(n); } // if more than ~15 upvals
    else { ASM(0x40); ASM(n); }
    X86_MOV_RBP(0x89, lregs + upi);     // mov %eax, -0x8(%ebp)
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
  X86_MOV_RBP(0x89, op.a);				// mov %eax,-A(%ebp)
}

void potion_x86_self(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_MOV_RBP(0x8B, start - 1);			// mov %rsp(self), %rax
  X86_MOV_RBP(0x89, op.a);			// mov %eax,-A(%ebp)
}


// reg[op.a] = PN_IS_REF(locals[op.b]) ? PN_DEREF(locals[op.b]) : locals[op.b];
void potion_x86_getlocal(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long regs) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PN_HAS_UPVALS(up);
  X86_MOV_RBP(0x8B, regs + op.b); 		// mov %ebp(B) %eax
  if (up) { // upvals need to be deref'd
    ASMS("\xF6\xC0\x01" 		// test 0x1 %al
	 "\x75"); ASM(X86C(19,20, 0,0)); 		// jnz a
    ASMS("\xF7\xC0"); ASMI(PN_REF_MASK); 	// test REFMASK %eax
    ASM(0x74); ASM(X86C(11,12, 0,0)); 		// jz a
    ASMS("\x81\x38"); ASMI(PN_TWEAK); 	// cmpq WEAK (%eax)   # 0x250004
    ASM(0x75); ASM(X86C(3,4, 0,0)); 		// jnz a
    X86_PRE(); ASM(0x8B); ASM(0x40);
               ASM(sizeof(struct PNObject)); 	// mov N(%eax) %eax;  #WeakRef->data (Obj+PN)
  }
  X86_MOV_RBP(0x89, op.a); 		     // a: mov %eax %esp(A)
}

// if (PN_IS_REF(locals[op.b])) PN_DEREF(locals[op.b])  = reg[op.a];
// else locals[op.b] = reg[op.a];
void potion_x86_setlocal(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long regs) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PN_HAS_UPVALS(up);
  X86_PRE(); ASM(0x8B); ASM_MOV_EBP(0x55, op.a) 	// mov %rsp(A) %rdx
  if (up) { // upvals need to be deref'd
    X86_MOV_RBP(0x8B, regs + op.b); 			// mov %rsp(B) %rax
    ASMS("\xF6\xC0\x01" 				// test 0x1 %al
	 "\x75"); ASM(X86C(19,20, 0,0)); 		// jnz a
    ASMS("\xF7\xC0"); ASMI(PN_REF_MASK); 		// test REFMASK %eax
    ASM(0x74); ASM(X86C(11,12, 0,0)); 			// jz a
    ASMS("\x81\x38"); ASMI(PN_TWEAK); 			// cmpq WEAK (%rax) # 0x250004
    ASM(0x75); ASM(X86C(3,4, 0,0)); 			// jnz a
    X86_PRE(); ASM(0x89); ASM(0x50);
               ASM(sizeof(struct PNObject)); 		// mov N(%rax) %rax
  }
  X86_PRE(); ASM(0x89); ASM_MOV_EBP(0x55, regs + op.b)// a: mov %rdx %rsp(B)
}

// reg[op.a] = PN_DEREF(upvals[op.b])
void potion_x86_getupval(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long lregs) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_MOV_RBP(0x8B, lregs + op.b);
  X86_PRE(); ASM(0x8B); ASM(0x40); ASM(sizeof(struct PNObject));
  X86_MOV_RBP(0x89, op.a);
}

// PN_DEREF(upvals[op.b]) = reg[op.a]
// TODO: place the upval in the write barrier (or have stack scanning handle weak refs)
void potion_x86_setupval(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long lregs) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_PRE(); ASM(0x8B); ASM_MOV_EBP(0x55, op.a) 		// mov -A(%rbp) %edx
  X86_MOV_RBP(0x8B, lregs + op.b); 				// mov %rsp(B) %rax
  X86_PRE(); ASM(0x89); ASM(0x50); ASM(sizeof(struct PNObject));// mov %rdx %rax.data
}

void potion_x86_global(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0);
  X86_ARGO(op.a, 1);
  X86_ARGO(op.b, 2);
  X86_PRE(); ASM(0xB8); ASMN(potion_define_global);     // mov &potion_define_global, %rax
  ASM(0xFF); ASM(0xD0);					// callq %rax
  X86_MOV_RBP(0x8B, op.b); 				// mov -B(%rbp) %eax
  X86_PRE(); ASM(0x89); ASM_MOV_EBP(0x45, op.a) 	// mov %eax -A(%rbp)
}

void potion_x86_newtuple(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0);
  X86_PRE(); ASM(0xB8); ASMN(potion_tuple_empty);// mov &potion_tuple_empty %rax
  ASM(0xFF); ASM(0xD0); 			 // callq %rax
  X86_MOV_RBP(0x89, op.a); 			 // mov %rax local
}

// the fast version for unsafe unchecked direct access to the PNTuple offset
// with immediate constant directly, and the indirect version uses R(B-1024)
void potion_x86_gettuple(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(op.a, 0);				// movq -A(%rbp), %rdi
  X86_PRE(); ASM(0xB8); ASMN(potion_fwd);	// mov &potion_fwd, %rax
  ASM(0xFF); ASM(0xD0);				// callq %rax
  if (op.b & ASM_TPL_IMM) { // not immediate index. R(B-1024)
    X86_PRE();ASM(0x8b);ASM_MOV_EBP(0x55,op.b-ASM_TPL_IMM); // mov -B-1024(%rbp) %rdx
    X86_PRE();ASMS("\xd1\xea");                // shr %rdx,1
    //ASM(0xcc);                  // displacement: 0x90 or 0xd0
    X86_PRE();ASM(0x8b);ASM(0x44);ASM((5+PN_SIZE_T)<<4);ASM(0x10);// mov 0x10(%rax,%rdx,PN_SIZE_T),%rax
  } else { // immediate index B
    X86_PRE();ASM(0xc7);ASM(0xc2);ASMI(op.b+2);         // mov B+$2, %rdx #PNTuple+2
    X86_PRE();ASM(0x8b);ASM(0x04);ASM((5+PN_SIZE_T)<<4);// mov (%rax,%rdx,PN_SIZE_T),%rax
  }
  X86_MOV_RBP(0x89, op.a); 		    	// mov %rax local
  return;
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

void potion_x86_gettable(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0);
  //X86_ARGO(0, 1);
  X86_ARGO(op.a, 2);
  X86_ARGO(op.b, 3);
  X86_PRE(); ASM(0xB8); ASMN(potion_table_at);  // mov &potion_table_set %rax
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
    X86_PRE(); ASMS("\x8D\x44\x10\xFF"); // lea -1(%eax,%edx,1),%eax
    ASM(0x70); ASM(2);                   // jo +2
  });
}

void potion_x86_sub(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_MATH(1, potion_obj_sub, {
    X86_PRE(); ASM(0x29); ASM(0xD0); // sub %edx %eax
    ASM(0x70); ASM(X86_PRE_T + 4);   // jo +4
    X86_PRE(); ASM(0xFF); ASM(0xC0); // inc %eax
  });
}

void potion_x86_mult(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_MATH(1, potion_obj_mult, {
    X86_PRE(); ASMS("\xD1\xFA"); 	// sar %rdx
    X86_PRE(); ASMS("\xFF\xC8"); 	// dec %rax
    X86_PRE(); ASMS("\x0F\xAF\xC2"); 	// imul %rdx %rax
    ASM(0x70); ASM(X86_PRE_T + 4);      // jo +4
    X86_PRE(); ASMS("\xFF\xC0"); 	// inc %rax
  });
}

void potion_x86_div(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_MATH(1, potion_obj_div, {
    ASMS("\xD1\xF8" 		// sar %rax
	 "\xD1\xFA" 		// sar %edx
	 "\x89\xD1" 		// mov %edx %ecx
	 "\x89\xC2" 		// mov %eax %edx
	 "\xC1\xFA\x1F" 	// sar 0x1f %edx
	 "\xF7\xF9" 		// idiv %ecx
	 "\x8D\x44\x00\x01");	// lea 0x1(%eax,%eax,1),%eax
  });
}

void potion_x86_rem(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_MATH(1, potion_obj_rem, {
    ASMS("\xD1\xF8"	  	// sar %rax
	 "\xD1\xFA" 		// sar %edx
	 "\x89\xD1" 		// mov %edx %ecx
	 "\x89\xC2" 		// mov %eax %edx
	 "\xC1\xFA\x1F" 	// sar 0x1f %edx
	 "\xF7\xF9" 		// idiv %ecx
	 "\x8D\x44\x12\x01");   // lea 0x1(%edx,%edx,1),%eax
  });
}

void potion_x86_pow(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_ARGO(start - 3, 0);
  X86_ARGO(op.a, 2);
  X86_ARGO(op.b, 3);
  X86_PRE(); ASM(0xB8); ASMN(potion_num_pow);// mov &potion_num_pow %rax
  ASM(0xFF); ASM(0xD0); 		 // callq %rax
  X86_MOV_RBP(0x89, op.a); 		 // mov %rax local
}

void potion_x86_neq(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  x86_cmp(P, asmp, op, 0x75, 0x85);	 // jne
  //X86_CMP(0x75, 0x85);	 	 // jne
}

void potion_x86_eq(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  x86_cmp(P, asmp, op, 0x74, 0x84);	 // je
  //X86_CMP(0x74, 0x84);		 // je
}

void potion_x86_lt(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_NUMCMP(0x7C, 0x72,  		// jl, jb
             "\x66\x0F\x2e\xc1" 	// ucomisd %xmm1, %xmm0
             );
}

void potion_x86_lte(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_NUMCMP(0x7E, 0x76,		// jle, jbe
             "\x66\x0F\x2e\xc1" 	// ucomisd %xmm1, %xmm0
             );
}

void potion_x86_gt(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_NUMCMP(0x7F, 0x77,		// jg, ja
             "\x66\x0F\x2e\xc1" 	// ucomisd %xmm1, %xmm0
             );
}

void potion_x86_gte(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_NUMCMP(0x7D, 0x73, 		// jge, jae
             "\x66\x0F\x2e\xc1" 	// ucomisd %xmm1, %xmm0
             );
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
      ASMS("\xD1\xF8" 		// sar %eax
	   "\xD1\xFA" 		// sar %edx
	   "\x89\xD1" 		// mov %edx %ecx
	   "\x89\xC2" 		// mov %eax %edx
	   "\xD3\xE0"); 	// sal %cl %eax
      ASM(0x70); ASM(6);        // jo +6
      ASMS("\x8D\x44\x00\x01"); // lea 0x1(%eax,%eax,1),%eax
  });
}

void potion_x86_bitr(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_MATH(1, potion_obj_bitr, {
    ASMS("\xD1\xF8" 		// sar %rax
	 "\xD1\xFA"		// sar %edx
	 "\x89\xD1"		// mov %edx %ecx
	 "\x89\xC2"		// mov %eax %edx
	 "\xD3\xF8");		// sar %cl %eax
    ASM(0x70); ASM(6);          // jo +6
    ASMS("\x8D\x44\x00\x01");	// lea 0x1(%eax,%eax,1),%eax
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
  int false1, false2, true1;
  PN_OP op = PN_OP_AT(f->asmb, pos);
  X86_MOV_RBP(0x8B, op.a); 				// mov -A(%rbp) %rax
  X86_PRE(); ASM(0x83); ASM(0xF8); ASM(PN_FALSE); 	// cmp FALSE %rax
  TAG_PREP(false1); ASM(0x74); ASM(0);			// je false (+13,21)
  X86_PRE(); ASM(0x85); ASM(0xC0); 			// test %rax %rax
  TAG_PREP(false2); ASM(0x74); ASM(0);			// jz false (+9,16)
  X86_MOVQ(op.a, test ? PN_FALSE : PN_TRUE); 		// -A(%rbp) = TRUE
  TAG_PREP(true1); ASM(0xEB); ASM(0);			// jmp true (+7,14)
  TAG_LABEL(false1); TAG_LABEL(false2);			//false:
  X86_MOVQ(op.a, test ? PN_TRUE : PN_FALSE); 		// -A(%rbp) = FALSE
  TAG_LABEL(true1);					//true:
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
  X86_PRE(); ASM(0x83); ASM(0xF8); ASM(PN_FALSE); 	// cmp FALSE %rax
  ASM(0x74); ASM(X86C(9,10, 0,0)); 			// jz false (+9)
  X86_PRE(); ASM(0x85); ASM(0xC0); 			// test %rax %rax
  ASM(0x74); ASM(5);					// jz false (+5)
  TAG_JMP(pos + op.b);					//true:
} // false:

void potion_x86_notjmp(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, PNJumps *jmps, size_t *offs, int *jmpc) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  DBG_t("; notjmp %d => %d\n", op.a, op.b);
  X86_MOV_RBP(0x8B, op.a);				// mov -A(%rbp) %rax
  X86_PRE(); ASM(0x83); ASM(0xF8); ASM(PN_FALSE);	// cmp FALSE %rax
  ASM(0x74); ASM(X86C(4,5, 0,0));			// jz false (+4)
  X86_PRE(); ASM(0x85); ASM(0xC0);			// test %rax %rax
  ASM(0x75); ASM(5);					// jnz true (+5)
  TAG_JMP(pos + op.b);					//false:
} // true:

void potion_x86_named(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  int tag;
  PN_OP op = PN_OP_AT(f->asmb, pos);
  DBG_t("; named %d %d\n", op.a, op.b);
  X86_ARGO(start - 3, 0); 				//P
  X86_ARGO(op.a, 1);      				//cl
  X86_ARGO(op.b - 1, 2);  				//name
  X86_PRE(); ASM(0xB8); ASMN(potion_sig_find); 		// mov &potion_sig_find %rax
  ASMS("\xFF\xD0"					// callq %eax
       "\x85\xC0");					// test %eax %eax
  TAG_PREP(tag);
  ASM(0x78); ASM(0);					// js +12
  X86_PRE(); ASM(0xF7); ASM(0xD8);			// neg %rax
  X86_PRE(); ASM(0x8B); ASM_MOV_EBP(0x55, op.b)		// mov -B(%rbp) %rdx
#if PN_SIZE_T != 8
  ASM(0x89); ASM(0x54); ASM_MOV_EBP(0x85, op.a + 2)	// mov %edx -A(%ebp,%eax,4)
#else
  if (op.a + 2 > 15) {
    DBG_v("named: mov %%rdx -A=%d(%%rbp,%%rax,8)\n", op.a + 2); //!! +2 only XXX
    X86_PRE(); ASM(0x89); ASM(0x94); ASM(0xC5); ASM(RBP(op.a + 2)); ASM(0xff); ASM(0xff);
  } else {
    X86_PRE(); ASM(0x89); ASM(0x54); ASM(0xC5); ASM(RBP(op.a + 2)); // mov %rdx -A(%rbp,%rax,8)
  }
#endif
  TAG_LABEL(tag);
}

// TODO: check for bytecode nodes and jit them as well?
void potion_x86_call(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  int argc = op.b - op.a; // including self
  int i, tag_a1, tag_a2, tag_b, tag_c, tag_d;

  // check type of the closure
  X86_PRE(); ASM(0x8B); ASM_MOV_EBP(0x45, op.a)		// mov %rbp(A) %rax
  ASM(0xF6); ASM(0xC0); ASM(0x01);			// test 0x1 %al
  TAG_PREP(tag_a1);
  ASM(0x75); ASM(X86C(56,68, 3,op.a));	 		// jnz [a]
  ASM(0xF7); ASM(0xC0); ASMI(PN_REF_MASK);		// test REFMASK %eax
  TAG_PREP(tag_a2);
  ASM(0x74); ASM(X86C(48,60, 5,op.a));			// jz [a]
  X86_PRE(); ASM(0x83); ASM(0xE0); ASM(0xF8);		// and ~PRIMITIVE %rax

  // if a class, pull out the constructor
  ASM(0x81); ASM(0x38); ASMI(PN_TVTABLE);		// cmpq VTABLE (%eax)  # 0x25000a
  TAG_PREP(tag_c);
  ASM(0x75); ASM(X86C(13,20, 1, start-3));		// jnz [c]
  X86_ARGO(start - 3, 0);				// mov &P 0(%esp)
  X86_ARGO(op.a, 2);					// mov A 2(%esp)
  X86_PRE(); ASM(0xB8); ASMN(potion_object_new);	// mov &potion_object_new %rax
  ASM(0xFF); ASM(0xD0); 				// callq %rax
  X86_MOV_RBP(0x89, op.a + 1); 			        // mov %rax local
  X86_PRE(); ASM(0x8B); ASM_MOV_EBP(0x45, op.a)		// mov %rbp(A) %rax
  X86_PRE(); ASM(0x8B); ASM(0x40);
    ASM((char *)&((struct PNVtable *)P->lobby)->ctor
	- (char *)P->lobby); 				// mov N(%rax) %rax
  X86_PRE(); ASM(0x89); ASM_MOV_EBP(0x45, op.a)		// mov %rax %rbp(A)

  // check type of the closure
  TAG_LABEL(tag_c);
  ASM(0x81); ASM(0x38); ASMI(PN_TCLOSURE);	     // c: cmpq CLOSURE (%eax) # 0x250005
  TAG_PREP(tag_d);
  ASM(0x74); ASM(X86C(22,30, 2,op.a));			// jz [d]

  // if not a closure, get the type's closure
  X86_MOV_RBP(0x8B, op.a);
  TAG_LABEL(tag_a1); TAG_LABEL(tag_a2);
  X86_MOV_RBP(0x89, op.a + 1);
  X86_ARGO(start - 3, 0);			     // a: mov &P 0(%esp)
  X86_ARGO(op.a, 1);					// mov A 1(%esp)
  X86_PRE(); ASM(0xB8); ASMN(potion_obj_get_call); 	// mov &potion_obj_get_call %rax
  ASM(0xFF); ASM(0xD0); 			        // callq *%rax
  TAG_PREP(tag_b);
  ASM(0xEB); ASM(X86C(3,4, 1,op.a)); 		       	// jmp [b]

  // get the closure's function
  TAG_LABEL(tag_d);
  X86_PRE(); ASM(0x8B); ASM_MOV_EBP(0x45, op.a)      // d: mov %rbp(A) %rax
  //[b]: got the method, call it (first special slot from PNClosure)
  TAG_LABEL(tag_b);
  X86_PRE(); ASM(0x8B); ASM(0x40);
             ASM(sizeof(struct PNObject));	     // b: mov N(%rax) %rax

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
  ASM(0xFF); ASM(0xD0);    // callq %rax
  X86_MOV_RBP(0x89, op.a); // mov %rax local
}

/*TODO*/
void potion_x86_tailcall(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
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

/* OP_PROTO */
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
    int n;
    (*pos)++;
    PN_OP opp = PN_OP_AT(f->asmb, *pos);
    if (opp.code == OP_GETUPVAL) {
      X86_PRE(); ASM(0x8B); ASM_MOV_EBP(0x55, lregs + opp.b) // mov upval %rdx
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
    X86_PRE(); ASM(0x89);			// mov %rdx N(%rax)
    n = sizeof(struct PNClosure) + (sizeof(PN) * (i + 1));
    if (n >= 0x80) { ASM(0x90); ASMI(n); }
    else { ASM(0x50); ASM(n); }
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
#if PN_SIZE_T != 8
  ASMS("\x55" 				// push %ebp
       "\x89\xE5"			// mov %esp %ebp
       "\x8B\x55\x08");			// mov 0x8(%ebp) %edx
#endif
  for (k = kh_end(vt->methods); k > kh_begin(vt->methods); k--) {
    if (kh_exist(PN, vt->methods, k - 1)) {
      ASM(0x81); ASM(X86C(0xFA,0xFF, 0,0));
        ASMI(PN_UNIQ(kh_key(PN, vt->methods, k - 1)));		// cmp NAME %edi
        ASM(0x75); ASM(X86C(7,11, 0,0));			// jnz +11
      X86_PRE(); ASM(0xB8); ASMN(kh_val(PN, vt->methods, k - 1)); // mov CL %rax
#if PN_SIZE_T != 8
      ASM(0x5D);
#endif
      ASM(0xC3); // retq
    }
  }
  ASM(0xB8); ASMI(0); // mov NIL %eax
#if PN_SIZE_T != 8
  ASM(0x5D);
#endif
  ASM(0xC3); // retq
}

void potion_x86_ivars(Potion *P, PN ivars, PNAsm * volatile *asmp) {
#if PN_SIZE_T != 8
  ASMS("\x55" 				// push %ebp
       "\x89\xE5"			// mov %esp %ebp
       "\x8B\x55\x08");			// mov 0x8(%ebp) %edx
#else
#endif
#if PN_SIZE_T != 8
  PN_TUPLE_EACH(ivars, i, v, {
    ASM(0x81); ASM(X86C(0xFA,0xFF, 0,0));
    ASMI(PN_UNIQ(v));			// cmp UNIQ %edi
    ASM(0x75); ASM(X86C(7,6, 0,0));	// jnz +7
    ASM(0xB8); ASMI(i);			// mov i %rax
    ASM(0x5D);                          // pop %rbp
    ASM(0xC3);				// retq
  });
#else
  PN_TUPLE_EACH(ivars, i, v, {
    ASM(0x81); ASM(X86C(0xFA,0xFF, 0,0));
    ASMI(PN_UNIQ(v));			// cmp UNIQ %edi
    ASM(0x75); ASM(X86C(7,6, 0,0));	// jnz +7
    ASM(0xB8); ASMI(i);			// mov i %rax
    ASM(0xC3);				// retq
  });
#endif
  X86_PRE(); ASM(0xB8); ASMN(-1);	// mov -1 %rax
#if PN_SIZE_T != 8
  ASM(0x5D);                            // pop %rbp
#endif
  ASM(0xC3);				// retq
}

MAKE_TARGET(x86);
