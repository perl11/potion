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

#define info(x, ...)

PN_SIZE potion_stack_len(Potion *P, _PN **p) {
  _PN *esp, *c = P->mem->cstack;
  POTION_ESP(&esp);
  if (p) *p = STACK_UPPER(c, esp);
  return esp < c ? c - esp : esp - c + 1;
}

#define HAS_REAL_TYPE(v) (P->vts == NULL || (((struct PNFwd *)v)->fwd == POTION_COPIED || PN_TYPECHECK(PN_VTYPE(v))))

static PN_SIZE pngc_mark_array(Potion *P, register _PN *x, register long n, int forward) {
  _PN v;
  PN_SIZE i = 0;
  struct PNMemory *M = P->mem;

  while (n--) {
    v = *x;
    if (IS_GC_PROTECTED(v) || IN_BIRTH_REGION(v) || IN_OLDER_REGION(v)) {
      v = potion_fwd(v);
      switch (forward) {
        case 0: // count only
          if (!IS_GC_PROTECTED(v) && IN_BIRTH_REGION(v) && HAS_REAL_TYPE(v))
            i++;
        break;
        case 1: // minor
          if (!IS_GC_PROTECTED(v) && IN_BIRTH_REGION(v) && HAS_REAL_TYPE(v)) {
            GC_FORWARD(x, v);
            i++;
          }
        break;
        case 2: // major
          if (!IS_GC_PROTECTED(v) && (IN_BIRTH_REGION(v) || IN_OLDER_REGION(v)) && HAS_REAL_TYPE(v)) {
            GC_FORWARD(x, v);
            i++;
          }
        break;
      }
    }
    x++;
  }
  return i;
}

PN_SIZE potion_mark_stack(Potion *P, int forward) {
  PN_SIZE n;
  struct PNMemory *M = P->mem;
  _PN *end, *start = M->cstack;
  POTION_ESP(&end);
#if POTION_STACK_DIR > 0
  n = end - start;
#else
  n = start - end + 1;
  start = end;
  end = M->cstack;
#endif
  if (n <= 0) return 0;
  return pngc_mark_array(P, start, n, forward);
}

void *pngc_page_new(int *sz, const char exec) {
  *sz = PN_ALIGN(*sz, POTION_PAGESIZE);
  return potion_mmap(*sz, exec);
}

void pngc_page_delete(void *mem, int sz) {
  potion_munmap(mem, PN_ALIGN(sz, POTION_PAGESIZE));
}

static inline int NEW_BIRTH_REGION(struct PNMemory *M, void **wb, int sz) {
  int keeps = wb - (void **)M->birth_storeptr;
  void *newad = pngc_page_new(&sz, 0);
  wb = (void *)(((void **)(newad + sz)) - (keeps + 4));
  PN_MEMCPY_N(wb + 1, M->birth_storeptr + 1, void *, keeps);
  DEL_BIRTH_REGION();
  SET_GEN(birth, newad, sz);
  SET_STOREPTR(5 + keeps);
}

//
// Both this function and potion_gc_major embody a simple
// Cheney loop (also called a "two-finger collector.")
// http://en.wikipedia.org/wiki/Cheney%27s_algorithm
// (Again, many thanks to Basile Starynkevitch for
// his tutelage in this matter.)
//
static int potion_gc_minor(Potion *P, int sz) {
  struct PNMemory *M = P->mem;
  void *scanptr = 0;
  void **storead = 0, **wb = 0;

  if (sz < 0)
    sz = 0;
  else if (sz >= POTION_MAX_BIRTH_SIZE)
    return POTION_NO_MEM;

  scanptr = (void *) M->old_cur;
  info("running gc_minor\n"
    "(young: %p -> %p = %ld)\n"
    "(old: %p -> %p = %ld)\n"
    "(storeptr len = %ld)\n",
    M->birth_lo, M->birth_hi, (long)(M->birth_hi - M->birth_lo),
    M->old_lo, M->old_hi, (long)(M->old_hi - M->old_lo),
    (long)((void *)M->birth_hi - (void *)M->birth_storeptr));
  potion_mark_stack(P, 1);

  GC_MINOR_STRINGS();

  wb = (void **)M->birth_storeptr;
  for (storead = wb; storead < (void **)M->birth_hi; storead++) {
    PN v = (PN)*storead;
    if (PN_IS_PTR(v))
      potion_mark_minor(P, (const struct PNObject *)v);
  }
  storead = 0;

  while ((PN)scanptr < (PN)M->old_cur)
    scanptr = potion_mark_minor(P, scanptr);
  scanptr = 0;

  sz += 2 * POTION_PAGESIZE;
  sz = max(sz, potion_birth_suggest(sz, M->old_lo, M->old_cur));

  sz = NEW_BIRTH_REGION(M, wb, sz);
  M->minors++;

  info("(new young: %p -> %p = %d)\n", M->birth_lo, M->birth_hi, (long)(M->birth_hi - M->birth_lo));
  return POTION_OK;
}

