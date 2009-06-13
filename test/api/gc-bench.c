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
#include <sys/time.h>
#include <sys/times.h>
#include "potion.h"
#include "internal.h"
#include "gc.h"
#include "khash.h"
#include "table.h"

Potion *P;

static const int tree_stretch = 20;
static const int tree_long_lived = 18;
static const int array_size = 2000000;
static const int min_tree = 4;
static const int max_tree = 20;

static int tree_type = PN_TUSER + 1;

#define ALLOC_NODE() PN_ALLOC_N(tree_type, struct PNObject, 2 * sizeof(PN))

unsigned
current_time(void)
{
  struct timeval t;
  struct timezone tz;

  if (gettimeofday (&t, &tz) == -1)
    return 0;
  return (t.tv_sec * 1000 + t.tv_usec / 1000);
}

//
// TODO: use a b-tree class rather than tuples
//
PN gc_make_tree(int depth) {
  vPN(Object) x;
  PN l, r;
  if (depth <= 0)
    return (PN)ALLOC_NODE();

  l = gc_make_tree(depth - 1);
  r = gc_make_tree(depth - 1);
  x = ALLOC_NODE();
  x->ivars[0] = l;
  x->ivars[1] = r;
  return (PN)x;
}

PN gc_populate_tree(PN node, int depth) {
  if (depth <= 0)
    return;

  depth--;
  ((struct PNObject *)node)->ivars[0] = (PN)ALLOC_NODE();
  potion_gc_update(P, node);
#ifdef HOLES
  n = (PN)ALLOC_NODE();
#endif
  ((struct PNObject *)node)->ivars[1] = (PN)ALLOC_NODE();
  potion_gc_update(P, node);
#ifdef HOLES
  n = (PN)ALLOC_NODE();
#endif
  gc_populate_tree(((struct PNObject *)node)->ivars[0], depth);
  gc_populate_tree(((struct PNObject *)node)->ivars[1], depth);
}

int gc_tree_depth(PN node, int side, int depth) {
  PN n = ((struct PNObject *)node)->ivars[side];
  if (n == PN_NIL) return depth;
  return gc_tree_depth(n, side, depth + 1);
}

int tree_size(int i) {
  return ((1 << (i + 1)) - 1);
}

int main(void) {
  POTION_INIT_STACK(sp);
  PN klass, ary, temp, long_lived;
  int i, j, count;

  P = potion_create(sp);
  klass = potion_type_new(P, tree_type, PN_VTABLE(PN_TOBJECT));
  ((struct PNVtable *)klass)->ivars = 2;
  PN_FLEX_SIZE(P->vts) = PN_TUSER + 2;

  printf("Stretching memory with a binary tree of depth %d\n",
    tree_stretch);
  temp = gc_make_tree(tree_stretch);
  temp = 0;

  printf("Creating a long-lived binary tree of depth %d\n",
    tree_long_lived);
  long_lived = (PN)ALLOC_NODE();
  gc_populate_tree(long_lived, tree_long_lived);

  printf("Creating a long-lived array of %d doubles\n",
    array_size);
  ary = potion_tuple_with_size(P, array_size);
  for (i = 0; i < array_size / 2; ++i)
    PN_TUPLE_AT(ary, i) = PN_NUM(1.0 / i);

  for (i = min_tree; i <= max_tree; i += 2) {
    long start, finish;
    int iter = 2 * tree_size(tree_stretch) / tree_size(i);
    printf ("Creating %d trees of depth %d\n", iter, i);

    start = current_time();
    for (j = 0; j < iter; ++j) {
      temp = (PN)ALLOC_NODE();
      gc_populate_tree(temp, i);
    }
    finish = current_time();
    printf("\tTop down construction took %d msec\n",
      finish - start);

    start = current_time();
    for (j = 0; j < iter; ++j) {
      temp = gc_make_tree(i);
      temp = 0;
    }
    finish = current_time();
    printf("\tBottom up construction took %d msec\n",
      finish - start);
  }
  
  if (long_lived == 0 || PN_TUPLE_AT(ary, 1000) != PN_NUM(1.0 / 1000))
    printf("Wait, problem.\n");

  printf ("Total %d minor and %d full garbage collections\n"
	  "   (min.birth.size=%dK, max.size=%dK, gc.thresh=%dK)\n",
	  P->mem->minors, P->mem->majors,
	  POTION_BIRTH_SIZE >> 10, POTION_MAX_BIRTH_SIZE >> 10,
	  POTION_GC_THRESHOLD >> 10);
  return 0;
}
