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

unsigned long *bottom_oh_the_stack(Potion *P, unsigned long *sp) {
  unsigned long *rbp;
again:
  rbp = (unsigned long *)*sp;
  if (rbp > sp && *rbp != (PN)P) {
    sp = rbp;
    goto again;
  }
  return rbp;
}

PN potion_callcc_yield(Potion *P, PN cl, PN self) {
#if __WORDSIZE == 64
  int i = 0, diff;
  unsigned long *rsp, *rbp;
  struct PNClosure *cc = PN_CLOSURE(cl);

  __asm__ ("mov %%rbp, %0;"
           :"=r"(rsp)
          );
  rbp = bottom_oh_the_stack(P, rsp);
  if ((PN)rbp != cc->data[0]) {
    fprintf(stderr, "** TODO: continuations which switch stacks must be rewritten.\n");
    return PN_NIL;
  }

  diff = cc->data[1] - (PN)rsp;
  __asm__ ("mov %%rax, %%rsp"
           : /* no output */
           :"a"(cc->data[1])
          );
  rbp = (unsigned long *)cc->data[1];
  PN_MEMCPY_N(rbp, (char *)cc->data + 2, PN, cc->extra - 2);
  __asm__ ("mov %%rcx, %%rbp;"
           "jmpq *%%rax"
           : /* no output */
           :"a"(cc->data[3] + 8), "c"(cc->data[2])
          );
#endif
  return PN_NIL;
}

PN potion_callcc(Potion *P, PN cl, PN self) {
#if __WORDSIZE == 64
  struct PNClosure *cc;
  unsigned long *rsp, *rbp;
  __asm__ ("mov %%rbp, %0;"
           :"=r"(rsp)
          );
  rbp = bottom_oh_the_stack(P, rsp);

  cc = (struct PNClosure *)potion_closure_new(P, (PN_F)potion_callcc_yield, PN_NIL, (rbp - rsp) + 2);
  cc->data[0] = (PN)rbp;
  cc->data[1] = (PN)rsp;
  PN_MEMCPY_N((char *)cc->data + 2, rsp, PN, rbp - rsp);
  return (PN)cc;
#else
  fprintf(stderr, "** TODO: callcc for 32-bit.\n");
  return PN_NIL;
#endif
}
