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

#define SET_GEN(t, p, s) \
  M->t##_lo = p; \
  M->t##_cur = p + 2 * sizeof(void *); \
  M->t##_hi = p + (s);
#define SET_STOREPTR(n) \
  M->birth_storeptr = (void *)(((void **)M->birth_hi) - n)

static void *pngc_page_new(int sz, const char exec) {
  // TODO: why does qish allocate pages before and after?
  return potion_mmap(PN_ALIGN(sz, POTION_PAGESIZE), exec);
}

static void pngc_page_delete(void *mem, int sz) {
  potion_munmap(mem, PN_ALIGN(sz, POTION_PAGESIZE));
}

static void potion_gc_minor(struct PNMemory *M, int sz) {
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
    void *page = pngc_page_new(POTION_BIRTH_SIZE, 0);
    SET_GEN(birth, page, POTION_BIRTH_SIZE);
    SET_STOREPTR(5);
    return;
  }

  if (M->old_lo == NULL) {
    void *page = pngc_page_new(POTION_BIRTH_SIZE * 2, 0);
    SET_GEN(old, page, POTION_BIRTH_SIZE * 2);
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

Potion *potion_gc_boot() {
  Potion *P;
  void *page1 = pngc_page_new(POTION_BIRTH_SIZE, 0);
  struct PNMemory *M = (struct PNMemory *)page1;
  PN_MEMZERO(M, struct PNMemory);

  SET_GEN(birth, page1, POTION_BIRTH_SIZE);
  SET_STOREPTR(4);

  M->birth_cur += PN_ALIGN(sizeof(struct PNMemory), 8);
  P = (Potion *)M->birth_cur;
  PN_MEMZERO(P, Potion);
  P->mem = M;

  M->birth_cur += PN_ALIGN(sizeof(Potion), 8);
  return P;
}
