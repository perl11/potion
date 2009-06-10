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

PN_SIZE potion_stack_len(struct PNMemory *M, _PN **p) {
  _PN *esp, *c = M->cstack;
  POTION_ESP(&esp);
  if (p) *p = STACK_UPPER(c, esp);
  return esp < c ? c - esp : esp - c + 1;
}

#define HAS_REAL_TYPE(v) (P->vts == NULL || (((struct PNFwd *)(v & PN_REF_MASK))->fwd == POTION_COPIED || PN_TYPECHECK(PN_VTYPE(v & PN_REF_MASK))))

static PN_SIZE pngc_mark_array(struct PNMemory *M, register _PN *x, register long n, int forward) {
  _PN v;
  PN_SIZE i = 0;
  Potion *P = (Potion *)((char *)(M) + PN_ALIGN(sizeof(struct PNMemory), 8));

  while (n--) {
    v = *x;
    if (IS_GC_PROTECTED(v) || IN_BIRTH_REGION(v) || IN_OLDER_REGION(v)) {
      *x = v = potion_fwd(v);
      switch (forward) {
        case 0: // count only
          if (!IS_GC_PROTECTED(v) && IN_BIRTH_REGION(v) && HAS_REAL_TYPE(v))
            i++;
        break;
        case 1: // minor
          if (!IS_GC_PROTECTED(v) && IN_BIRTH_REGION(v) && HAS_REAL_TYPE(v)) {
            GC_FORWARD(x);
            i++;
          }
        break;
        case 2: // major
          if (!IS_GC_PROTECTED(v) && (IN_BIRTH_REGION(v) || IN_OLDER_REGION(v)) && HAS_REAL_TYPE(v)) {
            GC_FORWARD(x);
            i++;
          }
        break;
      }
    }
    x++;
  }
  return i;
}

PN_SIZE potion_mark_stack(struct PNMemory *M, int forward) {
  PN_SIZE n;
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
  return pngc_mark_array(M, start, n, forward);
}

void *pngc_page_new(int *sz, const char exec) {
  *sz = PN_ALIGN(*sz, POTION_PAGESIZE);
  return potion_mmap(*sz, exec);
}

void pngc_page_delete(void *mem, int sz) {
  potion_munmap(mem, PN_ALIGN(sz, POTION_PAGESIZE));
}

//
// Both this function and potion_gc_major embody a simple
// Cheney loop (also called a "two-finger collector.")
// http://en.wikipedia.org/wiki/Cheney%27s_algorithm
// (Again, many thanks to Basile Starynkevitch for
// his tutelage in this matter.)
//
static int potion_gc_minor(struct PNMemory *M, int sz) {
  void *scanptr = 0;
  void *newad = 0;
  void **storead = 0;

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
  potion_mark_stack(M, 1);

  for (storead = ((void **)M->birth_storeptr) - 1;
       storead < (void **)M->birth_hi; storead++) {
    PN v = (PN)*storead;
    if (PN_IS_PTR(v))
      potion_mark_minor(M, (const struct PNObject *)(v & PN_REF_MASK));
  }
  storead = 0;

  while ((PN)scanptr < (PN)M->old_cur)
    scanptr = potion_mark_minor(M, scanptr);
  scanptr = 0;

  sz += 2 * POTION_PAGESIZE;
  sz = max(sz, POTION_BIRTH_SIZE);

  newad = pngc_page_new(&sz, 0);
  DEL_BIRTH_REGION();
  SET_GEN(birth, newad, sz);
  SET_STOREPTR(5);
  M->minors++;

  return POTION_OK;
}