static int potion_gc_major(Potion *P, int siz) {
  struct PNMemory *M = P->mem;
  void *prevoldlo = 0;
  void *prevoldhi = 0;
  void *prevoldcur = 0;
  void *newold = 0;
  void *protptr = (void *)M + PN_ALIGN(sizeof(struct PNMemory), 8);
  void *scanptr = 0;
  void **wb = 0;
  int birthest = 0;
  int birthsiz = 0;
  int newoldsiz = 0;
  int oldsiz = 0;

  if (siz < 0)
    siz = 0;
  else if (siz >= POTION_MAX_BIRTH_SIZE)
    return POTION_NO_MEM;

  prevoldlo = (void *)M->old_lo;
  prevoldhi = (void *)M->old_hi;
  prevoldcur = (void *)M->old_cur;

  info("running gc_major\n"
    "(young: %p -> %p = %ld)\n"
    "(old: %p -> %p = %ld)\n",
    M->birth_lo, M->birth_hi, (long)(M->birth_hi - M->birth_lo),
    M->old_lo, M->old_hi, (long)(M->old_hi - M->old_lo));
  birthest = potion_birth_suggest(siz, prevoldlo, prevoldcur);
  newoldsiz = (((char *)prevoldcur - (char *)prevoldlo) + siz + birthest +
    POTION_GC_THRESHOLD + 16 * POTION_PAGESIZE) + ((char *)M->birth_cur - (char *)M->birth_lo);
  newold = pngc_page_new(&newoldsiz, 0);
  M->old_cur = scanptr = newold + (sizeof(PN) * 2);
  info("(new old: %p -> %p = %d)\n", newold, (char *)newold + newoldsiz, newoldsiz);

  potion_mark_stack(P, 2);

  wb = (void **)M->birth_storeptr;
  if (M->birth_lo != M) {
    while ((PN)protptr < (PN)M->protect)
      protptr = potion_mark_major(P, protptr);
  }

  while ((PN)scanptr < (PN)M->old_cur)
    scanptr = potion_mark_major(P, scanptr);
  scanptr = 0;

  GC_MAJOR_STRINGS();

  pngc_page_delete((void *)prevoldlo, (char *)prevoldhi - (char *)prevoldlo);
  prevoldlo = 0;
  prevoldhi = 0;
  prevoldcur = 0;

  birthsiz = NEW_BIRTH_REGION(M, wb, siz + birthest);
  oldsiz = ((char *)M->old_cur - (char *)newold) +
    (birthsiz + 2 * birthest + 4 * POTION_PAGESIZE);
  oldsiz = PN_ALIGN(oldsiz, POTION_PAGESIZE);
  if (oldsiz < newoldsiz) {
    pngc_page_delete((void *)newold + oldsiz, newoldsiz - oldsiz);
    newoldsiz = oldsiz;
  }

  M->old_lo = newold;
  M->old_hi = (char *)newold + newoldsiz;
  M->majors++;

  newold = 0;

  return POTION_OK;
}

