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
#include "khash.h"
#include "table.h"

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
      switch (forward) {
        case 1:
          GC_FORWARD(x);
        break;
        case 2:
          GC_FOLLOW_FORWARD(v);
          if (IN_BIRTH_REGION(v) || IN_OLDER_REGION(v))
            GC_FORWARD(x);
        break;
      }
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
  potion_munmap(mem, sz);
}

//
// Both this function and potion_gc_major embody a simple
// Cheney loop (also called a "two-finger collector.")
// http://en.wikipedia.org/wiki/Cheney%27s_algorithm
// (Again, many thanks to Basile Starynkevitch for
// his tutelage in this matter.)
//
static int potion_gc_minor(struct PNMemory *M, int sz) {
  void *scanptr = (void *) M->old_cur;
  void *newad = 0;
  void **storead = 0;

  if (sz < 0)
    sz = 0;
  else if (sz >= POTION_MAX_BIRTH_SIZE)
    return POTION_NO_MEM;

  potion_mark_stack(M, 1);

  for (storead = ((void **)M->birth_storeptr) - 1;
       storead < (void **)M->birth_hi; storead++) {
    PN v = (PN)*storead;
    if (PN_IS_PTR(v))
      potion_mark_minor(M, (const struct PNObject *)v);
  }

  while ((PN)scanptr < (PN)M->old_cur) {
    scanptr = potion_mark_minor(M, scanptr);
    while ((PN)scanptr < (PN)M->old_cur && (*(void **)scanptr) == 0)
      scanptr = ((void **)scanptr) + 1;
  }

  sz += 2 * POTION_PAGESIZE;
  sz = max(sz, POTION_BIRTH_SIZE);

  newad = pngc_page_new(&sz, 0);
  pngc_page_delete((void *)M->birth_lo,
		(char *)M->birth_hi - (char *)M->birth_lo);
  SET_GEN(birth, newad, sz);
  SET_STOREPTR(5);
  M->minors++;

  return POTION_OK;
}

