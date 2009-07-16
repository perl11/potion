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

PN potion_callcc_yield(Potion *P, PN cl, PN self) {
  int i = 0, diff;
  struct PNClosure *cc = PN_CLOSURE(cl);
  PN_SIZE n;
  PN rcx, *start, *end, *sp1 = P->mem->cstack, *sp2 = NULL;
#if POTION_STACK_DIR > 0
  start = (PN *)cc->data[0];
  end = (PN *)cc->data[1];
#else
  start = (PN *)cc->data[1];
  end = (PN *)cc->data[0];
#endif

  if ((PN)sp1 != cc->data[0]) {
    fprintf(stderr, "** TODO: continuations which switch stacks must be rewritten.\n");
    return PN_NIL;
  }

  //
  // move stack pointer, fill in stack, resume
  //
#ifdef POTION_X86
#if __WORDSIZE__ == 64
  __asm__ ("mov 0x8(%3), %%rsp;"
           "add $0x10, %3;"
        "loop:"
           "mov (%3), %%rax;"
           "add $0x8, %1;"
           "mov %%rax, (%1);"
           "add $0x8, %3;"
           "cmp %1, %2;"
           "jne loop;"
           "mov %4, %%rax;"
           "leave; ret"
           :/* no output */
           :"r"(cc->data[12] + 3), "r"(start), "r"(end), "r"(cc->data),
            "r"(cl)
           :"%rax", "%rsp"
          );
#else
  __asm__ ("mov 0x4(%3), %%esp;"
           "add $0x8, %3;"
        "loop:"
           "mov (%3), %%eax;"
           "add $0x4, %1;"
           "mov %%eax, (%1);"
           "add $0x4, %3;"
           "cmp %1, %2;"
           "jne loop;"
           "mov %4, %%eax;"
           "leave; ret"
           :/* no output */
           :"r"(cc->data[12] + 3), "r"(start), "r"(end), "r"(cc->data),
            "r"(cl)
           :"%eax", "%esp"
          );
#endif
#else
  fprintf(stderr, "** TODO: callcc does not work outside of X86.\n");
#endif
  return cl;
}

PN potion_callcc(Potion *P, PN cl, PN self) {
  struct PNClosure *cc;
  PN_SIZE n;
  PN *start, *end, *sp1 = P->mem->cstack, *sp2 = NULL;
  POTION_ESP(&sp2);
#if POTION_STACK_DIR > 0
  n = sp2 - sp1;
  start = sp1;
  end = sp2;
#else
  n = sp1 - sp2 + 1;
  start = sp2;
  end = sp1;
#endif

  cc = (struct PNClosure *)potion_closure_new(P, (PN_F)potion_callcc_yield, PN_NIL, n + 1);
  cc->data[0] = (PN)sp1;
  cc->data[1] = (PN)sp2;
  PN_MEMCPY_N((char *)(cc->data + 2), start + 1, PN, n - 1);
  return (PN)cc;
}