void potion_garbagecollect(Potion *P, int sz, int full) {
  struct PNMemory *M = P->mem;
  if (M->collecting) return;
  M->pass++;
  M->collecting = 1;

  if (M->old_lo == NULL) {
    int gensz = POTION_MIN_BIRTH_SIZE * 4;
    if (gensz < sz * 4)
      gensz = min(POTION_MAX_BIRTH_SIZE, PN_ALIGN(sz * 4, POTION_PAGESIZE));
    void *page = pngc_page_new(&gensz, 0);
    SET_GEN(old, page, gensz);
    full = 0;
  } else if ((char *) M->old_cur + sz + potion_birth_suggest(sz, M->old_lo, M->old_cur) +
      ((char *) M->birth_hi - (char *) M->birth_lo) > (char *) M->old_hi)
    full = 1;
#if POTION_GC_PERIOD>0
  else if (M->pass % POTION_GC_PERIOD == POTION_GC_PERIOD)
    full = 1;
#endif

  if (full)
    potion_gc_major(P, sz);
  else
    potion_gc_minor(P, sz);

  M->dirty = 0;
  M->collecting = 0;
}

PN_SIZE potion_type_size(Potion *P, const struct PNObject *ptr) {
  int sz = 0;

  switch (((struct PNFwd *)ptr)->fwd) {
    case POTION_COPIED:
    case POTION_FWD:
      sz = ((struct PNFwd *)ptr)->siz;
      goto done;
  }

  if (ptr->vt > PN_TUSER) {
    sz = sizeof(struct PNObject) +
      (((struct PNVtable *)PN_VTABLE(ptr->vt))->ivlen * sizeof(PN));
    goto done;
  }

  switch (ptr->vt) {
    case PN_TNUMBER:
      sz = sizeof(struct PNDecimal);
    break;
    case PN_TSTRING:
      sz = sizeof(struct PNString) + PN_STR_LEN(ptr) + 1;
    break;
    case PN_TCLOSURE:
      sz = sizeof(struct PNClosure) + (PN_CLOSURE(ptr)->extra * sizeof(PN));
    break;
    case PN_TTUPLE:
      sz = sizeof(struct PNTuple) + (sizeof(PN) * ((struct PNTuple *)ptr)->len);
    break;
    case PN_TSTATE:
      sz = sizeof(Potion);
    break;
    case PN_TFILE:
      sz = sizeof(struct PNFile);
    break;
    case PN_TVTABLE:
      sz = sizeof(struct PNVtable);
    break;
    case PN_TSOURCE:
    // TODO: look up ast size (see core/ast.c)
      sz = sizeof(struct PNSource) + (3 * sizeof(PN));
    break;
    case PN_TBYTES:
      sz = sizeof(struct PNBytes) + ((struct PNBytes *)ptr)->siz;
    break;
    case PN_TPROTO:
      sz = sizeof(struct PNProto);
    break;
    case PN_TTABLE:
      sz = sizeof(struct PNTable) + kh_mem(PN, ptr);
    break;
    case PN_TSTRINGS:
      sz = sizeof(struct PNTable) + kh_mem(str, ptr);
    break;
    case PN_TFLEX:
      sz = sizeof(PNFlex) + ((PNFlex *)ptr)->siz;
    break;
    case PN_TCONT:
      sz = sizeof(struct PNCont) + (((struct PNCont *)ptr)->len * sizeof(PN));
    break;
    case PN_TUSER:
      sz = sizeof(struct PNData) + ((struct PNData *)ptr)->siz;
    break;
  }

done:
  if (sz < sizeof(struct PNFwd))
    sz = sizeof(struct PNFwd);
  return PN_ALIGN(sz, 8); // force 64-bit alignment
}

void *potion_gc_copy(Potion *P, struct PNObject *ptr) {
  void *dst = (void *)P->mem->old_cur;
  PN_SIZE sz = potion_type_size(P, (const struct PNObject *)ptr);
  memcpy(dst, ptr, sz);
  P->mem->old_cur = (char *)dst + sz;

  ((struct PNFwd *)ptr)->fwd = POTION_COPIED;
  ((struct PNFwd *)ptr)->siz = sz;
  ((struct PNFwd *)ptr)->ptr = (PN)dst;

  return dst;
}

