//
// internal.h
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#ifndef POTION_INTERNAL_H
#define POTION_INTERNAL_H

#define PN_ALLOC(T)          (T *)malloc(sizeof(T))
#define PN_ALLOC_N(T,N)      (T *)malloc(sizeof(T)*(N))
#define PN_REALLOC_N(V,T,N)  (V)=(T *)realloc((char *)(V), sizeof(T)*(N))
#define PN_FREE(T)           free((void *)T)

#define PN_MEMZERO(V,T,N)    memset((V), 0, sizeof(T)*(N))
#define PN_MEMCPY(V1,V2,T,N) memcpy((V1), (V2), sizeof(T)*(N))

void *LemonPotionAlloc(void *(*)(size_t));
void LemonPotion(void *, int, int, char *);
void LemonPotionFree(void *, void (*)(void*));

#endif
