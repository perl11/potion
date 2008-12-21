//
// internal.h
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#ifndef POTION_INTERNAL_H
#define POTION_INTERNAL_H

#define PN_ALLOC(T)          (T *)malloc(sizeof(T))
#define PN_ALLOC_N(T,N)      (T *)malloc(sizeof(T)*(N))
#define PN_REALLOC(X,T)      (X)=(T *)realloc((char *)(X), sizeof(T))
#define PN_REALLOC_N(X,T,N)  (X)=(T *)realloc((char *)(X), sizeof(T)*(N))
#define PN_FREE(T)           free((void *)T)

#define PN_MEMZERO(X,T)      memset((X), 0, sizeof(T))
#define PN_MEMZERO_N(X,T,N)  memset((X), 0, sizeof(T)*(N))
#define PN_MEMCPY(X,Y,T)     memcpy((X), (Y), sizeof(T))
#define PN_MEMCPY_N(X,Y,T,N) memcpy((X), (Y), sizeof(T)*(N))

void *LemonPotionAlloc(void *(*)(size_t));
void LemonPotion(void *, int, int, char *);
void LemonPotionFree(void *, void (*)(void*));

#endif