void *potion_mark_minor(Potion *P, const struct PNObject *ptr) {
  struct PNMemory *M = P->mem;
  PN_SIZE i;
  PN_SIZE sz = 16;

  switch (((struct PNFwd *)ptr)->fwd) {
    case POTION_COPIED:
    case POTION_FWD:
      GC_MINOR_UPDATE(((struct PNFwd *)ptr)->ptr);
    goto done;
  }

  if (ptr->vt > PN_TUSER) {
    GC_MINOR_UPDATE(PN_VTABLE(ptr->vt));
    int ivars = ((struct PNVtable *)PN_VTABLE(ptr->vt))->ivlen;
    for (i = 0; i < ivars; i++)
      GC_MINOR_UPDATE(((struct PNObject *)ptr)->ivars[i]);
    goto done;
  }

  switch (ptr->vt) {
    case PN_TWEAK:
      GC_MINOR_UPDATE(((struct PNWeakRef *)ptr)->data);
    break;
    case PN_TCLOSURE:
      GC_MINOR_UPDATE(((struct PNClosure *)ptr)->sig);
      for (i = 0; i < ((struct PNClosure *)ptr)->extra; i++)
        GC_MINOR_UPDATE(((struct PNClosure *)ptr)->data[i]);
    break;
    case PN_TTUPLE: {
      struct PNTuple * volatile t = (struct PNTuple *)potion_fwd((PN)ptr);
      for (i = 0; i < t->len; i++)
        GC_MINOR_UPDATE(t->set[i]);
    }
    break;
    case PN_TSTATE:
      GC_MINOR_UPDATE(((Potion *)ptr)->strings);
      GC_MINOR_UPDATE(((Potion *)ptr)->lobby);
      GC_MINOR_UPDATE(((Potion *)ptr)->vts);
      GC_MINOR_UPDATE(((Potion *)ptr)->source);
      GC_MINOR_UPDATE(((Potion *)ptr)->input);
      GC_MINOR_UPDATE(((Potion *)ptr)->pbuf);
      GC_MINOR_UPDATE(((Potion *)ptr)->unclosed);
      GC_MINOR_UPDATE(((Potion *)ptr)->call);
      GC_MINOR_UPDATE(((Potion *)ptr)->callset);
    break;
    case PN_TFILE:
      GC_MINOR_UPDATE(((struct PNFile *)ptr)->path);
    break;
    case PN_TVTABLE:
      GC_MINOR_UPDATE(((struct PNVtable *)ptr)->parent);
      GC_MINOR_UPDATE(((struct PNVtable *)ptr)->ivars);
      GC_MINOR_UPDATE(((struct PNVtable *)ptr)->methods);
      GC_MINOR_UPDATE(((struct PNVtable *)ptr)->ctor);
      GC_MINOR_UPDATE(((struct PNVtable *)ptr)->call);
      GC_MINOR_UPDATE(((struct PNVtable *)ptr)->callset);
    break;
    case PN_TSOURCE:
      GC_MINOR_UPDATE(((struct PNSource *)ptr)->a[0]);
      GC_MINOR_UPDATE(((struct PNSource *)ptr)->a[1]);
      GC_MINOR_UPDATE(((struct PNSource *)ptr)->a[2]);
    break;
    case PN_TPROTO:
      GC_MINOR_UPDATE(((struct PNProto *)ptr)->source);
      GC_MINOR_UPDATE(((struct PNProto *)ptr)->sig);
      GC_MINOR_UPDATE(((struct PNProto *)ptr)->stack);
      GC_MINOR_UPDATE(((struct PNProto *)ptr)->paths);
      GC_MINOR_UPDATE(((struct PNProto *)ptr)->locals);
      GC_MINOR_UPDATE(((struct PNProto *)ptr)->upvals);
      GC_MINOR_UPDATE(((struct PNProto *)ptr)->values);
      GC_MINOR_UPDATE(((struct PNProto *)ptr)->protos);
      GC_MINOR_UPDATE(((struct PNProto *)ptr)->tree);
      GC_MINOR_UPDATE(((struct PNProto *)ptr)->asmb);
    break;
    case PN_TTABLE:
      GC_MINOR_UPDATE_TABLE(PN, (struct PNTable *)potion_fwd((PN)ptr), 1);
    break;
    case PN_TFLEX:
      for (i = 0; i < PN_FLEX_SIZE(ptr); i++)
        GC_MINOR_UPDATE(PN_FLEX_AT(ptr, i));
    break;
    case PN_TCONT:
      GC_KEEP(ptr);
      pngc_mark_array(P, (_PN *)((struct PNCont *)ptr)->stack + 3, ((struct PNCont *)ptr)->len - 3, 1);
    break;
  }

done:
  sz = potion_type_size(P, ptr);
  return (void *)((char *)ptr + sz);
}

