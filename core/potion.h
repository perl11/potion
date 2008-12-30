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

//
// types
//
typedef unsigned long PN;
typedef unsigned int PNType;
typedef struct Potion_State Potion;

struct PNObject;
struct PNString;
struct PNClosure;
struct PNProto;
struct PNTuple;
struct PNGarbage;

#define PN_TNIL         0
#define PN_TNUMBER      1
#define PN_TBOOLEAN     2
#define PN_TSTRING      3
#define PN_TTABLE       4
#define PN_TCLOSURE     5
#define PN_TTUPLE       6
#define PN_TSTATE       7
#define PN_TFILE        8
#define PN_TOBJECT      9
#define PN_TVTABLE      10
#define PN_TSOURCE      11
#define PN_TBYTES       12
#define PN_TPROTO       13
#define PN_TUSER        14

#define PN_TYPE(x)      potion_type((PN)(x))
#define PN_VTYPE(x)     (((struct PNObject *)(x))->vt)
#define PN_VTABLE(t)    (P->vts[t])

#define PN_NONE         ((PN)-1)
#define PN_NIL          ((PN)0)
#define PN_TRUE         ((PN)2)
#define PN_FALSE        ((PN)4)
#define PN_EMPTY        ((PN)6)
#define PN_PRIMITIVE    7

#define PN_TEST(v)      (((PN)(v) & ~PN_NIL) != 0)
#define PN_IS_NIL(v)    ((PN)(v) == PN_NIL)
#define PN_IS_BOOL(v)   ((PN)(v) == PN_FALSE || (PN)(v) == PN_TRUE)
#define PN_IS_NUM(v)    ((PN)(v) & PN_NUM_FLAG)
#define PN_IS_TUPLE(v)  ((PN)(v) & PN_TUPLE_FLAG)
#define PN_IS_STR(v)    (PN_TYPE(v) == PN_TSTRING)
#define PN_IS_TABLE(v)  (PN_TYPE(v) == PN_TTABLE)
#define PN_IS_CLOSURE(v) (PN_TYPE(v) == PN_TCLOSURE)
#define PN_IS_PROTO(v)   (PN_TYPE(v) == PN_TPROTO)

#define PN_NUM_FLAG     0x01
#define PN_TUPLE_FLAG   0x06

#define PN_NUM(i)       ((PN)(((long)(i))<<1 | PN_NUM_FLAG))
#define PN_INT(x)       (((long)(x))>>1)
#define PN_STR_PTR(x)   ((struct PNString *)(x))->chars
#define PN_STR_LEN(x)   ((struct PNString *)(x))->len
#define PN_FUNC(f)      potion_closure_new(P, (imp_t)f, 0, 0)
#define PN_GB(x,o,m)    (x).next = o; (x).marked = m

#define PN_SET_TUPLE(t) (((PN)t)|PN_TUPLE_FLAG)
#define PN_GET_TUPLE(t) ((struct PNTuple *)(((PN)t)^PN_TUPLE_FLAG))
#define PN_TUPLE_LEN(t) (t == PN_EMPTY ? 0 : PN_GET_TUPLE(t)->len)
#define PN_TUPLE_AT(t, n) PN_GET_TUPLE(t)->set[n]
#define PN_TUPLE_EACH(T, I, V, B) \
  if (T != PN_EMPTY) { \
    struct PNTuple *__t##V = PN_GET_TUPLE(T); \
    unsigned long I; \
    for (I = 0; I < __t##V->len; I++) { \
      PN V = __t##V->set[I]; \
      B \
    } \
  }

struct PNGarbage {
  struct PNGarbage *next;
  unsigned char marked;
};

#define PN_OBJECT_HEADER \
  struct PNGarbage gb; \
  PNType vt;

#define PN_BOOT_OBJ_ALLOC(S, T, L) \
  ((S *)potion_allocate(P, 0, PN_VTABLE(T), PN_NUM((sizeof(S)-sizeof(struct PNObject))+(L))))
#define PN_OBJ_ALLOC(S, T, L) \
  ((S *)potion_send(PN_VTABLE(T), PN_allocate, PN_NUM((sizeof(S)-sizeof(struct PNObject))+(L))))

struct PNObject {
  PN_OBJECT_HEADER
  char data[0];
};

struct PNString {
  PN_OBJECT_HEADER
  unsigned int len;
  char chars[0];
};

struct PNFile {
  PN_OBJECT_HEADER
  FILE *stream;
  PN path;
  PN mode;
};