static int potion_gc_major(struct PNMemory *M, int siz) {
  void *prevoldlo = (void *)M->old_lo;
  void *prevoldhi = (void *)M->old_hi;
  void *prevoldcur = (void *)M->old_cur;
  void *newold = 0;
  void *scanptr = 0;
  void *newbirth = 0;
  int birthsiz = 0;
  int newoldsiz = 0;
  int oldsiz = 0;

  if (siz < 0)
    siz = 0;
  else if (siz >= POTION_BIRTH_SIZE)
    return POTION_NO_MEM;

  newoldsiz = (((char *)prevoldcur - (char *)prevoldlo) + siz + POTION_BIRTH_SIZE +
    POTION_GC_THRESHOLD + 16 * POTION_PAGESIZE) + ((char *)M->birth_cur - (char *)M->birth_lo);
  M->old_cur = scanptr = newold = pngc_page_new(&newoldsiz, 0);

  potion_mark_stack(M, 2);

  while ((PN)scanptr < (PN)M->old_cur) {
    scanptr = potion_mark_major(M, scanptr);
    while ((PN)scanptr < (PN)M->old_cur && (*(void **)scanptr) == 0)
      scanptr = ((void **)scanptr) + 1;
  }

  pngc_page_delete((void *)prevoldlo, (char *)prevoldhi - (char *)prevoldlo);

  birthsiz = siz + POTION_BIRTH_SIZE;
  newbirth = pngc_page_new(&birthsiz, 0);
  
  pngc_page_delete((void *)M->birth_lo, (char *)M->birth_hi - (char *)M->birth_lo);
  SET_GEN(birth, newbirth, birthsiz);
  SET_STOREPTR(5);

  oldsiz = ((char *)M->old_cur - (char *)newold) +
    (birthsiz + 2 * POTION_BIRTH_SIZE + 4 * POTION_PAGESIZE);
  oldsiz = PN_ALIGN(oldsiz, POTION_PAGESIZE);
  if (oldsiz < newoldsiz) {
    pngc_page_delete((void *)newold + oldsiz, newoldsiz - oldsiz);
    newoldsiz = oldsiz;
  }

  M->old_lo = newold;
  M->old_hi = (char *)newold + newoldsiz;
  M->majors++;

  return POTION_OK;
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
    potion_gc_major(M, sz);
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
      sz = sizeof(struct PNObject) + sizeof(kh__PN_t *);
    break;
    case PN_TUSER:
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

void *potion_mark_minor(struct PNMemory *M, const struct PNObject *ptr) {
  PN_SIZE i;
  PN_SIZE sz = 16;

  switch (ptr->vt) {
    case PN_TSTRING:
      sz = sizeof(struct PNString) + PN_STR_LEN(ptr) + 1;
    break;
    case PN_TWEAK:
      GC_MINOR_UPDATE(((struct PNWeakRef *)ptr)->data);
      sz = sizeof(struct PNWeakRef);
    break;
    case PN_TCLOSURE:
      GC_MINOR_UPDATE(((struct PNClosure *)ptr)->sig);
      for (i = 0; i < ((struct PNClosure *)ptr)->extra; i++)
        GC_MINOR_UPDATE(((struct PNClosure *)ptr)->data[i]);
      sz = sizeof(struct PNClosure) + (PN_CLOSURE(ptr)->extra * sizeof(PN));
    break;
    case PN_TTUPLE:
      for (i = 0; i < ((struct PNTuple *)ptr)->len; i++)
        GC_MINOR_UPDATE(((struct PNTuple *)ptr)->set[i]);
      sz = sizeof(struct PNTuple);
    break;
    case PN_TFILE:
      GC_MINOR_UPDATE(((struct PNFile *)ptr)->path);
      sz = sizeof(struct PNFile);
    break;
    // TODO: look up class fields and copy full object length
    case PN_TOBJECT:
      sz = sizeof(struct PNObject);
    break;
    case PN_TSOURCE:
    // TODO: look up ast size (see core/pn-ast.c)
      GC_MINOR_UPDATE(((struct PNSource *)ptr)->a[0]);
      GC_MINOR_UPDATE(((struct PNSource *)ptr)->a[1]);
      GC_MINOR_UPDATE(((struct PNSource *)ptr)->a[2]);
      sz = sizeof(struct PNSource) + (3 * sizeof(PN));
    break;
    case PN_TBYTES:
      sz = sizeof(struct PNBytes) + PN_STR_LEN(ptr) + 1;
    break;
    case PN_TPROTO:
      GC_MINOR_UPDATE(((struct PNProto *)ptr)->source);
      GC_MINOR_UPDATE(((struct PNProto *)ptr)->sig);
      GC_MINOR_UPDATE(((struct PNProto *)ptr)->stack);
      GC_MINOR_UPDATE(((struct PNProto *)ptr)->locals);
      GC_MINOR_UPDATE(((struct PNProto *)ptr)->upvals);
      GC_MINOR_UPDATE(((struct PNProto *)ptr)->values);
      GC_MINOR_UPDATE(((struct PNProto *)ptr)->protos);
      GC_MINOR_UPDATE(((struct PNProto *)ptr)->asmb);
      sz = sizeof(struct PNProto);
    break;
    case PN_TTABLE: {
      unsigned k;
      kh__PN_t *kh = ((struct PNTable *)ptr)->kh;
      for (k = kh_begin(kh); k != kh_end(kh); ++k)
        if (kh_exist(kh, k)) {
          PN v1 = kh_key(kh, k);
          PN v2 = kh_value(kh, k);
          GC_MINOR_UPDATE(v1);
          GC_MINOR_UPDATE(v2);
          if (kh_key(kh, k) != v1) {
            int ret;
            unsigned kn;
            kh_del(_PN, kh, k);
            kn = kh_put(_PN, kh, v1, &ret);
            kh_value(kh, kn) = v2;
            if (kn < k)
              k = kn;
          }
        }
      sz = sizeof(struct PNObject) + sizeof(kh__PN_t *);
    }
    break;
    case PN_TUSER:
      sz = sizeof(struct PNData) + ((struct PNData *)ptr)->len;
    break;
  }

  return (void *)((char *)ptr + sz);
}

void *potion_mark_major(struct PNMemory *M, const struct PNObject *ptr) {
  PN_SIZE i;
  PN_SIZE sz = 16;

  switch (ptr->vt) {
    case PN_TSTRING:
      sz = sizeof(struct PNString) + PN_STR_LEN(ptr) + 1;
    break;
    case PN_TWEAK:
      GC_MAJOR_UPDATE(((struct PNWeakRef *)ptr)->data);
      sz = sizeof(struct PNWeakRef);
    break;
    case PN_TCLOSURE:
      GC_MAJOR_UPDATE(((struct PNClosure *)ptr)->sig);
      for (i = 0; i < ((struct PNClosure *)ptr)->extra; i++)
        GC_MAJOR_UPDATE(((struct PNClosure *)ptr)->data[i]);
      sz = sizeof(struct PNClosure) + (PN_CLOSURE(ptr)->extra * sizeof(PN));
    break;
    case PN_TTUPLE:
      for (i = 0; i < ((struct PNTuple *)ptr)->len; i++)
        GC_MAJOR_UPDATE(((struct PNTuple *)ptr)->set[i]);
      sz = sizeof(struct PNTuple);
    break;
    case PN_TFILE:
      GC_MAJOR_UPDATE(((struct PNFile *)ptr)->path);
      sz = sizeof(struct PNFile);
    break;
    // TODO: look up class fields and copy full object length
    case PN_TOBJECT:
      sz = sizeof(struct PNObject);
    break;
    case PN_TSOURCE:
    // TODO: look up ast size (see core/pn-ast.c)
      GC_MAJOR_UPDATE(((struct PNSource *)ptr)->a[0]);
      GC_MAJOR_UPDATE(((struct PNSource *)ptr)->a[1]);
      GC_MAJOR_UPDATE(((struct PNSource *)ptr)->a[2]);
      sz = sizeof(struct PNSource) + (3 * sizeof(PN));
    break;
    case PN_TBYTES:
      sz = sizeof(struct PNBytes) + PN_STR_LEN(ptr) + 1;
    break;
    case PN_TPROTO:
      GC_MAJOR_UPDATE(((struct PNProto *)ptr)->source);
      GC_MAJOR_UPDATE(((struct PNProto *)ptr)->sig);
      GC_MAJOR_UPDATE(((struct PNProto *)ptr)->stack);
      GC_MAJOR_UPDATE(((struct PNProto *)ptr)->locals);
      GC_MAJOR_UPDATE(((struct PNProto *)ptr)->upvals);
      GC_MAJOR_UPDATE(((struct PNProto *)ptr)->values);
      GC_MAJOR_UPDATE(((struct PNProto *)ptr)->protos);
      GC_MAJOR_UPDATE(((struct PNProto *)ptr)->asmb);
      sz = sizeof(struct PNProto);
    break;
    case PN_TTABLE: {
      unsigned k;
      kh__PN_t *kh = ((struct PNTable *)ptr)->kh;
      for (k = kh_begin(kh); k != kh_end(kh); ++k)
        if (kh_exist(kh, k)) {
          PN v1 = kh_key(kh, k);
          PN v2 = kh_value(kh, k);
          GC_MAJOR_UPDATE(v1);
          GC_MAJOR_UPDATE(v2);
          if (kh_key(kh, k) != v1) {
            int ret;
            unsigned kn;
            kh_del(_PN, kh, k);
            kn = kh_put(_PN, kh, v1, &ret);
            kh_value(kh, kn) = v2;
            if (kn < k)
              k = kn;
          }
        }
      sz = sizeof(struct PNObject) + sizeof(kh__PN_t *);
    }
    break;
    case PN_TUSER:
      sz = sizeof(struct PNData) + ((struct PNData *)ptr)->len;
    break;
  }

  return (void *)((char *)ptr + sz);
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