static int potion_gc_major(struct PNMemory *M, int siz) {
  void *prevoldlo = 0;
  void *prevoldhi = 0;
  void *prevoldcur = 0;
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

  prevoldlo = (void *)M->old_lo;
  prevoldhi = (void *)M->old_hi;
  prevoldcur = (void *)M->old_cur;

  info("running gc_major\n"
    "(young: %p -> %p = %ld)\n"
    "(old: %p -> %p = %ld)\n",
    M->birth_lo, M->birth_hi, (long)(M->birth_hi - M->birth_lo),
    M->old_lo, M->old_hi, (long)(M->old_hi - M->old_lo));
  newoldsiz = (((char *)prevoldcur - (char *)prevoldlo) + siz + POTION_BIRTH_SIZE +
    POTION_GC_THRESHOLD + 16 * POTION_PAGESIZE) + ((char *)M->birth_cur - (char *)M->birth_lo);
  newold = pngc_page_new(&newoldsiz, 0);
  M->old_cur = scanptr = newold + (sizeof(PN) * 2);
  info("(new old: %p -> %p = %d)\n", newold, (char *)newold + newoldsiz, newoldsiz);

  potion_mark_stack(M, 2);

  while ((PN)scanptr < (PN)M->old_cur)
    scanptr = potion_mark_major(M, scanptr);
  scanptr = 0;

  pngc_page_delete((void *)prevoldlo, (char *)prevoldhi - (char *)prevoldlo);
  prevoldlo = 0;
  prevoldhi = 0;
  prevoldcur = 0;

  birthsiz = siz + POTION_BIRTH_SIZE;
  newbirth = pngc_page_new(&birthsiz, 0);
  
  DEL_BIRTH_REGION();
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

  newold = 0;

  return POTION_OK;
}

void potion_garbagecollect(struct PNMemory *M, int sz, int full) {
  if (M->collecting) return;
  M->pass++;
  M->collecting = 1;

  if (M->old_lo == NULL) {
    int gensz = POTION_BIRTH_SIZE * 3;
    void *page = pngc_page_new(&gensz, 0);
    SET_GEN(old, page, gensz);
  }

  if ((char *) M->old_cur + sz + POTION_BIRTH_SIZE +
      ((char *) M->birth_hi - (char *) M->birth_lo)
      > (char *) M->old_hi)
    full = 1;
#if POTION_GC_PERIOD>0
  else if (M->pass % POTION_GC_PERIOD == POTION_GC_PERIOD - 1)
    full = 1;
#endif

  if (full)
    potion_gc_major(M, sz);
  else
    potion_gc_minor(M, sz);

  M->dirty = 0;
  M->collecting = 0;
}

PN_SIZE potion_type_size(const struct PNObject *ptr) {
  int sz = 0;

  switch (((struct PNFwd *)ptr)->fwd) {
    case POTION_COPIED:
    case POTION_FWD:
      return ((struct PNFwd *)ptr)->siz;
  }

  switch (ptr->vt) {
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
    // TODO: look up class fields and copy full object length
    // case PN_TOBJECT:
    // return sizeof(struct PNObject);
    case PN_TVTABLE:
      sz = sizeof(struct PNVtable) + sizeof(kh_id_t);
    break;
    case PN_TSOURCE:
    // TODO: look up ast size (see core/pn-ast.c)
      sz = sizeof(struct PNSource) + (3 * sizeof(PN));
    break;
    case PN_TBYTES:
      sz = sizeof(struct PNBytes) + ((struct PNBytes *)ptr)->len + 1;
    break;
    case PN_TPROTO:
      sz = sizeof(struct PNProto);
    break;
    case PN_TTABLE:
      sz = sizeof(struct PNObject) + sizeof(kh__PN_t);
    break;
    case PN_TFLEX:
      sz = sizeof(PNFlex) + ((PNFlex *)ptr)->siz;
    break;
    case PN_TUSER:
      sz = sizeof(struct PNData) + ((struct PNData *)ptr)->siz;
    break;
  }

  if (sz < sizeof(struct PNFwd))
    sz = sizeof(struct PNFwd);
  return PN_ALIGN(sz, 8); // force 64-bit alignment
}

void *potion_gc_copy(struct PNMemory *M, struct PNObject *ptr) {
  void *dst = (void *)M->old_cur;
  PN_SIZE sz = potion_type_size((const struct PNObject *)ptr);
  memcpy(dst, ptr, sz);
  M->old_cur = (char *)dst + sz;

  if (ptr->vt == PN_TWEAK)
    dst = (void *)PN_SET_REF(dst);

  ((struct PNFwd *)ptr)->fwd = POTION_COPIED;
  ((struct PNFwd *)ptr)->siz = sz;
  ((struct PNFwd *)ptr)->ptr = (PN)dst;

  return dst;
}