void *potion_mark_major(Potion *P, const struct PNObject *ptr) {
  struct PNMemory *M = P->mem;
  PN_SIZE i;
  PN_SIZE sz = 16;

  switch (((struct PNFwd *)ptr)->fwd) {
    case POTION_COPIED:
    case POTION_FWD:
      GC_MAJOR_UPDATE(((struct PNFwd *)ptr)->ptr);
    goto done;
  }

  if (ptr->vt > PN_TUSER) {
    GC_MAJOR_UPDATE(PN_VTABLE(ptr->vt));
    int ivars = ((struct PNVtable *)PN_VTABLE(ptr->vt))->ivlen;
    for (i = 0; i < ivars; i++)
      GC_MAJOR_UPDATE(((struct PNObject *)ptr)->ivars[i]);
    goto done;
  }

  switch (ptr->vt) {
    case PN_TWEAK:
      GC_MAJOR_UPDATE(((struct PNWeakRef *)ptr)->data);
    break;
    case PN_TCLOSURE:
      GC_MAJOR_UPDATE(((struct PNClosure *)ptr)->sig);
      for (i = 0; i < ((struct PNClosure *)ptr)->extra; i++)
        GC_MAJOR_UPDATE(((struct PNClosure *)ptr)->data[i]);
    break;
    case PN_TTUPLE: {
      struct PNTuple * volatile t = (struct PNTuple *)potion_fwd((PN)ptr);
      for (i = 0; i < t->len; i++)
        GC_MAJOR_UPDATE(t->set[i]);
    }
    break;
    case PN_TSTATE:
      GC_MAJOR_UPDATE(((Potion *)ptr)->strings);
      GC_MAJOR_UPDATE(((Potion *)ptr)->lobby);
      GC_MAJOR_UPDATE(((Potion *)ptr)->vts);
      GC_MAJOR_UPDATE(((Potion *)ptr)->source);
      GC_MAJOR_UPDATE(((Potion *)ptr)->input);
      GC_MAJOR_UPDATE(((Potion *)ptr)->pbuf);
      GC_MAJOR_UPDATE(((Potion *)ptr)->unclosed);
      GC_MAJOR_UPDATE(((Potion *)ptr)->call);
      GC_MAJOR_UPDATE(((Potion *)ptr)->callset);
    break;
    case PN_TFILE:
      GC_MAJOR_UPDATE(((struct PNFile *)ptr)->path);
    break;
    case PN_TVTABLE:
      GC_MAJOR_UPDATE(((struct PNVtable *)ptr)->parent);
      GC_MAJOR_UPDATE(((struct PNVtable *)ptr)->ivars);
      GC_MAJOR_UPDATE(((struct PNVtable *)ptr)->methods);
      GC_MAJOR_UPDATE(((struct PNVtable *)ptr)->ctor);
      GC_MAJOR_UPDATE(((struct PNVtable *)ptr)->call);
      GC_MAJOR_UPDATE(((struct PNVtable *)ptr)->callset);
    break;
    case PN_TSOURCE:
      GC_MAJOR_UPDATE(((struct PNSource *)ptr)->a[0]);
      GC_MAJOR_UPDATE(((struct PNSource *)ptr)->a[1]);
      GC_MAJOR_UPDATE(((struct PNSource *)ptr)->a[2]);
    break;
    case PN_TPROTO:
      GC_MAJOR_UPDATE(((struct PNProto *)ptr)->source);
      GC_MAJOR_UPDATE(((struct PNProto *)ptr)->sig);
      GC_MAJOR_UPDATE(((struct PNProto *)ptr)->stack);
      GC_MAJOR_UPDATE(((struct PNProto *)ptr)->paths);
      GC_MAJOR_UPDATE(((struct PNProto *)ptr)->locals);
      GC_MAJOR_UPDATE(((struct PNProto *)ptr)->upvals);
      GC_MAJOR_UPDATE(((struct PNProto *)ptr)->values);
      GC_MAJOR_UPDATE(((struct PNProto *)ptr)->protos);
      GC_MAJOR_UPDATE(((struct PNProto *)ptr)->tree);
      GC_MAJOR_UPDATE(((struct PNProto *)ptr)->asmb);
    break;
    case PN_TTABLE:
      GC_MAJOR_UPDATE_TABLE(PN, (struct PNTable *)potion_fwd((PN)ptr), 1);
    break;
    case PN_TFLEX:
      for (i = 0; i < PN_FLEX_SIZE(ptr); i++)
        GC_MAJOR_UPDATE(PN_FLEX_AT(ptr, i));
    break;
    case PN_TCONT:
      GC_KEEP(ptr);
      pngc_mark_array(P, (_PN *)((struct PNCont *)ptr)->stack + 3, ((struct PNCont *)ptr)->len - 3, 2);
    break;
  }

done:
  sz = potion_type_size(P, ptr);
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
  int bootsz = POTION_MIN_BIRTH_SIZE;
  void *page1 = pngc_page_new(&bootsz, 0);
  struct PNMemory *M = (struct PNMemory *)page1;
  PN_MEMZERO(M, struct PNMemory);

  SET_GEN(birth, page1, bootsz);
  SET_STOREPTR(4);

  M->cstack = sp;
  P = (Potion *)((char *)M + PN_ALIGN(sizeof(struct PNMemory), 8));
  PN_MEMZERO(P, Potion);
  P->mem = M;

  M->birth_cur = (void *)((char *)P + PN_ALIGN(sizeof(Potion), 8));
  GC_PROTECT(P);
  return P;
}

