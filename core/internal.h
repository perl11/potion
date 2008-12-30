//
// internal.h
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#ifndef POTION_INTERNAL_H
#define POTION_INTERNAL_H

struct Potion_State;

typedef unsigned char u8;

#define PN_ALLOC(T)          (T *)malloc(sizeof(T))
#define PN_ALLOC2(T,C)       (T *)malloc(sizeof(T)+C)
#define PN_ALLOC_N(T,N)      (T *)malloc(sizeof(T)*(N))
#define PN_REALLOC(X,T)      (X)=(T *)realloc((char *)(X), sizeof(T))
#define PN_REALLOC_N(X,T,N)  (X)=(T *)realloc((char *)(X), sizeof(T)*(N))
#define PN_FREE(T)           free((void *)T)

#define PN_MEMZERO(X,T)      memset((X), 0, sizeof(T))
#define PN_MEMZERO_N(X,T,N)  memset((X), 0, sizeof(T)*(N))
#define PN_MEMCPY(X,Y,T)     memcpy((X), (Y), sizeof(T))
#define PN_MEMCPY_N(X,Y,T,N) memcpy((X), (Y), sizeof(T)*(N))

#define PN_ATOI(X,N) ({ \
  char *Ap = X; \
  int Ai = 0; \
  size_t Al = N; \
  while (Al--) { \
    if ((*Ap >= '0') && (*Ap <= '9')) { \
      Ai = (Ai * 10) + (*Ap - '0'); \
      Ap++; \
    } \
    else break; \
  } \
  Ai; \
})

struct PNBHeader {
  u8 sig[4];
  u8 vmid;
  u8 pn;
  u8 proto[0];
};

void *LemonPotionAlloc(void *(*)(size_t));
void LemonPotion(void *, int, PN, struct Potion_State *);
void LemonPotionFree(void *, void (*)(void*));

#endif
