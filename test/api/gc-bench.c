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

static int bench1_actual = 0;
static int bench2_actual = 0;

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

PN gc_populate_tree(PN node, int depth) {
  if (depth <= 0)
    return;

  depth--;
  PN_TUPLE_AT(node, 0) = potion_tuple_with_size(P, 2);
  potion_gc_update(P->mem, node);
#ifdef HOLES
  n = potion_tuple_with_size(P, 2);
#endif
  PN_TUPLE_AT(node, 1) = potion_tuple_with_size(P, 2);
  potion_gc_update(P->mem, node);
#ifdef HOLES
  n = potion_tuple_with_size(P, 2);
#endif
  gc_populate_tree(PN_TUPLE_AT(node, 0), depth);
  gc_populate_tree(PN_TUPLE_AT(node, 1), depth);
}

int gc_tree_depth(PN node, int side, int depth) {
  PN n = PN_TUPLE_AT(node, side);
  if (n == PN_NIL) return depth;
  return gc_tree_depth(n, side, depth + 1);
}

void gc_temp_bench(CuTest *T) {
  PN temp = gc_make_tree(tree_stretch);
  CuAssert(T, "no temp tree created", temp != PN_NIL);
  CuAssertIntEquals(T, "temp tree has incorrect leftmost depth", 20, gc_tree_depth(temp, 0, 0));
  CuAssertIntEquals(T, "temp tree has incorrect rightmost depth", 20, gc_tree_depth(temp, 1, 0));
  temp = 0;
  bench1_actual = potion_gc_actual(P, PN_NIL, PN_NIL);
}

void gc_long_bench(CuTest *T) {
  PN long_lived = potion_tuple_with_size(P, 2);
  gc_populate_tree(long_lived, tree_long_lived);
  CuAssert(T, "no long-lived tree created", long_lived != PN_NIL);
  CuAssertIntEquals(T, "long-lived tree has incorrect leftmost depth",
    18, gc_tree_depth(long_lived, 0, 0));
  CuAssertIntEquals(T, "long-lived tree has incorrect rightmost depth",
    18, gc_tree_depth(long_lived, 1, 0));
  bench2_actual = potion_gc_actual(P, PN_NIL, PN_NIL);
}

void gc_check_free(CuTest *T) {
  CuAssert(T, "memory from temp tree was not released", bench1_actual > bench2_actual);
}

CuSuite *gc_suite() {
  CuSuite *S = CuSuiteNew();
  SUITE_ADD_TEST(S, gc_temp_bench);
  SUITE_ADD_TEST(S, gc_long_bench);
  SUITE_ADD_TEST(S, gc_check_free);
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
