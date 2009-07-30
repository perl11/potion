//
// callcc.c
// creation and calling of continuations
//
// NOTE: these hacks make use of the frame pointer, so they must
// be compiled -fno-omit-frame-pointer!
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include "potion.h"
#include "internal.h"

PN potion_continuation_yield(Potion *P, PN cl, PN self) {
  int i = 0, diff;
  struct PNCont *cc = (struct PNCont *)self;
  PN rcx, *start, *end, *sp1 = P->mem->cstack, *sp2 = NULL;
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

  //
  // move stack pointer, fill in stack, resume
  //
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
           :"%eax", "%esp", "%ebp", "%esi"
          );
#endif
#else
  fprintf(stderr, "** TODO: callcc does not work outside of X86.\n");
#endif
  return self;
}

PN potion_callcc(Potion *P, PN cl, PN self) {
  struct PNCont *cc;
  PN_SIZE n;
  PN *start, *end, *sp1 = P->mem->cstack, *sp2, *sp3;
  POTION_ESP(&sp2);
  POTION_EBP(&sp3);
#if POTION_STACK_DIR > 0
  n = sp2 - sp1;
  start = sp1;
  end = sp2;
#else
  n = sp1 - sp2 + 1;
  start = sp2;
  end = sp1;
#endif

  cc = PN_ALLOC_N(PN_TCONT, struct PNCont, sizeof(PN) * (n + 3 + PN_SAVED_REGS));
  cc->len = n + 3;
  cc->stack[0] = (PN)sp1;
  cc->stack[1] = (PN)sp2;
  cc->stack[2] = (PN)sp3;
  cc->stack[3] = PN_NIL;
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
  PN_MEMCPY_N((char *)(cc->stack + 4 + PN_SAVED_REGS), start + 1, PN, n - 1);
  return (PN)cc;
}

PN potion_continuation_string(Potion *P, PN cl, PN self) {
  return potion_str(P, "Continuation");
}

void potion_cont_init(Potion *P) {
  PN cnt_vt = PN_VTABLE(PN_TCONT);
  potion_type_call_is(cnt_vt, PN_FUNC(potion_continuation_yield, 0));
  potion_method(cnt_vt, "string", potion_continuation_string, 0);
}

