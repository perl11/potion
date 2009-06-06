//
// gc-bench.c
// benchmarking creation and copying of a b-tree
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

static const int tree_stretch = 20;
static const int tree_long_lived = 18;
static const int array_size = 2000000;
static const int min_tree = 4;
static const int max_tree = 20;

//
// TODO: use a b-tree class rather than tuples
//
PN gc_make_tree(int depth) {
  PN l, r, x;
  if (depth <= 0)
    return potion_tuple_with_size(P, 2);

  l = gc_make_tree(depth - 1);
  r = gc_make_tree(depth - 1);
  x = potion_tuple_with_size(P, 2);
  PN_TUPLE_AT(x, 0) = l;
  PN_TUPLE_AT(x, 1) = r;
  return x;
}

void gc_bench(CuTest *T) {
  PN root, longLived, temp;
  temp = gc_make_tree(tree_stretch);
  temp = 0;
  // CuAssert(T, "stack length is not a positive number", potion_stack_len(M, NULL) > 0);
}

CuSuite *gc_suite() {
  CuSuite *S = CuSuiteNew();
  SUITE_ADD_TEST(S, gc_bench);
  return S;
}

int main(void) {
  POTION_INIT_STACK(sp);
  int count;
  P = potion_create(sp);
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
