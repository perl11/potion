/**\file gc.c
Generational copying garbage collector, non-precise with Cheney loop.

Heavily based on Qish, a lightweight copying generational GC
by Basile Starynkevitch.
http://starynkevitch.net/Basile/qishintro.html

(c) 2008 why the lucky stiff, the freelance professor
*/
#include <stdio.h>
#include <stdlib.h>
#include "potion.h"
#include "internal.h"
#include "gc.h"
#include "khash.h"
#include "table.h"
#include "../tools/greg.h"

#if defined(DEBUG)
//mingw32 has struct timeval in sys/time.h
//#ifdef WIN32
//# include <time.h>
//#else
#include <sys/time.h>
//#endif
static double mytime() {
    struct timeval Tp;
    struct timezone Tz;
    int status;
    status = gettimeofday (&Tp, &Tz);
    if (status == 0) {
        Tp.tv_sec += Tz.tz_minuteswest * 60;	/* adjust for TZ */
        return Tp.tv_sec + (Tp.tv_usec / 1000000.0);
    } else {
        return -1.0;
    }
}
#define DBG_Gv(P,...)				\
  if (P->flags & DEBUG_GC && P->flags & DEBUG_VERBOSE) { \
    printf(__VA_ARGS__);			\
  }
#define DBG_G(P,...)	       \
  if (P->flags & DEBUG_GC) {   \
    printf(__VA_ARGS__);       \
  }
#else
#define DBG_Gv(...)
#define DBG_G(...)
#endif

PN_SIZE potion_stack_len(Potion *P, _PN **p) {
  _PN *esp, *c = P->mem->cstack;
  POTION_ESP(&esp);
  if (p) *p = STACK_UPPER(c, esp);
  return esp < c ? c - esp : esp - c + 1;
}

#define HAS_REAL_TYPE(v) (P->vts == NULL || (((struct PNFwd *)v)->fwd == POTION_COPIED || PN_TYPECHECK(PN_VTYPE(v))))

ATTRIBUTE_NO_ADDRESS_SAFETY_ANALYSIS
static PN_SIZE pngc_mark_array(Potion *P, register _PN *x, register long n, int type) {
  _PN v;
  PN_SIZE i = 0;
  struct PNMemory *M = P->mem;

  while (n--) {
    v = *x;
    if (IS_GC_PROTECTED(v) || IN_BIRTH_REGION(v) || IN_OLDER_REGION(v)) {
      v = potion_fwd(v);
      switch (type) {
        case 0: // count only
          if (!IS_GC_PROTECTED(v) && IN_BIRTH_REGION(v) && HAS_REAL_TYPE(v)) {
            i++;
            DBG_Gv(P,"GC mark count only %p %6x\n", x, PN_TYPE(*x));
	  }
        break;
        case 1: // minor
          if (!IS_GC_PROTECTED(v) && IN_BIRTH_REGION(v) && HAS_REAL_TYPE(v)) {
            GC_FORWARD(x, v);
            i++;
            DBG_Gv(P,"GC mark minor %p -> 0x%lx %6x\n", x, v, PN_TYPE(*x));
          }
        break;
        case 2: // major
          if (!IS_GC_PROTECTED(v) && (IN_BIRTH_REGION(v) || IN_OLDER_REGION(v)) && HAS_REAL_TYPE(v)) {
            GC_FORWARD(x, v);
            i++;
            DBG_Gv(P,"GC mark major %p -> 0x%lx %6x\n", x, v, PN_TYPE(*x));
          }
        break;
      }
    }
    x++;
  }
  return i;
}

ATTRIBUTE_NO_ADDRESS_SAFETY_ANALYSIS
PN_SIZE potion_mark_stack(Potion *P, int type) {
  long n;
  _PN *end, *start = P->mem->cstack;
  POTION_ESP(&end);
#if POTION_STACK_DIR > 0
  n = end - start;
#else
  n = start - end + 1;
  start = end;
  end = P->mem->cstack;
#endif
  DBG_Gv(P,"mark_stack (%p -> %p = %ld, type=%d)\n", start, end, n, type);
  if (n <= 0) return 0;
  return pngc_mark_array(P, start, n, type);
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
  if (newad == NULL)
    potion_fatal("Out of memory");
  wb = (void *)(((void **)(newad + sz)) - (keeps + 4));
  PN_MEMCPY_N(wb + 1, M->birth_storeptr + 1, void *, keeps);
  DEL_BIRTH_REGION();
  SET_GEN(birth, newad, sz);
  SET_STOREPTR(5 + keeps);
  return sz;
}

