//
// internal.h
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#ifndef POTION_INTERNAL_H
#define POTION_INTERNAL_H

struct Potion_State;

typedef unsigned char u8;

#define PN_ALLOC(T)          (T *)potion_gc_alloc(P->mem, sizeof(T))
#define PN_ALLOC2(T,C)       (T *)potion_gc_alloc(P->mem, sizeof(T)+C)
#define PN_ALLOC_N(T,N)      (T *)potion_gc_alloc(P->mem, sizeof(T)*(N))
#define PN_CALLOC(T,C)       (T *)potion_gc_calloc(P->mem, sizeof(T)+C)
#define PN_CALLOC_N(T,N)     (T *)potion_gc_calloc(P->mem, sizeof(T)*N)

#define SYS_ALLOC(T)         (T *)malloc(sizeof(T))
#define SYS_ALLOC2(T,C)      (T *)malloc(sizeof(T)+C)
#define SYS_ALLOC_N(T,N)     (T *)malloc(sizeof(T)*(N))
#define SYS_CALLOC(T,C)      (T *)calloc(1, sizeof(T)+C)
#define SYS_CALLOC_N(T,C)    (T *)calloc(C, sizeof(T))
#define SYS_REALLOC(X,T)     (X)=(T *)realloc((char *)(X), sizeof(T))
#define SYS_REALLOC_N(X,T,N) (X)=(T *)realloc((char *)(X), sizeof(T)*(N))
#define SYS_FREE(T)          free((void *)T)

#define PN_MEMZERO(X,T)      memset((X), 0, sizeof(T))
#define PN_MEMZERO_N(X,T,N)  memset((X), 0, sizeof(T)*(N))
#define PN_MEMCPY(X,Y,T)     memcpy((void *)(X), (void *)(Y), sizeof(T))
#define PN_MEMCPY_N(X,Y,T,N) memcpy((void *)(X), (void *)(Y), sizeof(T)*(N))

#ifndef min
#define min(a, b) ((a) <= (b) ? (a) : (b))
#endif

#ifndef max
#define max(a, b) ((a) >= (b) ? (a) : (b))
#endif

#define PN_FLEX_NEW(N, T, S) \
  (N).ptr = SYS_ALLOC_N(T, S); \
  (N).capa = S; \
  (N).len = 0
#define PN_FLEX_NEEDS(X, N, T, S) \
  while ((N).capa < (N).len + X) \
    (N).capa += S; \
  SYS_REALLOC_N((N).ptr, T, (N).capa); \
  (N).len += X

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
  u8 major;
  u8 minor;
  u8 vmid;
  u8 pn;
  u8 proto[0];
};

void *LemonPotionAlloc(void *(*)(size_t));
void LemonPotion(void *, int, PN, struct Potion_State *);
void LemonPotionFree(void *, void (*)(void*));

size_t potion_cp_strlen_utf8(const char *);
void *potion_mmap(size_t, const char);
int potion_munmap(void *, size_t);
#define PN_ALLOC_FUNC(size) potion_mmap(size, 1)

//
// stack manipulation routines
//
#ifdef __i386
#define POTION_ESP(p) __asm__("mov %%esp, %0" : "=r" (*p))
#else
#ifdef POTION_X86
#define POTION_ESP(p) __asm__("mov %%rsp, %0" : "=r" (*p))
#else
__attribute__ ((noinline)) void potion_esp(void **);
#define POTION_ESP(p) potion_esp(p)
#endif
#endif

#if POTION_STACK_DIR > 0
#define STACK_UPPER(a, b) a
#elif POTION_STACK_DIR < 0
#define STACK_UPPER(a, b) b
#endif

#define GC_PROTECT(M) M->protect = (void *)M->birth_cur

#endif