void *potion_mark_minor(struct PNMemory *M, const struct PNObject *ptr) {
  PN_SIZE i;
  PN_SIZE sz = 16;

  switch (((struct PNFwd *)ptr)->fwd) {
    case POTION_COPIED:
    case POTION_FWD:
      GC_MINOR_UPDATE(((struct PNFwd *)ptr)->ptr);
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
    case PN_TTUPLE:
    {
      struct PNTuple * volatile t = (struct PNTuple *)potion_fwd((PN)ptr);
      for (i = 0; i < t->len; i++)
        GC_MINOR_UPDATE(t->set[i]);
    }
    break;
    case PN_TFILE:
      GC_MINOR_UPDATE(((struct PNFile *)ptr)->path);
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
      GC_MINOR_UPDATE(((struct PNProto *)ptr)->locals);
      GC_MINOR_UPDATE(((struct PNProto *)ptr)->upvals);
      GC_MINOR_UPDATE(((struct PNProto *)ptr)->values);
      GC_MINOR_UPDATE(((struct PNProto *)ptr)->protos);
      GC_MINOR_UPDATE(((struct PNProto *)ptr)->asmb);
    break;
    case PN_TTABLE: {
      unsigned k;
      struct PNTable * volatile t = (struct PNTable *)ptr;
      for (k = kh_begin(t->kh); k != kh_end(t->kh); ++k)
        if (kh_exist(t->kh, k)) {
          PN v1 = kh_key(t->kh, k);
          PN v2 = kh_value(t->kh, k);
          GC_MINOR_UPDATE(v1);
          GC_MINOR_UPDATE(v2);
          if (kh_key(t->kh, k) != v1) {
            int ret;
            unsigned kn;
            kh_del(_PN, t->kh, k);
            kn = kh_put(_PN, t->kh, v1, &ret);
            kh_value(t->kh, kn) = v2;
            if (kn < k)
              k = kn;
          }
        }
    }
    break;
  }

done:
  sz = potion_type_size(ptr);
  return (void *)((char *)ptr + sz);
}

void *potion_mark_major(struct PNMemory *M, const struct PNObject *ptr) {
  PN_SIZE i;
  PN_SIZE sz = 16;

  switch (((struct PNFwd *)ptr)->fwd) {
    case POTION_COPIED:
    case POTION_FWD:
      GC_MAJOR_UPDATE(((struct PNFwd *)ptr)->ptr);
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
    case PN_TTUPLE:
    {
      struct PNTuple * volatile t = (struct PNTuple *)potion_fwd((PN)ptr);
      for (i = 0; i < t->len; i++)
        GC_MAJOR_UPDATE(t->set[i]);
    }
    break;
    case PN_TFILE:
      GC_MAJOR_UPDATE(((struct PNFile *)ptr)->path);
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
      GC_MAJOR_UPDATE(((struct PNProto *)ptr)->locals);
      GC_MAJOR_UPDATE(((struct PNProto *)ptr)->upvals);
      GC_MAJOR_UPDATE(((struct PNProto *)ptr)->values);
      GC_MAJOR_UPDATE(((struct PNProto *)ptr)->protos);
      GC_MAJOR_UPDATE(((struct PNProto *)ptr)->asmb);
    break;
    case PN_TTABLE: {
      unsigned k;
      struct PNTable * volatile t = (struct PNTable *)ptr;
      for (k = kh_begin(t->kh); k != kh_end(t->kh); ++k)
        if (kh_exist(t->kh, k)) {
          PN v1 = kh_key(t->kh, k);
          PN v2 = kh_value(t->kh, k);
          GC_MAJOR_UPDATE(v1);
          GC_MAJOR_UPDATE(v2);
          if (kh_key(t->kh, k) != v1) {
            int ret;
            unsigned kn;
            kh_del(_PN, t->kh, k);
            kn = kh_put(_PN, t->kh, v1, &ret);
            kh_value(t->kh, kn) = v2;
            if (kn < k)
              k = kn;
          }
        }
    }
    break;
  }

done:
  sz = potion_type_size(ptr);
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
  P = (Potion *)((char *)M + PN_ALIGN(sizeof(struct PNMemory), 8));
  PN_MEMZERO(P, Potion);
  P->mem = M;

  M->birth_cur = (void *)((char *)P + PN_ALIGN(sizeof(Potion), 8));
  GC_PROTECT(M);
  return P;
}

// TODO: release memory allocated by the user
void potion_gc_release(struct PNMemory *M) {
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
