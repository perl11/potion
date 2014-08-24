/** \file internal.h
 non-API internal parts

 (c) 2008 why the lucky stiff, the freelance professor */
#ifndef POTION_INTERNAL_H
#define POTION_INTERNAL_H

struct Potion_State;

typedef unsigned char u8;

#define PN_ALLOC(V,T)        (T *)potion_gc_alloc(P, V, sizeof(T))
#define PN_ALLOC_N(V,T,C)    (T *)potion_gc_alloc(P, V, sizeof(T)+C)
#define PN_CALLOC_N(V,T,C)   (T *)potion_gc_calloc(P, V, sizeof(T)+C)
#define PN_REALLOC(X,V,T,N)  (X)=(T *)potion_gc_realloc(P, V, (struct PNObject *)(X), sizeof(T) + N)
#define PN_DALLOC_N(T,N)     potion_data_alloc(P, sizeof(T)*N)
#define PN_STRDUP(S)         PN_STR(S)

#define PN_MEMZERO(X,T)      memset((X), 0, sizeof(T))
#define PN_MEMZERO_N(X,T,N)  memset((X), 0, sizeof(T)*(N))
#define PN_MEMCPY(X,Y,T)     memcpy((void *)(X), (void *)(Y), sizeof(T))
#define PN_MEMCPY_N(X,Y,T,N) memcpy((void *)(X), (void *)(Y), sizeof(T)*(N))
#define PN_MEMMOVE_N(DST,SRC,T,N) memmove((void *)(DST), (void *)(SRC), sizeof(T)*(N))

#ifndef min
#define min(a, b) ((a) <= (b) ? (a) : (b))
#endif

#ifndef max
#define max(a, b) ((a) >= (b) ? (a) : (b))
#endif

#define TYPE_BATCH_SIZE 4096

#define PN_FLEX_NEW(N, V, T, S) \
  (N) = PN_ALLOC_N(V, T, (sizeof(*(N)->ptr) * S)); \
  (N)->siz = sizeof(*(N)->ptr) * S; \
  (N)->len = 0

#define PN_FLEX_NEEDS(X, N, V, T, S) ({ \
  PN_SIZE capa = (N)->siz / sizeof(*(N)->ptr); \
  if (capa < (N)->len + X) { \
    while (capa < (N)->len + X) \
      capa += S; \
    capa = sizeof(*(N)->ptr) * capa; \
    PN_REALLOC(N, V, T, capa); \
    (N)->siz = capa; \
  } \
})

#define PN_ATOI(X,N,B) ({ \
  char *Ap = X; \
  long Ai = 0; \
  size_t Al = N; \
  while (Al--) { \
    if ((*Ap >= '0') && (*Ap <= '9')) \
      Ai = (Ai * B) + (*Ap - '0'); \
    else if ((*Ap >= 'A') && (*Ap <= 'F')) \
      Ai = (Ai * B) + ((*Ap - 'A') + 10); \
    else if ((*Ap >= 'a') && (*Ap <= 'f')) \
      Ai = (Ai * B) + ((*Ap - 'a') + 10); \
    else break; \
    Ap++; \
  } \
  Ai; \
})

/// .pnb binary dump header
struct PNBHeader {
  u8 sig[4];
  u8 major;
  u8 minor;
  u8 vmid;
  u8 pn;
  u8 proto[];
};

size_t potion_cp_strlen_utf8(const char *);
void *potion_mmap(size_t, const char);
int potion_munmap(void *, size_t);
// i686-w64-mingw32 /include/stdio.h has asprintf defined
// i386-mingw32 not
#if POTION_WIN32 && !defined(__MINGW_SCANF_FORMAT)
int vasprintf (char **strp, const char *fmt, __VALIST ap);
int asprintf (char **string_ptr, const char *format, ...);
#endif
#define PN_ALLOC_FUNC(size) potion_mmap(size, 1)

//
// stack manipulation routines
//
#if POTION_X86 == POTION_JIT_TARGET
#if PN_SIZE_T == 8
// preserve: rbx r12 r13 r14 r15. scratch: rax rcx rdx r8 r9 r10 r11.
#define PN_SAVED_REGS 5
#if defined(__SANITIZE_ADDRESS__) && defined(__APPLE__)
#define POTION_ESP(p) __asm__("mov %%rsp, %0" : "=r" (*p)); *p += 0x178
#else
#if defined(__SANITIZE_ADDRESS__) && defined(__linux__)
#define POTION_ESP(p) __asm__("mov %%rsp, %0" : "=r" (*p)); *p += 0xd0
#else
#define POTION_ESP(p) __asm__("mov %%rsp, %0" : "=r" (*p))
#endif
#endif
#define POTION_EBP(p) __asm__("mov %%rbp, %0" : "=r" (*p))
#else
#define PN_SAVED_REGS 3
#if defined(__SANITIZE_ADDRESS__) && defined(__APPLE__)
#define POTION_ESP(p) __asm__("mov %%esp, %0" : "=r" (*p)); *p += 0x178
#else
#if defined(__SANITIZE_ADDRESS__) && defined(__linux__)
#define POTION_ESP(p) __asm__("mov %%esp, %0" : "=r" (*p)); *p += 0xd0
#else
#define POTION_ESP(p) __asm__("mov %%esp, %0" : "=r" (*p))
#endif
#endif
#define POTION_EBP(p) __asm__("mov %%ebp, %0" : "=r" (*p))
#endif
#else
#define PN_SAVED_REGS 0
__attribute__ ((noinline)) void potion_esp(void **);
#define POTION_ESP(p) potion_esp((void **)p)
#define POTION_EBP(p) potion_esp((void **)p)
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#if POTION_STACK_DIR > 0
#define STACK_UPPER(a, b) a
#elif POTION_STACK_DIR < 0
#define STACK_UPPER(a, b) b
#endif

#define GC_PROTECT(P) P->mem->protect = (void *)P->mem->birth_cur

#endif
