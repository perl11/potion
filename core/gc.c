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

static void *pngc_page_new(int sz, const char exec)
{
  // TODO: why does qish allocate pages before and after?
  return potion_mmap(PN_ALIGN(sz, POTION_PAGESIZE), exec);
}

static void pngc_page_delete(void *mem, int sz)
{
  potion_munmap(mem, PN_ALIGN(sz, POTION_PAGESIZE));
}

void potion_garbagecollect(int siz, int full)
{
  fprintf(stderr, "** warning: garbage collector full!\n");
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
struct PNMemory *potion_gc_init()
{
  void *page1 = pngc_page_new(POTION_BIRTH_SIZE, 0);
  struct PNMemory *M = (struct PNMemory *)page1;
  PN_MEMZERO(M, struct PNMemory);

  M->birth_lo = page1;
  M->birth_cur = page1 + 2 * sizeof(void *);
  M->birth_hi = page1 + POTION_BIRTH_SIZE;
  M->birth_storeptr = (void *)(((void **)M->birth_hi) - 4);

  M->birth_cur += PN_ALIGN(sizeof(struct PNMemory), 8);
  return M;
}