void potion_gc_minor_parser(PN parser) {
  if(parser == 0)
    return;

  struct _GREG *G = (struct _GREG *)parser;
  struct PNMemory *M = ((Potion *)G->data)->mem;
  Potion *P = G->data;

  if(PN_IS_PTR(G->ss)) {
    GC_MINOR_UPDATE(G->ss);
    potion_mark_minor(G->data, (const struct PNObject *) G->ss);
  }
  if(PN_IS_PTR(G->val[0])) {
    GC_MINOR_UPDATE(G->val[0]);
    potion_mark_minor(G->data, (const struct PNObject *) G->val[0]);
  }
  int c = G->val - G->vals;
  for(int i = 0; i < c; i++) {
    if(PN_IS_PTR(G->vals[i])) {
      GC_MINOR_UPDATE(G->vals[i]);
      potion_mark_minor(G->data, (const struct PNObject *) G->vals[i]);
    }
  }
}

void potion_gc_major_parser(PN parser) {
  if(parser == 0)
    return;

  struct _GREG *G = (struct _GREG *)parser;
  struct PNMemory *M = ((Potion *)G->data)->mem;
  Potion *P = G->data;

  if(PN_IS_PTR(G->ss)) {
    GC_MAJOR_UPDATE(G->ss);
    potion_mark_major(P, (const struct PNObject *) G->ss);
  }
  if(PN_IS_PTR(G->val[0])) {
    GC_MAJOR_UPDATE(G->val[0]);
    potion_mark_major(P, (const struct PNObject *) G->val[0]);
  }
  int c = G->val - G->vals;
  for(int i = 0; i < c; i++) {
    if(G->vals[i] != NULL && PN_IS_PTR(G->vals[i])) {
      GC_MAJOR_UPDATE(G->vals[i]);
      potion_mark_major(P, (const struct PNObject *) G->vals[i]);
    }
  }
}

