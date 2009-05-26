//
// gc-test.c
// rudimentary garbage collection testing
// (see core/gc.c for the lightweight garbage collector,
// based on ideas from Qish by Basile Starynkevitch.)
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "potion.h"
#include "internal.h"
#include "gc.h"
#include "CuTest.h"

struct PNMemory *M;

void gc_test_start(CuTest *T) {
  CuAssert(T, "GC struct isn't at start of first page", M == M->birth_lo);
  CuAssert(T, "stack length is not a positive number", potion_stack_len(M, NULL) > 0);
}

void gc_test_alloc1(CuTest *T) {
  PN ptr = potion_gc_alloc(M, 16);
  PN_SIZE count = potion_mark_stack(M);
  CuAssert(T, "couldn't allocate 16 bytes from GC", PN_IS_PTR(ptr));
  CuAssertIntEquals(T, "only one allocation should be found", count, 1);
}

CuSuite *gc_suite() {
  CuSuite *S = CuSuiteNew();
  SUITE_ADD_TEST(S, gc_test_start);
  SUITE_ADD_TEST(S, gc_test_alloc1);
  return S;
}

int main(void) {
  POTION_INIT_STACK(sp);
  int count;
  M = potion_gc_boot(sp)->mem;
  CuString *out = CuStringNew();
  CuSuite *suite = gc_suite();
  CuSuiteRun(suite);
  CuSuiteSummary(suite, out);
  CuSuiteDetails(suite, out);
  printf("%s\n", out->buffer);
  count = suite->failCount;
  CuSuiteFree(suite);
  CuStringFree(out);
  return count;
}
