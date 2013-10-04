///\file callcc.c
/// creation and calling of continuations, in non-portable asm, x86 only yet
//
// NOTE: these hacks make use of the frame pointer, so they must
// be compiled -fno-omit-frame-pointer!
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include "potion.h"
#include "internal.h"

/**\memberof PNCont
  "yield" method
  \param self PNCont
  \see potion_callcc()
  \returns does not return, continues execution at the position of the given PNCont */
PN potion_continuation_yield(Potion *P, PN cl, PN self) {
  struct PNCont *cc = (struct PNCont *)self;
  PN *start, *end, *sp1 = P->mem->cstack;
#if POTION_STACK_DIR > 0
  start = (PN *)cc->stack[0];
  end = (PN *)cc->stack[1];
#else
  start = (PN *)cc->stack[1];
  end = (PN *)cc->stack[0];
#endif

  if ((PN)sp1 != cc->stack[0]) {
    fprintf(stderr, "** TODO: continuations which switch stacks must be rewritten. (%p != %p)\n",
      sp1, (void *)(cc->stack[0]));
    return PN_NIL;
  }
  DBG_vt("\nyield: start=%p, end=%p, cc=%p\n", start, end, cc->stack);

  //
  // move stack pointer, fill in stack, resume
  cc->stack[3] = (PN)cc;
#if POTION_X86 == POTION_JIT_TARGET
#if __WORDSIZE == 64
  __asm__ ("mov 0x8(%2), %%rsp;"
           "mov 0x10(%2), %%rbp;"
           "mov %2, %%rbx;"
           "add $0x48, %2;"
        "loop:"
           "mov (%2), %%rax;"
           "add $0x8, %0;"
           "mov %%rax, (%0);"
           "add $0x8, %2;"
           "cmp %0, %1;"
           "jne loop;"
           "mov 0x18(%%rbx), %%rax;"
           "movq $0x0, 0x18(%%rbx);"
           "mov 0x28(%%rbx), %%r12;"
           "mov 0x30(%%rbx), %%r13;"
           "mov 0x38(%%rbx), %%r14;"
           "mov 0x40(%%rbx), %%r15;"
           "mov 0x20(%%rbx), %%rbx;"
           "leave; ret"
           :/* no output */
           :"r"(start), "r"(end), "r"(cc->stack)
           :"%rax", "%rsp", "%rbx"
          );
#else
  __asm__ ("mov 0x4(%2), %%esp;"
           "mov 0x8(%2), %%ebp;"
           "mov %2, %%esi;"
           "add $0x1c, %2;"
        "loop:"
           "mov (%2), %%eax;"
           "add $0x4, %0;"
           "mov %%eax, (%0);"
           "add $0x4, %2;"
           "cmp %0, %1;"
           "jne loop;"
           "mov 0xc(%%esi), %%eax;"
           "mov 0x14(%%esi), %%edi;"
           "mov 0x18(%%esi), %%ebx;"
           "mov 0x10(%%esi), %%esi;"
           "leave; ret"
           :/* no output */
           :"r"(start), "r"(end), "r"(cc->stack)
           :"%eax", "%esp", /*"%ebp",*/ "%esi"
          );
  //DBG_vt("yield => start=%p, end=%p, cc=%p\n", start, end, cc->stack);
#endif
#else
  fprintf(stderr, "** TODO: callcc/yield does not work outside of X86 yet.\n");
#endif
#ifdef DEBUG
  if (!P->strings || !P->lobby || !P->mem)
    potion_fatal("fatal: yield stack underflow\n");
#endif
  return self;
}

/**\memberof PNVtable
   global "here" method
   \see potion_continuation_yield()
   \returns a PNCont continuation object which can be yield'ed to later */
ATTRIBUTE_NO_ADDRESS_SAFETY_ANALYSIS
PN potion_callcc(Potion *P, PN cl, PN self) {
  struct PNCont *cc;
  PN_SIZE n;
  PN *start, *sp1 = P->mem->cstack, *sp2, *sp3;
#if defined(DEBUG) && defined(__APPLE__)
  if ((_PN)sp1 & 0xF) {
    fprintf(stderr,"P->mem->cstack=0x%lx ", (_PN)sp1);
    potion_fatal("stack not 16byte aligned");
  }
#elif defined(DEBUG) && (__WORDSIZE == 64)
  if (((_PN)sp1 & 0xF) != 0 && ((_PN)sp1 & 0xF) != 8) {
    fprintf(stderr,"P->mem->cstack=0x%lx ", (_PN)sp1);
    potion_fatal("stack not 8byte aligned");
  }
#endif
  POTION_ESP(&sp2); // usually P
  POTION_EBP(&sp3);
#if POTION_STACK_DIR > 0
  n = sp2 - sp1;
  start = sp1;
#else
  n = sp1 - sp2 + 1;
  start = sp2;
#endif

  cc = PN_ALLOC_N(PN_TCONT, struct PNCont, sizeof(PN) * (n + 3 + PN_SAVED_REGS));
  cc->len = n + 3;
  cc->stack[0] = (PN)sp1;
  cc->stack[1] = (PN)sp2;
  cc->stack[2] = (PN)sp3;
  cc->stack[3] = PN_NIL;
  DBG_vt("\ncallcc: start=%p, end=%p, cc=%p\n", start, sp2, cc->stack);
#if POTION_X86 == POTION_JIT_TARGET
#if __WORDSIZE == 64
  __asm__ ("mov %%rbx, 0x20(%0);"
           "mov %%r12, 0x28(%0);"
           "mov %%r13, 0x30(%0);"
           "mov %%r14, 0x38(%0);"
           "mov %%r15, 0x40(%0);"::"r"(cc->stack));
#else
  __asm__ ("mov %%esi, 0x10(%0);"
           "mov %%edi, 0x14(%0);"
           "mov %%ebx, 0x18(%0)"::"r"(cc->stack));
#endif
#endif

// avoid wrong asan stack underflow, caught in memcpy
#if defined(__clang__) && defined(__SANITIZE_ADDRESS__)
  {
    PN *s = start + 1;
    PN *d = cc->stack + 4 + PN_SAVED_REGS;
    for (int i=0; i < n - 1; i++) {
      *d++ = *s++;
    }
  }
#else
  PN_MEMCPY_N((char *)(cc->stack + 4 + PN_SAVED_REGS), start + 1, PN, n - 1);
#endif
  return (PN)cc;
}

void potion_cont_init(Potion *P) {
  PN cnt_vt = PN_VTABLE(PN_TCONT);
  potion_type_call_is(cnt_vt, PN_FUNC(potion_continuation_yield, 0));
}

