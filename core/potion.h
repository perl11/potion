//
// potion.h
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#ifndef POTION_H
#define POTION_H

#define POTION_VERSION  "0.0"
#define POTION_MINOR    0
#define POTION_MAJOR    0
#define POTION_SIG      "p\07\10n"
#define POTION_VMID     0x79

#define POTION_X86      0
#define POTION_PPC      1
#define POTION_TARGETS  2

#include <limits.h>
#include <string.h>
#include "config.h"

//
// types
//
typedef unsigned long PNType, _PN;
typedef unsigned int PN_SIZE;
typedef struct Potion_State Potion;

struct PNObject;
struct PNData;
struct PNString;
struct PNBytes;
struct PNDecimal;
struct PNClosure;
struct PNProto;
struct PNTuple;
struct PNWeakRef;
struct PNMemory;
struct PNJitAsm;

#define PN_TNIL         0
#define PN_TNUMBER      1
#define PN_TBOOLEAN     2
#define PN_TSTRING      3
#define PN_TWEAK        4
#define PN_TCLOSURE     5
#define PN_TTUPLE       6
#define PN_TSTATE       7
#define PN_TFILE        8
#define PN_TOBJECT      9
#define PN_TVTABLE      10
#define PN_TSOURCE      11
#define PN_TBYTES       12
#define PN_TPROTO       13
#define PN_TLOBBY       14
#define PN_TTABLE       15
#define PN_TUSER        16

#define PN              volatile _PN
#define vPN(t)          struct PN##t * volatile
#define PN_TYPE(x)      potion_type((PN)(x))
#define PN_VTYPE(x)     (((struct PNObject *)(x))->vt)
#define PN_VTABLE(t)    (PN_FLEX_AT(P->vts, t))

#define PN_NIL          ((PN)0)
#define PN_ZERO         ((PN)1)
#define PN_FALSE        ((PN)2)
#define PN_TRUE         ((PN)6)
#define PN_PRIMITIVE    7
#define PN_REF_MASK     ~7
#define PN_NONE         ((PN_SIZE)-1)

#define PN_TEST(v)      ((PN)(v) != PN_FALSE)
#define PN_BOOL(v)      ((v) ? PN_TRUE : PN_FALSE)
#define PN_IS_PTR(v)    (!PN_IS_NUM(v) && ((v) & PN_REF_MASK))
#define PN_IS_NIL(v)    ((PN)(v) == PN_NIL)
#define PN_IS_BOOL(v)   ((PN)(v) & PN_TBOOLEAN)
#define PN_IS_NUM(v)    ((PN)(v) & PN_TNUMBER)
#define PN_IS_TUPLE(v)  (PN_TYPE(v) == PN_TTUPLE)
#define PN_IS_STR(v)    (PN_TYPE(v) == PN_TSTRING)
#define PN_IS_TABLE(v)  (PN_TYPE(v) == PN_TTABLE)
#define PN_IS_CLOSURE(v) (PN_TYPE(v) == PN_TCLOSURE)
#define PN_IS_DECIMAL(v) (PN_IS_PTR(v) && PN_TYPE(v) == PN_TNUMBER)
#define PN_IS_PROTO(v)   (PN_TYPE(v) == PN_TPROTO)
#define PN_IS_REF(v)     (((PN)(v) & PN_PRIMITIVE) == PN_TWEAK)

#define PN_NUM(i)       ((PN)((((long)(i))<<1) + PN_TNUMBER))
#define PN_INT(x)       (((long)(x))>>1)
#define PN_STR_PTR(x)   potion_str_ptr((struct PNString *)(x))
#define PN_STR_LEN(x)   ((struct PNString *)(x))->len
#define PN_CLOSURE(x)   ((struct PNClosure *)(x))
#define PN_CLOSURE_F(x) ((struct PNClosure *)(x))->method
#define PN_PROTO(x)     ((struct PNProto *)(x))
#define PN_FUNC(f, s)   potion_closure_new(P, (PN_F)f, potion_sig(P, s), 0)
#define PN_SET_REF(t)   (((PN)t)+PN_TWEAK)
#define PN_GET_REF(t)   ((struct PNWeakRef *)(((PN)t)^PN_TWEAK))
#define PN_DEREF(x)     PN_GET_REF(x)->data

