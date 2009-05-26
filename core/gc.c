//
// gc.c
// the garbage collector
//
// heavily based on Qish, a lightweight copying generational GC
// by Basile Starynkevitch.
// <http://starynkevitch.net/Basile/qishintro.html>
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include "potion.h"
#include "internal.h"
#include "gc.h"

PN_SIZE potion_stack_len(struct PNMemory *M, _PN **p) {
  _PN *esp, *c = M->cstack;
  POTION_ESP(&esp);
  if (p) *p = STACK_UPPER(c, esp);
  return esp < c ? c - esp : esp - c + 1;
}

static PN_SIZE pngc_mark_array(struct PNMemory *M, register _PN *x, register long n, int forward) {
  _PN v;
  PN_SIZE i = 0;
  while (n--) {
    v = *x;
#if 0
    if ((_PN)M == v)
      printf("pnmemory @ %ld\n", n);
    else if (IS_NEW_PTR(v))
      printf("pointer  @ %ld: %p\n", n, v);
    else
      printf("non-pointer  @ %ld: %p\n", n, v);
#endif
    if ((_PN)M != v && IS_NEW_PTR(v)) {
      if (forward)
        GC_FORWARD(x);
      i++;
    }
    x++;
  }
  return i;
}

PN_SIZE potion_mark_stack(struct PNMemory *M, int forward) {
  long n;
  _PN *end, *start = M->cstack;
  POTION_ESP(&end);
#if POTION_STACK_DIR > 0
  n = end - start;
#else
  n = start - end + 1;
  start = end;
  end = M->cstack;
#endif
  if (n <= 0) return;
  return pngc_mark_array(M, start, n, forward);
}

void *pngc_page_new(int *sz, const char exec) {
  // TODO: why does qish allocate pages before and after?
  *sz = PN_ALIGN(*sz, POTION_PAGESIZE);
  return potion_mmap(*sz, exec);
}

void pngc_page_delete(void *mem, int sz) {
  potion_munmap(mem, PN_ALIGN(sz, POTION_PAGESIZE));
}

static int potion_gc_minor(struct PNMemory *M, int sz) {
  struct PNFrame *f = 0;
  int n = 0;
  void *birthlo = (void *) M->birth_lo;
  void *birthhi = (void *) M->birth_hi;
  void *scanptr = (void *) M->old_cur;
  void *newad = 0;
  void **storead = 0;

  if (sz < 0)
    sz = 0;
  else if (sz >= POTION_MAX_BIRTH_SIZE)
    return POTION_NO_MEM;

  return POTION_OK;
}

static void potion_gc_full(struct PNMemory *M, int sz,
  int nbforw, void **oldforw, void **newforw) {
}

void potion_garbagecollect(struct PNMemory *M, int sz, int full) {
  M->pass++;
  M->collecting = 1;

  // first page is full, allocate new birth area
  // TODO: take sz into account
  // TODO: promote first page to first 2nd gen
  if (M->birth_lo == M) {
    int gensz = POTION_BIRTH_SIZE;
    void *page = pngc_page_new(&gensz, 0);
    SET_GEN(birth, page, gensz);
    SET_STOREPTR(5);
    return;
  }

  if (M->old_lo == NULL) {
    int gensz = POTION_BIRTH_SIZE * 2;
    void *page = pngc_page_new(&gensz, 0);
    SET_GEN(old, page, gensz);
  }

  if ((char *) M->old_cur + sz + POTION_BIRTH_SIZE +
      ((char *) M->birth_hi - (char *) M->birth_lo)
      > (char *) M->old_hi)
    full = 1;
#if POTION_GC_PERIOD>0
  else if (M->pass % POTION_GC_PERIOD == 0)
    full = 1;
#endif

  if (full)
    potion_gc_full(M, sz, 0, 0, 0);
  else
    potion_gc_minor(M, sz);
  M->dirty = 0;
  M->collecting = 0;
}

void *potion_gc_copy(struct PNMemory *M, const struct PNObject *ptr) {
  void *dst = (void *)M->old_cur;
  PN_SIZE sz = 0;

  switch (ptr->vt) {
    case PN_TSTRING:
      sz = sizeof(struct PNString) + PN_STR_LEN(ptr) + 1;
    break;
    case PN_TWEAK:
      sz = sizeof(struct PNWeakRef);
    break;
    case PN_TCLOSURE:
      sz = sizeof(struct PNClosure) + (PN_CLOSURE(ptr)->extra * sizeof(PN));
    break;
    case PN_TTUPLE:
      sz = sizeof(struct PNTuple);
    break;
    case PN_TFILE:
      sz = sizeof(struct PNFile);
    break;
    // TODO: look up class fields and copy full object length
    case PN_TOBJECT:
      sz = sizeof(struct PNObject);
    break;
    case PN_TSOURCE:
    // TODO: look up ast size (see core/pn-ast.c)
      sz = sizeof(struct PNSource) + (3 * sizeof(PN));
    break;
    case PN_TBYTES:
      sz = sizeof(struct PNBytes) + PN_STR_LEN(ptr) + 1;
    break;
    case PN_TPROTO:
      sz = sizeof(struct PNProto);
    break;
    case PN_TTABLE:
      // TODO: include table.h or something
      sz = sizeof(struct PNObject) + sizeof(void *);
    break;
    case PN_TUSER:
      // TODO: include table.h or something
      sz = sizeof(struct PNData) + ((struct PNData *)ptr)->len;
    break;
  }

  if (sz == 0) {
    fprintf(stderr, "** warning: gc asked to copy zero-length object!\n");
    return dst;
  }

  memcpy(dst, ptr, sz);
  sz = PN_ALIGN(sz, 8); // force 64-bit alignment
  M->old_cur = (char *)dst + sz;
  return dst;
}

//
// Potion's GC is a generational copying GC. This is why the
// volatile keyword is used so liberally throughout the source
// code. PN types may suddenly move during any collection phase.
// They move from the birth area to the old area.
//
// Potion actually begins by allocating an old area. This is for
// two reasons. First, the script may be too short to require an
// old area, so we want to avoid allocating two areas to start with.
// And second, since Potion loads its core classes into GC first,
// we save ourselves a severe promotion step by beginning with an
// automatic promotion to second generation. (Oh and this allows
// the core Potion struct pointer to be non-volatile.)
//
// In short, this first page is never released, since the GC struct
// itself is on that page.
//
// While this may pay a slight penalty in memory size for long-running
// scripts, perhaps I could add some occassional compaction to solve
// that as well.
//
Potion *potion_gc_boot(void *sp) {
  Potion *P;
  int bootsz = POTION_BIRTH_SIZE;
  void *page1 = pngc_page_new(&bootsz, 0);
  struct PNMemory *M = (struct PNMemory *)page1;
  PN_MEMZERO(M, struct PNMemory);

  SET_GEN(birth, page1, bootsz);
  SET_STOREPTR(4);

  M->cstack = sp;
  M->birth_cur += PN_ALIGN(sizeof(struct PNMemory), 8);
  P = (Potion *)M->birth_cur;
  PN_MEMZERO(P, Potion);
  P->mem = M;

  M->birth_cur += PN_ALIGN(sizeof(Potion), 8);
  GC_PROTECT(M);
  return P;
}
