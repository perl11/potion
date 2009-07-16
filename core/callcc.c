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
  PN_SIZE n;
  PN rcx, *start, *end, *sp1 = P->mem->cstack, *sp2 = NULL;
#if POTION_STACK_DIR > 0
  start = (PN *)cc->stack[0];
  end = (PN *)cc->stack[1];
#else
  start = (PN *)cc->stack[1];
  end = (PN *)cc->stack[0];
#endif

  if ((PN)sp1 != cc->stack[0]) {
    fprintf(stderr, "** TODO: continuations which switch stacks must be rewritten.\n");
    return PN_NIL;
  }

  //
  // move stack pointer, fill in stack, resume
  //
#ifdef POTION_X86
#if __WORDSIZE == 64
  __asm__ ("mov 0x8(%3), %%rsp;"
           "mov 0x10(%3), %%rbp;"
           "add $0x18, %3;"
        "loop:"
           "mov (%3), %%rax;"
           "add $0x8, %1;"
           "mov %%rax, (%1);"
           "add $0x8, %3;"
           "cmp %1, %2;"
           "jne loop;"
           "mov %0, %%rax;"
           "leave; ret"
           :/* no output */
           :"r"(cl), "r"(start), "r"(end), "r"(cc->stack)
           :"%rax", "%rsp"
          );
#else
  __asm__ ("mov 0x4(%3), %%esp;"
           "mov 0x8(%3), %%ebp;"
           "add $0xc, %3;"
        "loop:"
           "mov (%3), %%eax;"
           "add $0x4, %1;"
           "mov %%eax, (%1);"
           "add $0x4, %3;"
           "cmp %1, %2;"
           "jne loop;"
           "mov %0, %%eax;"
           "leave; ret"
           :/* no output */
           :"r"(cl), "r"(start), "r"(end), "r"(cc->stack)
           :"%eax", "%esp"
          );
#endif
#else
  fprintf(stderr, "** TODO: callcc does not work outside of X86.\n");
#endif
  return cl;
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

  cc = PN_ALLOC_N(PN_TCONT, struct PNCont, sizeof(PN) * (n + 2));
  cc->len = n + 2;
  cc->stack[0] = (PN)sp1;
  cc->stack[1] = (PN)sp2;
  cc->stack[2] = (PN)sp3;
  PN_MEMCPY_N((char *)(cc->stack + 3), start + 1, PN, n - 1);
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