typedef PN (*imp_t)(Potion *P, PN closure, PN receiver, ...);

struct PNClosure {
  PN_OBJECT_HEADER
  imp_t method;
  PN sig;
  PN value;
};

struct PNProto {
  PN_OBJECT_HEADER
  PN source; // program name
  PN sig;    // argument signature
  PN stack;  // size of the stack
  PN locals; // local variables
  PN values; // numbers, strings, etc.
  PN protos; // nested closures
  PN asmb;   // assembled instructions
};

struct PNTuple {
  struct PNGarbage *next;
  unsigned long len;
  PN set[0];
};

// the potion type is the 't' in the vtable tuple (m,t)
static inline PNType potion_type(PN obj) {
  if (PN_IS_NUM(obj))  return PN_TNUMBER;
  if (obj == 0)        return PN_NIL;
  if (obj & PN_PRIMITIVE) {
    if (obj == PN_FALSE) return PN_TBOOLEAN;
    return (obj & PN_PRIMITIVE);
  }
  return PN_VTYPE(obj);
}

static inline int potion_is_ref(PN obj) {
  return (!((obj & PN_PRIMITIVE) || obj == 0));
}

//
// the interpreter
//
struct Potion_State {
  PN_OBJECT_HEADER
  PN strings; /* table of all strings */
  PN lobby; /* root namespace */
  PN *vts;
  int typen; /* number of actual types in circulation */
  int typea; /* type space allocated */ 
  PN source; /* temporary ast node */
  int xast; /* extra ast allocations */
};

//
// method caches
// (more great stuff from ian piumarta)
//
#if ICACHE
#define potion_send(RCV, MSG, ARGS...) ({ \
    PN r = (PN)(RCV); \
    static PNType prevVT = 0; \
    static int prevTN = 0; \
    static PN closure = 0; \
    PNType thisVT = potion_type(r); \
    int thisTN = P->typen; \
    thisVT == prevVT && prevTN == thisTN ? closure : \
      (prevVT = thisVT, prevTN = thisTN, closure = potion_bind(P, r, (MSG))); \
    ((struct PNClosure *)closure)->method(P, closure, r, ##ARGS); \
  })
#else
#define potion_send(RCV, MSG, ARGS...) ({ \
    PN r = (PN)(RCV); \
    PN c = potion_bind(P, r, (MSG)); \
    ((struct PNClosure *)c)->method(P, c, r, ##ARGS); \
  })
#endif

#define potion_method(RCV, MSG, FN, SIG) \
  potion_send(RCV, PN_def, potion_str(P, MSG), PN_FUNC(FN))

#if MCACHE
struct PNMcache {
  PN vt;
  PN selector;
  PN closure;
} potion_mcache[8192];
#endif

PN PN_allocate, PN_compile, PN_def, PN_delegated, PN_inspect, PN_lookup;

//
// the Potion functions
//
Potion *potion_create();
void potion_destroy(Potion *);
PN potion_str(Potion *, const char *);
PN potion_str2(Potion *, char *, size_t);
PN potion_bytes(Potion *, size_t);
PN potion_allocate(Potion *, PN, PN, PN);
PN potion_def_method(Potion *P, PN, PN, PN, PN);
PN potion_type_new(Potion *, PNType, PN);
PN potion_delegated(Potion *, PN, PN);
PN potion_lookup(Potion *, PN, PN, PN);
PN potion_bind(Potion *, PN, PN);
PN potion_closure_new(Potion *, imp_t, PN, PN);

inline PN potion_tuple_with_size(Potion *, unsigned long);
inline PN potion_tuple_new(Potion *, PN);
inline PN potion_tuple_push(Potion *, PN, PN);
inline unsigned long potion_tuple_put(Potion *, PN *, PN);
inline unsigned long potion_tuple_find(Potion *, PN, PN);
PN potion_source_compile(Potion *, PN, PN, PN, PN);
PN potion_source_load(Potion *, PN, PN);
PN potion_source_dump(Potion *P, PN, PN);

void potion_lobby_init(Potion *);
void potion_primitive_init(Potion *);
void potion_num_init(Potion *);
void potion_str_hash_init(Potion *);
void potion_str_init(Potion *);
void potion_table_init(Potion *);
void potion_source_init(Potion *);
void potion_compiler_init(Potion *);

PN potion_parse(Potion *, PN);
PN potion_vm(Potion *, PN);
void potion_run();

#endif