#define PN_ALIGN(o, x)   (((((o) - 1) / (x)) + 1) * (x))
#define PN_FLEX(N, T)    struct { T *ptr; PN_SIZE capa; PN_SIZE len; } N;
#define PN_FLEX_AT(N, I) N.ptr[I]
#define PN_FLEX_SIZE(N)  N.len

#define PN_IS_EMPTY(T)  (PN_GET_TUPLE(T)->len == 0)
#define PN_TUP0()       potion_tuple_empty(P)
#define PN_TUP(X)       potion_tuple_new(P, X)
#define PN_PUSH(T, X)   potion_tuple_push(P, T, X)
#define PN_GET(T, X)    potion_tuple_find(P, T, X)
#define PN_PUT(T, X)    potion_tuple_push_unless(P, T, X)
#define PN_SET_TUPLE(t) ((PN)t)
#define PN_GET_TUPLE(t) ((struct PNTuple *)t)
#define PN_TUPLE_LEN(t) PN_GET_TUPLE(t)->len
#define PN_TUPLE_AT(t, n) PN_GET_TUPLE(t)->set[n]
#define PN_TUPLE_COUNT(T, I, B) ({ \
    struct PNTuple * volatile __t##I = PN_GET_TUPLE(T); \
    if (__t##I->len != 0) { \
      PN_SIZE I; \
      for (I = 0; I < __t##I->len; I++) B \
    } \
  })
#define PN_TUPLE_EACH(T, I, V, B) ({ \
    struct PNTuple * volatile __t##V = PN_GET_TUPLE(T); \
    if (__t##V->len != 0) { \
      PN_SIZE I; \
      for (I = 0; I < __t##V->len; I++) { \
        PN V = __t##V->set[I]; \
        B \
      } \
    } \
  })

#define PN_OBJECT_HEADER \
  PNType vt;

#define PN_BOOT_OBJ_ALLOC(S, T, L) \
  ((S *)potion_allocate(P, 0, PN_VTABLE(T), PN_NUM((sizeof(S)-sizeof(struct PNObject))+(L))))
#define PN_OBJ_ALLOC(S, T, L) \
  ((S *)potion_send(PN_VTABLE(T), PN_allocate, PN_NUM((sizeof(S)-sizeof(struct PNObject))+(L))))

//
// standard objects act like C structs
// the fields are defined by the type
// and it's a fixed size, not volatile.
//
struct PNObject {
  PN_OBJECT_HEADER
  PN data[0];
};

//
// struct to wrap arbitrary data that
// we may want to allocate from Potion.
//
struct PNData {
  PN_OBJECT_HEADER
  PN_SIZE len;
  char data[0]; 
};

//
// strings are immutable UTF-8, the ID is
// incremental and they may be garbage
// collected. (non-volatile)
//
struct PNString {
  PN_OBJECT_HEADER
  PN_SIZE len;
  unsigned int id;
  char chars[0];
};

//
// byte strings are raw character data,
// volatile, may be appended/changed.
//
struct PNBytes {
  PN_OBJECT_HEADER
  PN_SIZE len;
  char *chars;
};

#define PN_PREC 8

//
// decimals are floating point numbers
// stored as binary data. immutable.
//
struct PNDecimal {
  PN_OBJECT_HEADER
  PN_SIZE sign;
  PN_SIZE len;
  PN digits[0];
};

//
// a file is wrapper around a file
// descriptor, non-volatile but mutable.
//
struct PNFile {
  PN_OBJECT_HEADER
  FILE *stream;
  PN path;
  PN mode;
};

typedef PN (*PN_F)(Potion *, PN, PN, ...);

//
// a closure is an anonymous function,
// non-volatile.
//
struct PNClosure {
  PN_OBJECT_HEADER
  PN_F method;
  PN sig;
  PN_SIZE extra;
  PN data[0];
};

