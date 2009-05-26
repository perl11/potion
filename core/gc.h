//
// gc.h
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#ifndef POTION_GC_H
#define POTION_GC_H

#ifndef POTION_BIRTH_SIZE
#define POTION_BIRTH_SIZE  (1 << 20)
#endif

#ifndef POTION_MAX_BIRTH_SIZE
#define POTION_MAX_BIRTH_SIZE (16 * POTION_BIRTH_SIZE)
#endif

#if POTION_MAX_BIRTH_SIZE < 4 * POTION_BIRTH_SIZE
#error invalid min and max birth sizes
#endif

#define POTION_GC_THRESHOLD (3 * POTION_BIRTH_SIZE)
#define POTION_GC_PERIOD    256
#define POTION_NB_ROOTS     64

#define SET_GEN(t, p, s) \
  M->t##_lo = p; \
  M->t##_cur = p + 2 * sizeof(void *); \
  M->t##_hi = p + (s);

#define SET_STOREPTR(n) \
  M->birth_storeptr = (void *)(((void **)M->birth_hi) - n)

#define IS_GC_PROTECTED(p) \
  ((_PN)(p) >= (_PN)M && (_PN)(p) < (_PN)M->protect)

#define IN_BIRTH_REGION(p) \
  ((_PN)(p) >= (_PN)M->birth_lo && (_PN)(p) < (_PN)M->birth_hi)

#define IS_NEW_PTR(p) \
  (PN_IS_PTR(p) && IN_BIRTH_REGION(p) && !IS_GC_PROTECTED(p))

#define GC_FORWARD(p) do { \
  struct PNObject *_pnobj = *((struct PNObject **)p); \
  if (_pnobj->vt == PN_NIL) { \
    *(p) = _pnobj->data[0]; \
  } else { \
    void *_pnad = potion_gc_copy(M, (const struct PNObject *)_pnobj); \
    _pnobj->vt = 0; \
    *(p) = _pnobj->data[0] = (_PN)_pnad; \
  } \
}  while(0)

#define GC_MINOR_UPDATE(p) do { \
  PN _p; \
  GC_FOLLOW_FORWARD(p); \
  _p = (PN)(p); \
  if (_p >= (PN)M->birth_lo && _p < (PN)M->birth_hi) \
    { GC_FORWARD((void**)&(p)); } \
} while(0)

#define GC_FULL_UPDATE(p) do { \
  PN _p; \
  GC_FOLLOW_FORWARD(p); \
  _p = (PN)(p); \
  if ((_p >= (PN)M->birth_lo && _p < (PN)M->birth_hi) || \
      (_p >= (PN)M->old_lo && _p < (PN)M->old_hi)) \
    {GC_FORWARD((void**)&(p));} \
} while(0)

#define GC_FOLLOW_FORWARD(p) do { \
  while ((PN)(p) > (PN)POTION_PAGESIZE && *((PN *)(p)) == 0) \
    (p) = (void*)(((PN *)(p))[1]); \
} while(0)

PN_SIZE potion_stack_len(struct PNMemory *, _PN **);
PN_SIZE potion_mark_stack(struct PNMemory *, int);
void *potion_gc_copy(struct PNMemory *, const struct PNObject *);
void *pngc_page_new(int *, const char);
void *potion_mark_minor(struct PNMemory *, const struct PNObject *);

#endif