/** \par
  Both this function and potion_gc_major embody a simple
  Cheney loop (also called a "two-finger collector.")
  http://en.wikipedia.org/wiki/Cheney%27s_algorithm
  (Again, many thanks to Basile Starynkevitch for
  his tutelage in this matter.)
*/
static int potion_gc_minor(Potion *P, int sz) {
  struct PNMemory *M = P->mem;
  void *scanptr = 0;
  void **storead = 0, **wb = 0;

  if (sz < 0)
    sz = 0;
  else if (sz >= POTION_MAX_BIRTH_SIZE)
    return POTION_NO_MEM;

  scanptr = (void *) M->old_cur;
  DBG_Gv(P,"running gc_minor "
	"(young: %p -> %p = %ld) "
	"(old: %p -> %p = %ld) "
	"(storeptr len = %ld)\n",
	M->birth_lo, M->birth_hi, (long)(M->birth_hi - M->birth_lo),
	M->old_lo, M->old_hi, (long)(M->old_hi - M->old_lo),
	(long)((void *)M->birth_hi - (void *)M->birth_storeptr));
  potion_mark_stack(P, 1);

  GC_MINOR_STRINGS();
  potion_gc_minor_parser(P->parser);

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

  DBG_Gv(P,"(new young: %p -> %p = %ld)\n", M->birth_lo, M->birth_hi, (long)(M->birth_hi - M->birth_lo));
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
  else if (siz >= POTION_MAX_BIRTH_SIZE) {
    fprintf(stderr, "** Requesting too much memory: 0x%x > 0x%x\n", siz, POTION_MAX_BIRTH_SIZE);
    return POTION_NO_MEM;
  }

  prevoldlo = (void *)M->old_lo;
  prevoldhi = (void *)M->old_hi;
  prevoldcur = (void *)M->old_cur;

  DBG_G(P,"running gc_major "
	"(young: %p -> %p = %ld) "
	"(old: %p -> %p = %ld)\n",
	M->birth_lo, M->birth_hi, (long)(M->birth_hi - M->birth_lo),
	M->old_lo, M->old_hi, (long)(M->old_hi - M->old_lo));
  birthest = potion_birth_suggest(siz, prevoldlo, prevoldcur);
  newoldsiz = (((char *)prevoldcur - (char *)prevoldlo) + siz + birthest +
    POTION_GC_THRESHOLD + 16 * POTION_PAGESIZE) + ((char *)M->birth_cur - (char *)M->birth_lo);
  newold = pngc_page_new(&newoldsiz, 0);
  if (newold == NULL) {
    fprintf(stderr, "** Out of memory\n");
    return POTION_NO_MEM;
  }

  M->old_cur = scanptr = newold + (sizeof(PN) * 2);
  DBG_G(P,"(new old: %p -> %p = %d)\n", newold, (char *)newold + newoldsiz, newoldsiz);

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
  potion_gc_minor_parser(P->parser);

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
#ifdef DEBUG
  double time = mytime();
#endif
  M->pass++;
  M->collecting = 1;

  if (M->old_lo == NULL) {
    int gensz = POTION_MIN_BIRTH_SIZE * 4;
    if (gensz < sz * 4)
      gensz = min(POTION_MAX_BIRTH_SIZE, PN_ALIGN(sz * 4, POTION_PAGESIZE));
    void *page = pngc_page_new(&gensz, 0);
    if (page == NULL) {
      fprintf(stderr, "** Out of memory\n");
      return;
    }
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

#ifdef DEBUG
  M->time += mytime() - time;
#endif
  M->dirty = 0;
  M->collecting = 0;
}

PN_SIZE potion_type_size(Potion *P, const struct PNObject *ptr) {
  int sz = 0;

  switch (((struct PNFwd *)ptr)->fwd) {
    case POTION_COPIED:
    case POTION_FWD:
      sz = ((struct PNFwd *)ptr)->siz;
      goto done_1;
  }

  if (ptr->vt < PN_TNIL) goto err;
  if (ptr->vt > PN_TUSER) {
    if (P->vts && ptr->vt < PN_TNIL + P->vts->len
        && PN_VTABLE(ptr->vt) && PN_TYPECHECK(ptr->vt)) {
      sz = sizeof(struct PNObject) +
        (((struct PNVtable *)PN_VTABLE(ptr->vt))->ivlen * sizeof(PN));
      //sz = potion_send((PN)ptr, PN_size); //cannot use bind with POTION_COPIED objs during GC!
    } else {
    err:
      if (P->flags & (DEBUG_VERBOSE
#ifdef DEBUG
			 |DEBUG_GC
#endif
			 ))
      fprintf(stderr, "** Invalid User Object 0x%lx vt: 0x%lx\n",
	      (unsigned long)ptr, (unsigned long)ptr->vt);
      return 0;
    }
    goto done_1;
  }

  switch (ptr->vt) {
    case PN_TNUMBER:
      sz = sizeof(struct PNDouble);
    break;
    case PN_TSTRING:
      sz = sizeof(struct PNString) + PN_STR_LEN(ptr) + 1;
    break;
    case PN_TCLOSURE:
      sz = sizeof(struct PNClosure) + (PN_CLOSURE(ptr)->extra * sizeof(PN));
    break;
    case PN_TTUPLE:
      sz = sizeof(struct PNTuple) + (sizeof(PN) * ((struct PNTuple *)ptr)->alloc);
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
      sz = sizeof(struct PNSource);
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
    case PN_TLICK:
      sz = sizeof(struct PNLick);
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

done_1:
  if (sz < sizeof(struct PNFwd))
    sz = sizeof(struct PNFwd);
  return PN_ALIGN(sz, 8); // force 64-bit alignment
}

void *potion_gc_copy(Potion *P, struct PNObject *ptr) {
  void *dst = (void *)P->mem->old_cur;
  PN_SIZE sz = potion_type_size(P, (const struct PNObject *)ptr);
  if (!sz) { //external pointer or immediate value
    DBG_G(P,"GC copy: assuming extern pointer or immediate potion value %p: %ld / 0x%lx\n", ptr, *(long*)ptr, *(long*)ptr);
    //return ptr;
    memcpy(dst, ptr, sizeof(void *)); // coverity[suspicious_sizeof::FALSE]
    return dst;
  }
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
    goto done_2;
  }

  if (ptr->vt > PN_TUSER) {
    GC_MINOR_UPDATE(PN_VTABLE(ptr->vt));
    int ivars = ((struct PNVtable *)PN_VTABLE(ptr->vt))->ivlen;
    for (i = 0; i < ivars; i++)
      GC_MINOR_UPDATE(((struct PNObject *)ptr)->ivars[i]);
    goto done_2;
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
      DBG_Gv(P,"GC mark minor Potion_State\n");  // only with threads
      GC_MINOR_UPDATE(((Potion *)ptr)->strings);
      GC_MINOR_UPDATE(((Potion *)ptr)->lobby);
      GC_MINOR_UPDATE(((Potion *)ptr)->vts);
      GC_MINOR_UPDATE(((Potion *)ptr)->call);
      GC_MINOR_UPDATE(((Potion *)ptr)->callset);
      GC_MINOR_UPDATE(((Potion *)ptr)->input);
      GC_MINOR_UPDATE(((Potion *)ptr)->source);
      //GC_MINOR_UPDATE(((Potion *)ptr)->pbuf);
      GC_MINOR_UPDATE(((Potion *)ptr)->line);
      GC_MINOR_UPDATE(((Potion *)ptr)->unclosed);
      //GC_MINOR_UPDATE(((Potion *)ptr)->target);
      GC_MINOR_UPDATE(((Potion *)ptr)->mem);
    break;
    case PN_TFILE:
      GC_MINOR_UPDATE(((struct PNFile *)ptr)->path);
    break;
    case PN_TVTABLE:
      if (((struct PNVtable *)ptr)->parent)
        GC_MINOR_UPDATE(PN_VTABLE(((struct PNVtable *)ptr)->parent));
      GC_MINOR_UPDATE(((struct PNVtable *)ptr)->name);
      GC_MINOR_UPDATE(((struct PNVtable *)ptr)->ivars);
      GC_MINOR_UPDATE(((struct PNVtable *)ptr)->methods);
      GC_MINOR_UPDATE(((struct PNVtable *)ptr)->meta);
      GC_MINOR_UPDATE(((struct PNVtable *)ptr)->ctor);
      GC_MINOR_UPDATE(((struct PNVtable *)ptr)->call);
      GC_MINOR_UPDATE(((struct PNVtable *)ptr)->callset);
    break;
    case PN_TSOURCE:
      GC_MINOR_UPDATE(((struct PNSource *)ptr)->a[0]);
      GC_MINOR_UPDATE(((struct PNSource *)ptr)->a[1]);
      GC_MINOR_UPDATE(((struct PNSource *)ptr)->a[2]);
      GC_MINOR_UPDATE(((struct PNSource *)ptr)->line);
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
      GC_MINOR_UPDATE(((struct PNProto *)ptr)->debugs);
      GC_MINOR_UPDATE(((struct PNProto *)ptr)->tree);
      GC_MINOR_UPDATE(((struct PNProto *)ptr)->asmb);
    break;
    case PN_TTABLE:
      GC_MINOR_UPDATE_TABLE(PN, (struct PNTable *)potion_fwd((PN)ptr), 1);
    break;
    case PN_TLICK:
      GC_MINOR_UPDATE(((struct PNLick *)ptr)->name);
      GC_MINOR_UPDATE(((struct PNLick *)ptr)->attr);
      GC_MINOR_UPDATE(((struct PNLick *)ptr)->inner);
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

done_2:
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
    goto done_3;
  }

  if (ptr->vt > PN_TUSER) {
    GC_MAJOR_UPDATE(PN_VTABLE(ptr->vt));
    int ivars = ((struct PNVtable *)PN_VTABLE(ptr->vt))->ivlen;
    for (i = 0; i < ivars; i++)
      GC_MAJOR_UPDATE(((struct PNObject *)ptr)->ivars[i]);
    goto done_3;
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
      DBG_Gv(P,"GC mark major Potion_State\n"); // only with threads
      //DBG_G(P,"   strings\n");
      GC_MAJOR_UPDATE(((Potion *)ptr)->strings);
      //DBG_G(P,"   lobby\n");
      GC_MAJOR_UPDATE(((Potion *)ptr)->lobby);
      //DBG_G(P,"   vts\n");
      GC_MAJOR_UPDATE(((Potion *)ptr)->vts);
      //DBG_G(P,"   call\n");
      GC_MAJOR_UPDATE(((Potion *)ptr)->call);
      //DBG_G(P,"   callset\n");
      GC_MAJOR_UPDATE(((Potion *)ptr)->callset);
      //DBG_G(P,"   input\n");
      GC_MAJOR_UPDATE(((Potion *)ptr)->input);
      //DBG_G(P,"   source\n");
      GC_MAJOR_UPDATE(((Potion *)ptr)->source);
      //DBG_G(P,"   pbuf\n");
      //GC_MAJOR_UPDATE(((Potion *)ptr)->pbuf);
      //DBG_G(P,"   line\n");
      GC_MAJOR_UPDATE(((Potion *)ptr)->line);
      //DBG_G(P,"   unclosed\n");
      GC_MAJOR_UPDATE(((Potion *)ptr)->unclosed);
      //GC_MAJOR_UPDATE(((Potion *)ptr)->target);
      //DBG_G(P,"   mem\n");
      GC_MAJOR_UPDATE(((Potion *)ptr)->mem);
    break;
    case PN_TFILE:
      GC_MAJOR_UPDATE(((struct PNFile *)ptr)->path);
    break;
    case PN_TVTABLE:
      if (((struct PNVtable *)ptr)->parent)
        GC_MAJOR_UPDATE(PN_VTABLE(((struct PNVtable *)ptr)->parent));
      GC_MAJOR_UPDATE(((struct PNVtable *)ptr)->name);
      GC_MAJOR_UPDATE(((struct PNVtable *)ptr)->ivars);
      GC_MAJOR_UPDATE(((struct PNVtable *)ptr)->methods);
      GC_MAJOR_UPDATE(((struct PNVtable *)ptr)->meta);
      GC_MAJOR_UPDATE(((struct PNVtable *)ptr)->ctor);
      GC_MAJOR_UPDATE(((struct PNVtable *)ptr)->call);
      GC_MAJOR_UPDATE(((struct PNVtable *)ptr)->callset);
    break;
    case PN_TSOURCE:
      GC_MAJOR_UPDATE(((struct PNSource *)ptr)->a[0]);
      GC_MAJOR_UPDATE(((struct PNSource *)ptr)->a[1]);
      GC_MAJOR_UPDATE(((struct PNSource *)ptr)->a[2]);
      GC_MAJOR_UPDATE(((struct PNSource *)ptr)->line);
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
      GC_MAJOR_UPDATE(((struct PNProto *)ptr)->debugs);
      GC_MAJOR_UPDATE(((struct PNProto *)ptr)->tree);
      GC_MAJOR_UPDATE(((struct PNProto *)ptr)->asmb);
    break;
    case PN_TTABLE:
      GC_MAJOR_UPDATE_TABLE(PN, (struct PNTable *)potion_fwd((PN)ptr), 1);
    break;
    case PN_TLICK:
      GC_MAJOR_UPDATE(((struct PNLick *)ptr)->name);
      GC_MAJOR_UPDATE(((struct PNLick *)ptr)->attr);
      GC_MAJOR_UPDATE(((struct PNLick *)ptr)->inner);
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

done_3:
  sz = potion_type_size(P, ptr);
  return (void *)((char *)ptr + sz);
}

/** \par
 Potion's GC is a generational copying GC. This is why the
 volatile keyword is used so liberally throughout the source
 code. PN types may suddenly move during any collection phase.
 They move from the birth area to the old area.

 Potion actually begins by allocating an old area. This is for
 two reasons. First, the script may be too short to require an
 old area, so we want to avoid allocating two areas to start with.
 And second, since Potion loads its core classes into GC first,
 we save ourselves a severe promotion step by beginning with an
 automatic promotion to second generation. (Oh and this allows
 the core Potion struct pointer to be non-volatile.)

 In short, this first page is never released, since the GC struct
 itself is on that page.

 While this may pay a slight penalty in memory size for long-running
 scripts, perhaps I could add some occassional compaction to solve
 that as well.
 \sa potion_init() which calls GC_PROTECT()
*/
Potion *potion_gc_boot(void *sp) {
  Potion *P;
  int bootsz = POTION_MIN_BIRTH_SIZE;
  void *page1 = pngc_page_new(&bootsz, 0);
  if (page1 == NULL)
    potion_fatal("Not enough memory");
  struct PNMemory *M = (struct PNMemory *)page1;
  PN_MEMZERO(M, struct PNMemory);
#ifdef DEBUG
  M->time = 0.0;
#endif

  SET_GEN(birth, page1, bootsz);
  SET_STOREPTR(4);

  // stack must be 16-byte aligned on amd64 SSE or __APPLE__, and 32-byte with AVX instrs.
  // at least amd64 atof() does SSE register return.
#if (PN_SIZE_T == 8) || defined(__APPLE__)
  M->cstack = (((_PN)sp & ((1<<5)-1)) == 0 )
    ? sp : (void *)(_PN)((_PN)sp | ((1<<5)-1) )+1;
#else
  M->cstack = sp;
#endif
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
    DBG_G(P,"GC page delete: %p - %p\n", M, protend);
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