//
// a prototype is compiled source code,
// non-volatile.
//
struct PNProto {
  PN_OBJECT_HEADER
  PN source; // program name or enclosing scope
  PN sig;    // argument signature
  PN stack;  // size of the stack
  PN locals; // local variables
  PN upvals; // variables in upper scopes
  PN values; // numbers, strings, etc.
  PN protos; // nested closures
  PN_SIZE localsize, upvalsize;
  PN asmb;   // assembled instructions
};

//
// a tuple is an ordered list,
// volatile.
//
struct PNTuple {
  PN_OBJECT_HEADER
  PN_SIZE len;
  PN *set;
};

//
// a weak ref is used for upvals, it acts as
// a memory slot, non-volatile but mutable.
//
struct PNWeakRef {
  PN_OBJECT_HEADER
  PN data;
};

// the potion type is the 't' in the vtable tuple (m,t)
static inline PNType potion_type(PN obj) {
  if (PN_IS_NUM(obj))  return PN_TNUMBER;
  if (PN_IS_BOOL(obj)) return PN_TBOOLEAN;
  if (PN_IS_NIL(obj))  return PN_TNIL;
  return PN_VTYPE(obj & PN_REF_MASK);
}

// quick access to either PNString or PNByte pointer
static inline char *potion_str_ptr(struct PNString *s) {
  return s->vt == PN_TBYTES ? ((struct PNBytes *)s)->chars :
    s->chars;
}

//
// the jit
//
#define OP_MAX 64

typedef void (*OP_F)(struct PNJitAsm *, ...);

typedef struct {
  void (*setup)    (struct PNJitAsm *);
  void (*stack)    (struct PNJitAsm *, long);
  void (*registers)(struct PNJitAsm *, long);
  void (*local)    (struct PNJitAsm *, long, long);
  void (*upvals)   (struct PNJitAsm *, long, int);
  void (*jmpedit)  (struct PNJitAsm *, unsigned char *, int);
  OP_F op[OP_MAX];
  void (*finish)   (struct PNJitAsm *);
} PNTarget;

//
// the interpreter
// (one per thread, houses its own garbage collector)
//
struct Potion_State {
  PN_OBJECT_HEADER
  PNTarget targets[POTION_TARGETS];
  PN strings; /* table of all strings */
  unsigned int next_string_id;
  PN lobby; /* root namespace */
  PN_FLEX(vts, PN); /* built in types */
  PN source; /* temporary ast node */
  PN unclosed; /* used by parser for named block endings */
  int dast; /* parsing depth */
  int xast; /* extra ast allocations */
  struct PNMemory *mem; /* allocator/gc */
};

//
// the garbage collector
//
struct PNMemory {
  // the birth region
  volatile void *birth_lo, *birth_hi, *birth_cur;
  volatile void **birth_storeptr;
  volatile int birth_changedconstrank;

  // the old region (TODO: consider making the old region common to all threads)
  volatile void *old_lo, *old_hi, *old_cur;

  volatile struct PNFrame *frame;
  volatile int collecting, dirty, pass;
};

void potion_garbagecollect(struct PNMemory *, int, int);

// quick inline allocation
static inline void *potion_gc_alloc(Potion *P, int siz) {
  volatile void *res = 0;
  siz = PN_ALIGN(siz, 8); // force 64-bit alignment
  if (P->mem->dirty || (char *)P->mem->birth_cur + siz >= (char *)P->mem->birth_storeptr - 2)
    potion_garbagecollect(P->mem, siz + 4 * sizeof(double), 0);
  res = P->mem->birth_cur;
  P->mem->birth_cur = (char *)res + siz;
  return (void *)res;
}

static inline void *potion_gc_calloc(Potion *P, int siz) {
  void *res = potion_gc_alloc(P, siz);
  memset(res, 0, siz);
  return res;
}

//
// method caches
// (more great stuff from ian piumarta)
//
// TODO: make the ICACHE per-thread
//
#define potion_send_dyn(RCV, MSG, ARGS...) ({ \
    PN r = (PN)(RCV); \
    PN c = potion_bind(P, r, (MSG)); \
    ((struct PNClosure *)c)->method(P, c, r, ##ARGS); \
  })
