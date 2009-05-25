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
// While this may pay a slight penalty in memory size for long-running
// scripts, perhaps I could add some occassional compaction to solve
// that as well.
//
void potion_gc_init(Potion *P)
{
  int oldsiz = 4 * POTION_BIRTH_SIZE;
  int birthsiz = 2 * POTION_BIRTH_SIZE;
  if (P->mem != NULL) return;

  P->mem = SYS_CALLOC(struct PNMemory, 0);

  P->mem->birth_lo = pngc_page_new(birthsiz, 0);
  P->mem->birth_cur = P->mem->birth_lo + 2 * sizeof(void *);
  P->mem->birth_hi = P->mem->birth_lo + birthsiz;
  P->mem->birth_storeptr = (void *) (((void **)P->mem->birth_hi) - 4);

  P->mem->old_lo = pngc_page_new(oldsiz, 0);
  P->mem->old_cur = P->mem->old_lo + 2 * sizeof(void *);
  P->mem->old_hi = P->mem->old_lo + oldsiz;
}
