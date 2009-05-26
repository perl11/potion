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

#define IN_BIRTH_REGION(p) \
  ((_PN)(p) >= (_PN)M->birth_lo && (_PN)(p) <= (_PN)M->birth_hi)

#define IS_NEW_PTR(p) \
  (PN_IS_PTR(p) && IN_BIRTH_REGION(p))

#define MINOR_UPDATE(p) { \
  if (IN_BIRTH_REGION(p) && ((_PN)(p) & 3) == 0) \
    GC_FORWARD(&(p)); }

PN_SIZE potion_stack_len(struct PNMemory *, _PN **);
PN_SIZE potion_mark_stack(struct PNMemory *);

#endif