#if ICACHE
#define potion_send(RCV, MSG, ARGS...) ({ \
    PN r = (PN)(RCV); \
    static PNType prevVT = 0; \
    static int prevTN = 0; \
    static PN closure = 0; \
    PNType thisVT = potion_type(r); \
    int thisTN = PN_FLEX_SIZE(P->vts); \
    thisVT == prevVT && prevTN == thisTN ? closure : \
      (prevVT = thisVT, prevTN = thisTN, closure = potion_bind(P, r, (MSG))); \
    ((struct PNClosure *)closure)->method(P, closure, r, ##ARGS); \
  })
#else
#define potion_send potion_send_dyn
#endif

#define potion_method(RCV, MSG, FN, SIG) \
  potion_send(RCV, PN_def, potion_str(P, MSG), PN_FUNC(FN, SIG))

extern PN PN_allocate, PN_break, PN_call, PN_compile, PN_continue,
   PN_def, PN_delegated, PN_else, PN_elsif, PN_if,
   PN_lookup, PN_loop, PN_print, PN_return, PN_string, PN_while;

//
// the Potion functions
//
Potion *potion_create();
void potion_destroy(Potion *);
PNType potion_kind_of(PN);
PN potion_str(Potion *, const char *);
PN potion_str2(Potion *, char *, size_t);
PN potion_byte_str(Potion *, const char *);
PN potion_bytes(Potion *, size_t);
PN_SIZE pn_printf(Potion *, PN, const char *, ...);
void potion_bytes_obj_string(Potion *, PN, PN);
PN potion_bytes_append(Potion *, PN, PN, PN);
PN potion_allocate(Potion *, PN, PN, PN);
void potion_release(Potion *, PN);
PN potion_def_method(Potion *P, PN, PN, PN, PN);
PN potion_type_new(Potion *, PNType, PN);
void potion_type_func(PN, PN_F);
PN potion_obj_call(Potion *, PN, PN, ...);
PN potion_delegated(Potion *, PN, PN);
PN potion_call(Potion *, PN, PN_SIZE, PN * volatile);
PN potion_lookup(Potion *, PN, PN, PN);
PN potion_bind(Potion *, PN, PN);
PN potion_closure_new(Potion *, PN_F, PN, PN_SIZE);
PN potion_callcc(Potion *, PN, PN);
PN potion_ref(Potion *, PN);
PN potion_sig(Potion *, char *);
PN potion_decimal(Potion *, int, int, char *);
PN potion_pow(Potion *, PN, PN, PN);

PN potion_tuple_empty(Potion *);
PN potion_tuple_with_size(Potion *, PN_SIZE);
PN potion_tuple_new(Potion *, PN);
PN potion_tuple_push(Potion *, PN, PN);
PN_SIZE potion_tuple_push_unless(Potion *, PN, PN);
PN_SIZE potion_tuple_find(Potion *, PN, PN);
PN potion_tuple_at(Potion *, PN, PN, PN);
PN potion_table_set(Potion *, PN, PN, PN);
PN potion_table_at(Potion *, PN, PN, PN);
PN potion_source_compile(Potion *, PN, PN, PN, PN);
PN potion_source_load(Potion *, PN, PN);
PN potion_source_dump(Potion *, PN, PN);

Potion *potion_gc_boot();
void potion_lobby_init(Potion *);
void potion_object_init(Potion *);
void potion_primitive_init(Potion *);
void potion_num_init(Potion *);
void potion_str_hash_init(Potion *);
void potion_str_init(Potion *);
void potion_table_init(Potion *);
void potion_source_init(Potion *);
void potion_compiler_init(Potion *);
void potion_vm_init(Potion *);

PN potion_any_is_nil(Potion *, PN, PN);

PN potion_parse(Potion *, PN);
PN potion_vm(Potion *, PN, PN, PN_SIZE, PN * volatile);
PN potion_eval(Potion *, const char *);
PN potion_run(Potion *, PN);
PN_F potion_jit_proto(Potion *, PN, PN);

#endif
