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
#if __WORDSIZE == 64
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
  __asm__ ("mov 0x8(%%rdx), %%rsp;"
           "add $0x10, %%rdx;"
        "loop:"
           "mov (%%rdx), %%r8d;"
           "add $0x8, %%rbx;"
           "mov %%r8d, (%%rbx);"
           "add $0x8, %%rdx;"
           "cmp %%rbx, %%rcx;"
           "jne loop;"
           :"=a"(rcx)
           :"a"(cc->data[12] + 3), "b"(start), "c"(end), "d"(cc->data)
          );
#endif
  return PN_NIL;
}

PN potion_callcc(Potion *P, PN cl, PN self) {
#if __WORDSIZE == 64
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
#else
  fprintf(stderr, "** TODO: callcc for 32-bit.\n");
  return PN_NIL;
#endif
}