// TODO: release memory allocated by the user
void potion_gc_release(Potion *P) {
  struct PNMemory *M = P->mem;
  void *birthlo = (void *)M->birth_lo;
  void *birthhi = (void *)M->birth_hi;
  void *oldlo = (void *)M->old_lo;
  void *oldhi = (void *)M->old_hi;

  if (M->birth_lo != M) {
    void *protend = (void *)PN_ALIGN((_PN)M->protect, POTION_PAGESIZE);
    pngc_page_delete((void *)M, (char *)protend - (char *)M);
  }

  pngc_page_delete(birthlo, birthhi - birthlo);
  if (oldlo != NULL)
    pngc_page_delete(oldlo, oldhi - oldlo);

  birthlo = 0;
  birthhi = 0;
  oldlo = 0;
  oldhi = 0;
}

PN potion_gc_actual(Potion *P, PN cl, PN self)
{
  int total = (char *)P->mem->birth_cur - (char *)P->mem->birth_lo;
  if (P->mem != P->mem->birth_lo)
    total += (char *)P->mem->protect - (char *)P->mem;
  if (P->mem->old_lo != NULL)
    total += (char *)P->mem->old_cur - (char *)P->mem->old_lo;
  return PN_NUM(total);
}

PN potion_gc_fixed(Potion *P, PN cl, PN self)
{
  int total = 0;
  if (P->mem->protect != NULL)
    total += (char *)P->mem->protect - (char *)P->mem;
  return PN_NUM(total);
}

PN potion_gc_reserved(Potion *P, PN cl, PN self)
{
  int total = (char *)P->mem->birth_hi - (char *)P->mem->birth_lo;
  if (P->mem != P->mem->birth_lo)
    total += (char *)P->mem->protect - (char *)P->mem;
  if (P->mem->old_lo != NULL)
    total += (char *)P->mem->old_hi - (char *)P->mem->old_lo;
  return PN_NUM(total);
}
