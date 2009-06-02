//
// gc.h
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#ifndef POTION_GC_H
#define POTION_GC_H

#ifndef POTION_BIRTH_SIZE
#define POTION_BIRTH_SIZE  (PN_SIZE_T << 16)
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
  M->t##_cur = p; \
  M->t##_hi = p + (s);

#define SET_STOREPTR(n) \
  M->birth_storeptr = (void *)(((void **)M->birth_hi) - n)

#define DEL_BIRTH_REGION() \
  if (M->birth_lo == M && IN_BIRTH_REGION(M->protect)) { \
    void *protend = (void *)PN_ALIGN((_PN)M->protect, POTION_PAGESIZE); \
    pngc_page_delete(protend, (char *)M->birth_hi - (char *)protend); \
  } else { \
    pngc_page_delete((void *)M->birth_lo, (char *)M->birth_hi - (char *)M->birth_lo); \
  }

#define IS_GC_PROTECTED(p) \
  ((_PN)(p) >= (_PN)M && (_PN)(p) < (_PN)M->protect)

#define IN_BIRTH_REGION(p) \
  ((_PN)(p) >= (_PN)M->birth_lo && (_PN)(p) < (_PN)M->birth_hi)

#define IN_OLDER_REGION(p) \
  ((_PN)(p) >= (_PN)M->old_lo && (_PN)(p) < (_PN)M->old_hi)

#define IS_NEW_PTR(p) \
  (PN_IS_PTR(p) && IN_BIRTH_REGION(p) && !IS_GC_PROTECTED(p))

#define GC_FORWARD(p) do { \
  struct PNObject *_pnobj = *((struct PNObject **)p); \
  if (_pnobj->vt == PN_TNIL) { \
    *(p) = _pnobj->data[0]; \
  } else { \
    void *_pnad = potion_gc_copy(M, (const struct PNObject *)_pnobj); \
    _pnobj->vt = PN_TNIL; \
    *(p) = _pnobj->data[0] = (_PN)_pnad; \
  } \
}  while(0)

#define GC_MINOR_UPDATE(p) do { \
  if (PN_IS_PTR(p)) { \
    (p) = potion_fwd(p); \
    if (IN_BIRTH_REGION(p) && !IS_GC_PROTECTED(p)) \
      { GC_FORWARD(&(p)); } \
  } \
} while(0)

#define GC_MAJOR_UPDATE(p) do { \
  if (PN_IS_PTR(p)) { \
    (p) = potion_fwd(p); \
    if (!IS_GC_PROTECTED(p) && \
        (IN_BIRTH_REGION(p) || IN_OLDER_REGION(p))) \
      {GC_FORWARD((_PN *)&(p));} \
  } \
} while(0)

PN_SIZE potion_stack_len(struct PNMemory *, _PN **);
PN_SIZE potion_mark_stack(struct PNMemory *, int);
void *potion_gc_copy(struct PNMemory *, const struct PNObject *);
void *pngc_page_new(int *, const char);
void *potion_mark_minor(struct PNMemory *, const struct PNObject *);
void *potion_mark_major(struct PNMemory *, const struct PNObject *);

#endif
