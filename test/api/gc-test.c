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

//
// everything allocated in alloc1 and alloc4 tests goes out of scope, so will
// not be moved to the old generation. data in the `forward` test will be copied.
//
void gc_test_alloc1(CuTest *T) {
  PN ptr = (PN)potion_gc_alloc(M, PN_TUSER, 16);
  PN_SIZE count = potion_mark_stack(M, 0);
  CuAssert(T, "couldn't allocate 16 bytes from GC", PN_IS_PTR(ptr));
  CuAssertIntEquals(T, "only one allocation should be found", 1, count);
}

void gc_test_alloc4(CuTest *T) {
  PN ptr = (PN)potion_gc_alloc(M, PN_TUSER, 16);
  PN ptr2 = (PN)potion_gc_alloc(M, PN_TUSER, 16);
  PN ptr3 = (PN)potion_gc_alloc(M, PN_TUSER, 16);
  PN ptr4 = (PN)potion_gc_alloc(M, PN_TUSER, 16);
  PN_SIZE count = potion_mark_stack(M, 0);
  CuAssert(T, "couldn't allocate 16 bytes from GC", PN_IS_PTR(ptr));
  CuAssert(T, "couldn't allocate 16 bytes from GC", PN_IS_PTR(ptr2));
  CuAssert(T, "couldn't allocate 16 bytes from GC", PN_IS_PTR(ptr3));
  CuAssert(T, "couldn't allocate 16 bytes from GC", PN_IS_PTR(ptr4));
  CuAssertIntEquals(T, "four allocations should be found", 4, count);
}

void gc_test_forward(CuTest *T) {
  PN_SIZE count;
  char *fj = "frances johnson.";
  PN ptr = potion_data_alloc(M, 16);
  register unsigned long old = ptr & 0xFFFF;
  memcpy(((struct PNData *)ptr)->data, fj, 16);

  count = potion_mark_stack(M, 1);
  CuAssert(T, "copied location identical to original", (old & 0xFFFF) != ptr);
  CuAssertIntEquals(T, "copied object not still PN_TUSER", ((struct PNData *)ptr)->vt, PN_TUSER);
  CuAssert(T, "copied data not identical to original",
    strncmp(((struct PNData *)ptr)->data, fj, 16) == 0);
}

CuSuite *gc_suite() {
  CuSuite *S = CuSuiteNew();
  SUITE_ADD_TEST(S, gc_test_start);
  SUITE_ADD_TEST(S, gc_test_alloc1);
  SUITE_ADD_TEST(S, gc_test_alloc4);
  SUITE_ADD_TEST(S, gc_test_forward);
  return S;
}

int main(void) {
  POTION_INIT_STACK(sp);
  int count;

  // manually initialize the older generation
  M = potion_gc_boot(sp)->mem;
  if (M->old_lo == NULL) {
    int gensz = POTION_BIRTH_SIZE * 2;
    void *page = pngc_page_new(&gensz, 0);
    SET_GEN(old, page, gensz);
  }

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
