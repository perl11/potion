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
}

CuSuite *gc_suite() {
  CuSuite *S = CuSuiteNew();
  SUITE_ADD_TEST(S, gc_test_start);
  return S;
}

int main(void) {
  int count;
  M = potion_gc_boot()->mem;
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
