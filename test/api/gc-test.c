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

Potion *P;

#if defined(DEBUG)
#define DBG_Gv(P,...)				\
  if (P->flags & (DEBUG_GC | DEBUG_VERBOSE)) {	\
    printf(__VA_ARGS__);			\
  }
#define DBG_G(P,...)	       \
  if (P->flags & DEBUG_GC) {   \
    printf(__VA_ARGS__);       \
  }
#else
#define DBG_Gv(...)
#define DBG_G(...)
#endif

void gc_test_start(CuTest *T) {
  CuAssert(T, "GC struct isn't at start of first page", P->mem == P->mem->birth_lo);
  CuAssert(T, "stack length is not a positive number", potion_stack_len(P, NULL) > 0);
}

//
// everything allocated in alloc1 and alloc4 tests goes out of scope, so will
// not be moved to the old generation. data in the `forward` test will be copied.
//
void gc_test_alloc1(CuTest *T) {
  PN ptr = (PN)potion_gc_alloc(P, PN_TUSER, 16);
  PN_SIZE count = potion_mark_stack(P, 0);
  CuAssert(T, "couldn't allocate 16 bytes from GC", PN_IS_PTR(ptr));
  CuAssert(T, "only one or two allocations should be found", count >= 1 && count <= 2);
}

void gc_test_gc_minor(CuTest *T) {
  potion_str_hash_init(P);
  PN ptr = (PN)potion_gc_alloc(P, PN_TUSER, 16);
  PN s1 = PN_STR("teststring");
  potion_garbagecollect(P, POTION_PAGESIZE, 0);
  CuAssert(T, "couldn't allocate 16 bytes from GC", PN_IS_PTR(ptr));
  CuAssert(T, "s1", PN_IS_STR(s1));
}
void gc_test_gc_major(CuTest *T) {
  POTION_INIT_STACK(sp);
  P = potion_create(sp);
  PN ptr = (PN)potion_gc_alloc(P, PN_TUSER, 16);
  potion_garbagecollect(P, POTION_PAGESIZE, 1);
  CuAssert(T, "couldn't allocate 16 bytes from GC", PN_IS_PTR(ptr));
}

void gc_test_alloc4(CuTest *T) {
  PN ptr = (PN)potion_gc_alloc(P, PN_TUSER, 16);
  PN ptr2 = (PN)potion_gc_alloc(P, PN_TUSER, 16);
  PN ptr3 = (PN)potion_gc_alloc(P, PN_TUSER, 16);
  PN ptr4 = (PN)potion_gc_alloc(P, PN_TUSER, 16);
  PN_SIZE count = potion_mark_stack(P, 0);
  CuAssert(T, "couldn't allocate 16 bytes from GC", PN_IS_PTR(ptr));
  CuAssert(T, "couldn't allocate 16 bytes from GC", PN_IS_PTR(ptr2));
  CuAssert(T, "couldn't allocate 16 bytes from GC", PN_IS_PTR(ptr3));
  CuAssert(T, "couldn't allocate 16 bytes from GC", PN_IS_PTR(ptr4));
  CuAssert(T, "min. four allocations should be found", count >= 4);
}

void gc_test_forward(CuTest *T) {
  char *fj = "frances johnson.";
  vPN(Data) ptr = potion_data_alloc(P, 16);
  register unsigned long old = (PN)ptr & 0xFFFF;
  memcpy(ptr->data, fj, 16);

  //DBG_Gv(P,"forward ptr->data: %p \"%s\"\n", &ptr->data, ptr->data);
  potion_mark_stack(P, 1);
  //DBG_Gv(P,"marked ptr->data: %p \"%s\"\n", &ptr->data, ptr->data);
  CuAssert(T, "copied location identical to original", (old & 0xFFFF) != (PN)ptr);
  CuAssertIntEquals(T, "copied object not still PN_TUSER", ptr->vt, PN_TUSER);
  CuAssert(T, "copied data not identical to original",
    strncmp(ptr->data, fj, 16) == 0);
}

CuSuite *gc_suite() {
  CuSuite *S = CuSuiteNew();
  SUITE_ADD_TEST(S, gc_test_start);
#ifndef __SANITIZE_ADDRESS__
  SUITE_ADD_TEST(S, gc_test_alloc1);
  SUITE_ADD_TEST(S, gc_test_alloc4);
#endif
  SUITE_ADD_TEST(S, gc_test_forward);
  SUITE_ADD_TEST(S, gc_test_gc_minor);
  SUITE_ADD_TEST(S, gc_test_gc_major);
  return S;
}

int main(int argc, char **argv) {
  POTION_INIT_STACK(sp);
  int count;

  // manually initialize the older generation
  P = potion_gc_boot(sp);
  if (argc == 2) {
#ifdef DEBUG
#   define ADD_FLAGS(flag) P->flags = (Potion_Flags)((int)P->flags | (flag))
    if (!strcmp(argv[1], "-DG"))  ADD_FLAGS(DEBUG_GC);
    if (!strcmp(argv[1], "-DGv")) ADD_FLAGS(DEBUG_GC|DEBUG_VERBOSE);
#endif
  }
  if (P->mem->old_lo == NULL) {
    struct PNMemory *M = P->mem;
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
