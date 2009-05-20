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
#define POTION_GC_PERIOD 256

struct PNMemory {
  // the birth region
  volatile void *birth_lo, *birth_hi, *birth_cur;
  volatile void **birth_storeptr;
  volatile int birth_changedconstrank;

  // the old region (TODO: consider making the old region common to all threads)
  volatile void *old_lo, *old_hi, *old_cur;

  volatile struct PNFrame *frame;
  volatile int collecting, dirty;
};

#endif
