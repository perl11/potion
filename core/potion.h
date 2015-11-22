/** \file potion.h
  The potion API

  (c) 2008 why the lucky stiff, the freelance professor
  (c) 2013-2015 perl11.org */
/**
\mainpage potion

\see doc/INTERNALS.md

Everything is an object.
Every object is a closure (lambda), even values, variables, classes, types, metaclasses, ...
It's essentially a LISP-1.

PN is either an immediate tagged value for int, bool and nil,
or a ptr to an object.

ops use a three-addresss layout: "dest = op src what" in a single word

The root class is P->lobby, which holds global values and methods.

# Methods use as first three params:
 - Potion *P - the global interpeter singleton (not threaded yet)
 - PN cl     - bound closure, the optional parent enclosing block for the lexical environment.
               This gives shared methods access to any differentiating data
               that might be stored in the closure.
 - PN self   - the object we are acting on

and optionally args, statically typed via signature strings.

# Potion method signatures:

  name=one-char type.
  - o PN (any)
  - N Num
  - & Closure
  - S String
*/
#ifndef POTION_H
#define POTION_H

#define POTION_SIG      "p\07\10n"
#define POTION_VMID     0x79

#define POTION_X86      0
#define POTION_PPC      1
#define POTION_ARM      2

#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include "config.h"
#if POTION_WIN32
# include <sys/stat.h>
# define DLLEXPORT __declspec(dllexport)
#else
# define DLLEXPORT
#endif

#define _XSTR(s) _STR(s)
#define _STR(s)  #s

#if HAVE_ASAN_ATTR
# define ATTRIBUTE_NO_ADDRESS_SAFETY_ANALYSIS __attribute__((no_address_safety_analysis))
#else
# define ATTRIBUTE_NO_ADDRESS_SAFETY_ANALYSIS
#endif
#ifdef DEBUG
# ifndef YY_DEBUG
#  define YY_DEBUG
# endif
#endif

//
// types
//
typedef unsigned long _PN;
typedef unsigned int PN_SIZE, PNType, PNUniq;
typedef struct Potion_State Potion;
typedef volatile _PN PN;

struct PNObject;
struct PNFwd;
struct PNData;
struct PNString;
struct PNBytes;
struct PNDouble;
struct PNClosure;
struct PNProto;
struct PNTuple;
struct PNWeakRef;
struct PNError;
struct PNCont;
struct PNMemory;
struct PNVtable;

/* patch for issue #1 supplied by joeatwork */
#ifndef __WORDSIZE
# if PN_SIZE_T == 8
#  define __WORDSIZE 64
# else
#  define __WORDSIZE 32
# endif
#endif

#define PN_TNIL         0x250000    /// NIL is magic 0x250000 (type 0)
#define PN_TNUMBER      (1+PN_TNIL) /// TNumber is Int, or if fwd'd a TDouble
#define PN_TBOOLEAN     (2+PN_TNIL)
#define PN_TDOUBLE      (3+PN_TNIL) //is a Number, no arbitrary prec. yet
#define PN_TINTEGER     (4+PN_TNIL) //is a Number
#define PN_TSTRING      (5+PN_TNIL)
#define PN_TWEAK        (6+PN_TNIL)
#define PN_TCLOSURE     (7+PN_TNIL)
#define PN_TTUPLE       (8+PN_TNIL)
#define PN_TSTATE       (9+PN_TNIL)
#define PN_TFILE        (10+PN_TNIL) //0a
#define PN_TOBJECT      (11+PN_TNIL) //0b
#define PN_TVTABLE      (12+PN_TNIL) //0c
#define PN_TSOURCE      (13+PN_TNIL) //0d
#define PN_TBYTES       (14+PN_TNIL) //0e
#define PN_TPROTO       (15+PN_TNIL) //0f
#define PN_TLOBBY       (16+PN_TNIL) //10
#define PN_TTABLE       (17+PN_TNIL) //11
#define PN_TLICK        (18+PN_TNIL) //12
#define PN_TFLEX        (19+PN_TNIL) //13
#define PN_TSTRINGS     (20+PN_TNIL) //14
#define PN_TERROR       (21+PN_TNIL) //15
#define PN_TCONT        (22+PN_TNIL) //16
#define PN_TUSER        (23+PN_TNIL) //17

#define vPN(t)          struct PN##t * volatile
#define PN_TYPE(x)      potion_type((PN)(x))
#define PN_VTYPE(x)     (((struct PNObject *)(x))->vt)
#define PN_TYPE_ID(t)   ((t)-PN_TNIL)
#define PN_VTABLE(t)    (PN_FLEX_AT(P->vts, PN_TYPE_ID(t)))
#define PN_TYPECHECK(t) (t >= PN_TNIL && PN_TYPE_ID(t) < PN_FLEX_SIZE(P->vts))

#define PN_NIL          ((PN)0)
#define PN_ZERO         ((PN)1)   //i.e. PN_NUM(0)
#define PN_FALSE        ((PN)2)
#define PN_TRUE         ((PN)6)
#define PN_PRIMITIVE    7
#define PN_REF_MASK     ~7
#define PN_NONE         ((PN_SIZE)-1)
#define POTION_FWD      0xFFFFFFFE
#define POTION_COPIED   0xFFFFFFFF

#define NIL_NAME        "nil" // "undef" in p2
#define NILKIND_NAME    "NilKind"

#define PN_FINTEGER     1
#define PN_FBOOLEAN     2
//Beware: TEST(PN_NUM(0)) vs BOOL(0<1) i.e. test1(1) vs test(1)
#define PN_TEST(v)     ((PN)(v) != PN_FALSE && (PN)(v) != PN_NIL)
///\class PNBoolean
/// From cmp (x<y) to immediate object (no struct) 0x...2. PN_TRUE (0x6) or PN_FALSE (0x2)
#define PN_BOOL(v)      ((v) ? PN_TRUE : PN_FALSE)
#define PN_IS_PTR(v)    (!PN_IS_INT(v) && ((PN)(v) & PN_REF_MASK))
#define PN_IS_NIL(v)    ((PN)(v) == PN_NIL)
#define PN_IS_BOOL(v)   ((PN)(v) & PN_FBOOLEAN)
#define PN_IS_INT(v)    ((PN)(v) & PN_FINTEGER)
#define PN_IS_DBL(v)    (PN_IS_PTR(v) && ({PNType _t = potion_qptr_type((PN)v); _t == PN_TNUMBER || _t == PN_TDOUBLE;}))
#define PN_IS_NUM(v)    (PN_IS_INT(v) || PN_IS_DBL(v))
#define PN_IS_TUPLE(v)  (PN_IS_PTR(v) && (potion_ptr_type((PN)v) == PN_TTUPLE))
#define PN_IS_STR(v)    (PN_IS_PTR(v) && (potion_ptr_type((PN)v) == PN_TSTRING))
#define PN_IS_TABLE(v)  (PN_IS_PTR(v) && (potion_ptr_type((PN)v) == PN_TTABLE))
#define PN_IS_CLOSURE(v) (PN_IS_PTR(v) && (potion_ptr_type((PN)v) == PN_TCLOSURE))
#define PN_IS_PROTO(v)   (PN_IS_PTR(v) && (potion_ptr_type((PN)v) == PN_TPROTO))
#define PN_IS_REF(v)     (PN_IS_PTR(v) && (potion_ptr_type((PN)v) == PN_TWEAK))
#define PN_IS_METACLASS(v) (((struct PNVtable *)v)->meta == PN_NIL)
#define PN_IS_FFIPTR(p)  ((PN_IS_PTR(p) && !(p >= (_PN)P->mem && p <= (_PN)P->mem->birth_hi)) \
			  || (!PN_IS_PTR(p) && p > (_PN)P->mem->birth_hi))

#define PN_CHECK_STR(obj)  if (!PN_IS_STR(obj)) return potion_type_error_want(P, ""#obj, (PN)obj, "String")
#define PN_CHECK_STRB(obj) if (!PN_IS_STR(obj) || (PN_TYPE(obj) != PN_TBYTES)) return potion_type_error_want(P, ""#obj, (PN)obj, "String or Bytes")
#define PN_CHECK_NUM(obj)  if (!PN_IS_NUM(obj)) return potion_type_error_want(P, ""#obj, (PN)obj, "Number")
#define PN_CHECK_INT(obj)  if (!PN_IS_INT(obj)) return potion_type_error_want(P, ""#obj, (PN)obj, "Integer")
#define PN_CHECK_DBL(obj)  if (!PN_IS_DBL(obj)) return potion_type_error_want(P, ""#obj, (PN)obj, "Double")
#define PN_CHECK_BOOL(obj) if (!PN_IS_BOOL(obj)) return potion_type_error_want(P, ""#obj, (PN)obj, "Bool")
#define PN_CHECK_TUPLE(obj)   if (!PN_IS_TUPLE(obj)) return potion_type_error_want(P, ""#obj, (PN)obj, "Tuple")
#define PN_CHECK_CLOSURE(obj) if (!PN_IS_CLOSURE(obj)) return potion_type_error_want(P, ""#obj, (PN)obj, "Closure")
//exact only. TODO check derived types, parents and mixins via bind
#define PN_CHECK_TYPE(obj,type) if (type != PN_TYPE(obj)) return potion_type_error(P, (PN)obj)
#ifdef DEBUG
#define DBG_CHECK_TYPE(obj,type) PN_CHECK_TYPE(obj,type)
#define DBG_CHECK_NUM(obj) PN_CHECK_NUM(obj)
#define DBG_CHECK_INT(obj) PN_CHECK_INT(obj)
#define DBG_CHECK_DBL(obj) PN_CHECK_DBL(obj)
#define DBG_CHECK_TUPLE(obj) PN_CHECK_TUPLE(obj)
#else
#define DBG_CHECK_TYPE(obj,type)
#define DBG_CHECK_NUM(obj)
#define DBG_CHECK_INT(obj)
#define DBG_CHECK_DBL(obj)
#define DBG_CHECK_TUPLE(obj)
#endif

///\class PNInteger
/// Either a PN_INT immediate object (no struct) 0x...1
///        Integer: 31bit/63bit shifted off the last 1 bit
/// or a PNDouble double.
///\see PN_NUM (int to obj), PN_INT (obj to int), PN_IS_INT (is num obj?)
#define PN_NUM(i)       ((PN)((((long)(i))<<1) + PN_FINTEGER))
#define PN_INT(x)       ((long)((long)(x))>>1)
#define PN_DBL(num)     (PN_IS_INT(num) ? (double)PN_INT(num) : ((struct PNDouble *)num)->value)
typedef _PN (*PN_F)(Potion *, PN, PN, ...);
#define PN_PREC 16
#define PN_RAND()       PN_NUM(potion_rand_int())
#define PN_STR(x)       potion_str(P, x)
#define PN_STRN(x, l)   potion_str2(P, x, l)
#define PN_STRCAT(a, b) potion_strcat(P, (a), (b))
#define PN_STR_PTR(x)   potion_str_ptr(x)                     ///<\memberof PNString \memberof PNBytes
#define PN_STR_LEN(x)   ((struct PNString *)(x))->len         ///<\memberof PNString
#define PN_STR_B(x)     potion_bytes_string(P, PN_NIL, x)     ///<\memberof PNBytes
#define PN_CLOSURE(x)   ((struct PNClosure *)(x))             ///<\memberof PNClosure
#define PN_CLOSURE_F(x) ((struct PNClosure *)(x))->method     ///<\memberof PNClosure
#define PN_PROTO(x)     ((struct PNProto *)(x))               ///<\memberof PNProto
#define PN_FUNC(f, s)   potion_closure_new(P, (PN_F)f, potion_sig(P, s), 0) ///<\memberof PNClosure
#define PN_DEREF(x)     ((struct PNWeakRef *)(x))->data       ///<\memberof PNWeakRef
#define PN_DATA(x)      ((struct PNData *)(x))->data          ///<\memberof PNData
#define PN_TOUCH(x)     potion_gc_update(P, (PN)(x))
#define PN_ALIGN(o, x)   (((((o) - 1) / (x)) + 1) * (x))

#define PN_OBJECT_HEADER \
  PNType vt; \
  PNUniq uniq

#define PN_FLEX(N, T)    typedef struct { PN_OBJECT_HEADER; PN_SIZE len; PN_SIZE siz; T ptr[]; } N
#define PN_FLEX_AT(N, I) ((PNFlex *)(N))->ptr[I]
#define PN_FLEX_SIZE(N)  ((PNFlex *)(N))->len

#if PN_SIZE_T == 4
#define PN_NUMHASH(x)   x
#else
#define PN_NUMHASH(x)   (PNUniq)((x)>>33^(x)^(x)<<11)
#endif
#define PN_UNIQ(x)      (PN_IS_PTR(x) ? ((struct PNObject *)(x))->uniq : PN_NUMHASH(x))

#define AS_STR(x)       PN_STR_PTR(potion_send(x, PN_string))
#ifdef DEBUG
#define DBG_t(...) \
  if (P->flags & DEBUG_TRACE) fprintf(stderr, __VA_ARGS__)
#define DBG_v(...) \
  if (P->flags & DEBUG_VERBOSE) fprintf(stderr, __VA_ARGS__)
#define DBG_vt(...) \
  if (P->flags & DEBUG_VERBOSE && P->flags & DEBUG_TRACE) fprintf(stderr, __VA_ARGS__)
#define DBG_vi(...) \
  if (P->flags & DEBUG_VERBOSE && P->flags & DEBUG_INSPECT) fprintf(stderr, __VA_ARGS__)
#define DBG_c(...) \
  if (P->flags & DEBUG_COMPILE) fprintf(stderr, __VA_ARGS__)
#else
#define DBG_t(...)
#define DBG_v(...)
#define DBG_vt(...)
#define DBG_vi(...)
#define DBG_c(...)
#endif

#define PN_IS_EMPTY(T)  (PN_TUPLE_LEN(T) == 0)           ///<\memberof PNTuple
#define PN_TUP0()       potion_tuple_empty(P)            ///<\memberof PNTuple
#define PN_TUP(X)       potion_tuple_new(P, X)           ///<\memberof PNTuple
#define PN_PUSH(T, X)   potion_tuple_push(P, T, (PN)X)   ///<\memberof PNTuple
#define PN_SHIFT(T)     potion_tuple_shift(P, 0, T)      ///<\memberof PNTuple
#define PN_GET(T, X)    potion_tuple_find(P, T, X)       ///<\memberof PNTuple
#define PN_PUT(T, X)    potion_tuple_push_unless(P, T, X) ///<\memberof PNTuple
#define PN_GET_TUPLE(t) ((struct PNTuple *)potion_fwd((PN)t)) ///<\memberof PNTuple
#define PN_TUPLE_LEN(t) PN_GET_TUPLE(t)->len             ///<\memberof PNTuple
#define PN_TUPLE_AT(t, n) PN_GET_TUPLE(t)->set[n]        ///<\memberof PNTuple
///\memberof PNTuple
#define PN_TUPLE_COUNT(T, I, B) ({ \
    struct PNTuple * volatile __t##I = PN_GET_TUPLE(T); \
    if (__t##I->len != 0) { \
      PN_SIZE I; \
      for (I = 0; I < __t##I->len; I++) B \
    } \
  })
///\memberof PNTuple
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

///
/// standard objects act like C structs
/// the fields are defined by the type
/// and it's a fixed size, not volatile.
///
struct PNObject {
  PN_OBJECT_HEADER;  ///< PNType vt; PNUniq uniq
  PN ivars[];
};

///
/// forwarding pointer (in case of
/// reallocation)
///
struct PNFwd {
  unsigned int fwd;
  PN_SIZE siz;
  PN ptr;
};

///
/// struct to wrap arbitrary data that
/// we may want to allocate from Potion.
///
/// constructor: potion_data_alloc()
/// accessor: PN_DATA()
///
struct PNData {
  PN_OBJECT_HEADER;  ///< PNType vt; PNUniq uniq
  PN_SIZE siz;
  char data[];
};

///
/// strings are immutable UTF-8, the ID is
/// incremental and they may be garbage
/// collected. (non-volatile)
///
struct PNString {
  PN_OBJECT_HEADER;  ///< PNType vt; PNUniq uniq
  PN_SIZE len;
  char chars[];
};

///
/// byte strings are raw character data,
/// volatile, may be appended/changed.
///
struct PNBytes {
  PN_OBJECT_HEADER;  ///< PNType vt; PNUniq uniq
  PN_SIZE len;
  PN_SIZE siz;
  char chars[];
};

/// PN_MANTISSA the last part of ->real
#define PN_MANTISSA(d, n) d->real[1 + n]

///
/// doubles are floating point numbers
/// stored as binary data. immutable.
///
struct PNDouble {
  PN_OBJECT_HEADER;  ///< PNType vt; PNUniq uniq
  double value;
};

///
/// a file is wrapper around a file
/// descriptor, non-volatile but mutable.
///
struct PNFile {
  PN_OBJECT_HEADER;  ///< PNType vt; PNUniq uniq
  int fd;
  PN path;
  mode_t mode;
};

///
/// a closure is an anonymous function, without closed values, \see PNProto
/// non-volatile.
///
struct PNClosure {
  PN_OBJECT_HEADER;  ///< PNType vt; PNUniq uniq
  PN_F method;
  PN sig;            ///< signature PNTuple
  int arity;         ///< cached number of declared args, including optional
  int minargs;       ///< cached number of mandatory args, without optional
  PN_SIZE extra;     ///< 0 or 1 if has code attached at data
  PN name;           /// PNString
  PN data[];        ///< code
};

///
/// An AST fragment, non-volatile.
///
enum PN_AST {
  AST_CODE,
  AST_VALUE,
  AST_ASSIGN,
  AST_NOT,
  AST_OR,
  AST_AND,
  AST_CMP,
  AST_EQ,
  AST_NEQ,
  AST_GT,
  AST_GTE,
  AST_LT,
  AST_LTE,
  AST_PIPE,
  AST_CARET,
  AST_AMP,
  AST_WAVY,
  AST_BITL,
  AST_BITR,
  AST_PLUS,
  AST_MINUS,
  AST_INC,
  AST_TIMES,
  AST_DIV,
  AST_REM,
  AST_POW,
  AST_MSG,
  AST_PATH,
  AST_QUERY,
  AST_PATHQ,
  AST_EXPR,
  AST_LIST, /* for TABLE (=,..) and TUPLE (,...) */
  AST_BLOCK,
  AST_LICK, /* [...] */
  AST_PROTO,
  AST_DEBUG
};
struct PNSource {
  PN_OBJECT_HEADER;  	///< PNType vt; PNUniq uniq
  enum PN_AST part;     ///< AST type, avoid -Wswitch (aligned access: 4+4+8+4+24)
  struct PNSource * volatile a[3];///< PNTuple of 1-3 kids, \see ast.c
  // luxury for reflection only: debugging, error messages
  struct {
#if PN_SIZE_T != 8
    PNType fileno:16;
    PNType lineno:16;
#else
    PNType fileno:32;
    PNType lineno:32;
#endif
  } loc;                ///< bitfield of fileno and lineno
  PN line;              ///< PNString of src line
};

///
/// a prototype is compiled source code, a closure block (lambda)
/// non-volatile.
///
struct PNProto {
  PN_OBJECT_HEADER;  ///< PNType vt; PNUniq uniq
  PN source; ///< program name or enclosing scope
  PN sig;    ///< argument signature
  PN stack;  ///< size of the stack
  PN paths;  ///< paths (instance variables)
  PN locals; ///< local variables
  PN upvals; ///< variables in upper scopes
  PN values; ///< numbers, strings, etc.
  PN protos; ///< nested closures
  PN debugs; ///< tree parts
  PN tree;   ///< abstract syntax tree
  PN_SIZE pathsize, localsize, upvalsize;
  PN asmb;   ///< assembled instructions
  PN_F jit;  ///< jit function pointer
  int arity; ///< cached sig arity (number of args)
  PN name;   ///< PNString
};

///
/// a tuple is an array of PNs.
/// volatile.
///
struct PNTuple {
  PN_OBJECT_HEADER;  ///< PNType vt; PNUniq uniq
  PN_SIZE len;
  PN_SIZE alloc;     ///< overallocate a bit
  PN set[];
};

///
/// a weak ref is used for upvals, it acts as
/// a memory slot, non-volatile but mutable.
///
struct PNWeakRef {
  PN_OBJECT_HEADER;  ///< PNType vt; PNUniq uniq
  PN data;
};

///
/// an error, including a description,
/// file location, a brief excerpt.
///
struct PNError {
  PN_OBJECT_HEADER;  ///< PNType vt; PNUniq uniq
  PN parent;
  PN message;
  PN line, chr;
  PN excerpt;
};

///
/// a lick is a unit of generic tree data.
///
struct PNLick {
  PN_OBJECT_HEADER;  ///< PNType vt; PNUniq uniq
  PN name;  ///< PNString
  PN attr;  ///< PN
  PN inner; ///< "licks" PNTuple or "text" PNString member
};

///
/// a continuation saves the stack and all
/// stack pointers.
///
struct PNCont {
  PN_OBJECT_HEADER;  ///< PNType vt; PNUniq uniq
  PN_SIZE len;
  PN stack[];  // [0] = head of potion stack
               // [1] = current %rsp
               // [2] = current %rbp
               // [3+] = full stack dump, ascending
};

/// the potion type is the 't' in the vtable tuple (m,t)
static inline PN potion_fwd(PN);

/// either immediate (NUM,BOOL,NIL) or a fwd
static inline PNType potion_type(PN obj) {
  if (PN_IS_INT(obj))  return PN_TNUMBER;
  if (PN_IS_BOOL(obj)) return PN_TBOOLEAN;
  if (PN_IS_NIL(obj))  return PN_TNIL;
#if 1
  while (1) {
    struct PNFwd *o = (struct PNFwd *)obj;
    if (o->fwd != POTION_FWD)
      return ((struct PNObject *)o)->vt;
    obj = o->ptr;
  }
#else
  return ((struct PNObject *)potion_fwd(obj))->vt;
#endif
}

/// if obj is guaranteed to be a PTR
static inline PNType potion_ptr_type(PN obj) {
  while (1) {
    struct PNFwd *o = (struct PNFwd *)obj;
    if (o->fwd != POTION_FWD)
      return ((struct PNObject *)o)->vt;
    obj = o->ptr;
  }
}
/// PN_QUICK_FWD - doing a single fwd check after a possible realloc
#define PN_QUICK_FWD(t, obj) \
  if (((struct PNFwd *)obj)->fwd == POTION_FWD) \
    obj = (t)(((struct PNFwd *)obj)->ptr);

/// if obj is guaranteed to be a PTR
/// fwd only once, no loop.
static inline PNType potion_qptr_type(PN obj) {
  PN_QUICK_FWD(PN, obj);
  return ((struct PNObject *)obj)->vt;
}

/// resolve forwarding pointers for mutable types (PNTuple, PNBytes, etc.)
static inline PN potion_fwd(PN obj) {
  while (PN_IS_PTR(obj) && ((struct PNFwd *)obj)->fwd == POTION_FWD)
    obj = ((struct PNFwd *)obj)->ptr;
  return obj;
}

/// quick access to either PNString or PNByte pointer
static inline char *potion_str_ptr(PN s) {
  if (((struct PNString *)s)->vt == PN_TSTRING)
    return ((struct PNString *)s)->chars;
  s = potion_fwd(s);
  return ((struct PNBytes *)s)->chars;
}

PN_FLEX(PNFlex, PN);
//PN_FLEX(PNAsm, unsigned char);
typedef struct { PN_OBJECT_HEADER; PN_SIZE len; PN_SIZE siz; unsigned char ptr[]; } PNAsm;

///
/// the jit
///
#define OP_MAX 50 // OP_DEBUG+1 was 64, statically allocated in Potion interpreter

typedef void (*OP_F)(Potion *P, struct PNProto *, PNAsm * volatile *, ...);

/// definition of the jit targets: x86, ppc, arm
typedef struct {
  void (*setup)    (Potion *, struct PNProto * volatile, PNAsm * volatile *);
  void (*stack)    (Potion *, struct PNProto * volatile, PNAsm * volatile *, long);
  void (*registers)(Potion *, struct PNProto * volatile, PNAsm * volatile *, long);
  void (*local)    (Potion *, struct PNProto * volatile, PNAsm * volatile *, long, long);
  void (*upvals)   (Potion *, struct PNProto * volatile, PNAsm * volatile *, long, long, int);
  void (*jmpedit)  (Potion *, struct PNProto * volatile, PNAsm * volatile *, unsigned char *, int);
  OP_F op[OP_MAX];
  void (*finish)   (Potion *, struct PNProto * volatile, PNAsm * volatile *);
  void (*mcache)   (Potion *, struct PNVtable * volatile, PNAsm * volatile *);
  void (*ivars)    (Potion *, PN, PNAsm * volatile *);
} PNTarget;

///
/// the interpreter
/// (one per thread, houses its own garbage collector)
///

typedef enum {
  EXEC_VM = 0,      ///< bytecode (switch or cgoto)
  EXEC_JIT = 1,     ///< JIT if detected at config-time (x86, ppc)
  EXEC_DEBUG = 2,   ///< -d: instrumented bytecode (line stepping) or just slow runloop?
  EXEC_CHECK = 3,   ///< -c stop after compilation
  EXEC_COMPILE = 4, ///< to bytecode (dumpbc)
  MAX_EXEC = 5      ///< sanity-check (possible stack overwrites by callcc)
} exec_mode_t;

// we combine the exclusive enum 0-4 for exec, then the exclusive syntax modes 16-18,
// and finally the inclusive debugging modes
#define EXEC_BITS 4 ///< 0-4

typedef enum {
  MODE_STD    = 1<<EXEC_BITS,   ///< 0x10 16 plain potion syntax
#ifdef P2
  MODE_P2     = MODE_STD+1,     ///< 0x11 17 use p2 extensions
  MODE_P6     = MODE_STD+2,     ///< 0x12 18 syntax p6. other via use syntax ""
#endif
  // room for registered syntax modules 18-63 (45 modules: p5, p6, sql, c, ...)
  MAX_SYNTAX  = (1<<(EXEC_BITS+2))-1     ///< sanity-check
} syntax_mode_t;

typedef enum {
  // exec + syntax + debug flags:
  DEBUG_INSPECT = 1<<(EXEC_BITS+2),	 // 0x0040
  DEBUG_VERBOSE = 1<<(EXEC_BITS+3)	 // 0x0080
#ifdef DEBUG
  ,
  DEBUG_TRACE   = 1<<(EXEC_BITS+4),  	 // 0x0100
  DEBUG_PARSE   = 1<<(EXEC_BITS+5),	 // 0x0200
  DEBUG_PARSE_VERBOSE = 1<<(EXEC_BITS+6),// 0x0400
  DEBUG_COMPILE = 1<<(EXEC_BITS+7),	 // 0x2000
  DEBUG_GC      = 1<<(EXEC_BITS+8),	 // 0x0800
  DEBUG_JIT     = 1<<(EXEC_BITS+9)	 // 0x1000
#endif
} Potion_Flags;

/// the global interpreter state P. currently singleton (not threads yet)
struct Potion_State {
  PN_OBJECT_HEADER;        ///< PNType vt; PNUniq uniq
  PNTarget target;         ///< the jit
  struct PNTable *strings; ///< table of all strings
  PN lobby;                ///< root namespace
  PNFlex * volatile vts;   ///< built in types
  Potion_Flags flags;      ///< vm flags: execution model and debug flags
  struct PNMemory *mem;    ///< allocator/gc
  PN call, callset;        ///< generic call and setter
  int prec;                ///< double precision

  //parser-only:
  PN input, source;        ///< parser input and output (AST)
  int yypos;               ///< parser buffer position
  PNAsm * volatile pbuf;   ///< parser buffer
  PN line;                 ///< currently parsed line (for debug)
  PN_SIZE fileno;          ///< currently parsed file
  PN unclosed;             ///< used by parser for named block endings
};

///
/// the garbage collector
///
struct PNMemory {
  /// the birth region
  volatile void *birth_lo, *birth_hi, *birth_cur;
  volatile void **birth_storeptr;

  /// the old region (TODO: consider making the old region common to all threads)
  volatile void *old_lo, *old_hi, *old_cur;

  volatile int collecting, dirty, pass, majors, minors;
  void *cstack;  ///< machine stack start
  void *protect; ///< end of protected memory
#ifdef DEBUG
  double time;
#endif
};

#define POTION_INIT_STACK(x) \
  PN __##x = 0x571FF; void *x = (void *)&__##x
void potion_garbagecollect(Potion *, int, int);
PN_SIZE potion_type_size(Potion *, const struct PNObject *);
unsigned long potion_rand_int();
double potion_rand_double();

/// quick inline allocation
static inline void *potion_gc_alloc(Potion *P, PNType vt, int siz) {
  struct PNMemory *M = P->mem;
  struct PNObject *res = 0;
  if (siz < sizeof(struct PNFwd))
    siz = sizeof(struct PNFwd);
  siz = PN_ALIGN(siz, 8); // force 64-bit alignment
  /*@ assert Value: mem_access: \valid_read(&M->dirty); */
  /*@ assert
    Value: ptr_comparison:
    \pointer_comparable((char *)M->birth_cur+siz,
                        (char *)M->birth_storeptr-2); */
  assert(M);
  assert(M->birth_cur);
  assert(M->birth_storeptr);
  if (M->dirty || (char *)M->birth_cur + siz >= (char *)M->birth_storeptr - 2)
    potion_garbagecollect(P, siz + 4 * sizeof(double), 0);
  res = (struct PNObject *)M->birth_cur;
  res->vt = vt;
  res->uniq = (PNUniq)potion_rand_int();
  M->birth_cur = (char *)res + siz;
  return (void *)res;
}

// TODO: mmap already inits to zero?
static inline void *potion_gc_calloc(Potion *P, PNType vt, int siz) {
  return potion_gc_alloc(P, vt, siz);
}

static inline void potion_gc_update(Potion *P, PN x) {
  struct PNMemory *M = P->mem;
  if ((x > (PN)M->birth_lo && x < (PN)M->birth_hi && (x < (PN)M || x >= (PN)M->protect)) ||
      x == (PN)M->birth_storeptr[1] ||
      x == (PN)M->birth_storeptr[2] ||
      x == (PN)M->birth_storeptr[3])
       return;
  *(M->birth_storeptr--) = (void *)x;
  if ((void **)M->birth_storeptr - 4 <= (void **)M->birth_cur)
    potion_garbagecollect(P, POTION_PAGESIZE, 0);
}

static inline void *potion_gc_realloc(Potion *P, PNType vt, struct PNObject * volatile obj, PN_SIZE sz) {
  void *dst = 0;
  PN_SIZE oldsz = 0;

  if (obj != NULL) {
    oldsz = potion_type_size(P, (const struct PNObject *)obj);
    if (oldsz >= sz)
      return (void *)obj;
  }

  dst = potion_gc_alloc(P, vt, sz);
  if (obj != NULL) {
    memcpy(dst, (void *)obj, oldsz);
    ((struct PNFwd *)obj)->fwd = POTION_FWD;
    ((struct PNFwd *)obj)->siz = oldsz;
    ((struct PNFwd *)obj)->ptr = (PN)dst;
    potion_gc_update(P, (PN)obj);
  }

  return dst;
}

///\memberof PNData
/// default constructor for PNData user-objects, as PN_TUSER type
/// \see also PNWeakRef to store external pointers only
static inline struct PNData *potion_data_alloc(Potion *P, int siz) {
  struct PNData *data = potion_gc_alloc(P, PN_TUSER, sizeof(struct PNData) + siz);
  data->siz = siz;
  return data;
}

///
/// internal errors
///
#define POTION_OK       0
#define POTION_NO_MEM   8910

///
/// method caches
/// (more great stuff from ian piumarta)
///
#define potion_send(RCV, MSG, ARGS...) ({ \
    PN r = (PN)(RCV); \
    PN c = potion_bind(P, r, (MSG)); \
    if (PN_IS_CLOSURE(c)) \
      c = ((struct PNClosure *)c)->method(P, c, r, ##ARGS); \
    c; \
  })

#define potion_method(RCV, MSG, FN, SIG) \
  potion_send(RCV, PN_def, potion_str(P, MSG), PN_FUNC(FN, SIG))
#define potion_class_method(RCV, MSG, FN, SIG) \
  potion_send(((struct PNVtable *)(RCV))->meta, PN_def, potion_str(P, MSG), PN_FUNC(FN, SIG))

/// common method names in GC protected area
extern PN PN_allocate, PN_break, PN_call, PN_class, PN_compile,
  PN_continue, PN_def, PN_delegated, PN_else, PN_elsif, PN_if,
  PN_lookup, PN_loop, PN_print, PN_return, PN_self, PN_string,
  PN_while;
extern PN PN_add, PN_sub, PN_mult, PN_div, PN_rem, PN_bitn, PN_bitl, PN_bitr;
extern PN PN_cmp, PN_number, PN_name, PN_length, PN_size, PN_STR0;
extern PN PN_extern, PN_integer;
extern PN pn_filenames;

/// zero values per type
static inline PN potion_type_default(char type) {
  return type == 'N' ? PN_ZERO
       : type == 'I' ? PN_ZERO
       : type == 'S' ? PN_STR0
                     : PN_NIL;
}

///
/// the potion API
///
Potion *potion_create(void *);
void potion_destroy(Potion *);
PN potion_error(Potion *, PN, long, long, PN);
void potion_fatal(char *);
void potion_allocation_error(void);
PN potion_io_error(Potion *, const char *);
PN potion_type_error(Potion *, PN);
PN potion_type_error_want(Potion *, const char *, PN, const char *);
void potion_syntax_error(Potion *, struct PNSource *, const char *, ...)
  __attribute__ ((format (printf, 3, 4)));
PNType potion_kind_of(PN);
void potion_p(Potion *, PN);
PN potion_str(Potion *, const char *);
PN potion_str2(Potion *, char *, size_t);
PN potion_strcat(Potion *P, char *str, char *str2);
PN potion_str_add(Potion *P, PN, PN, PN);
PN potion_str_format(Potion *, const char *, ...)
  __attribute__ ((format (printf, 2, 3)));
PN potion_byte_str(Potion *, const char *);
PN potion_byte_str2(Potion *, const char *, size_t len);
PN potion_bytes(Potion *, size_t);
PN potion_bytes_string(Potion *, PN, PN);
PN_SIZE pn_printf(Potion *, PN, const char *, ...)
  __attribute__ ((format (printf, 3, 4)));
void potion_bytes_obj_string(Potion *, PN, PN);
PN potion_bytes_append(Potion *, PN, PN, PN);
void potion_release(Potion *, PN);
PN potion_def_method(Potion *P, PN, PN, PN, PN);
PN potion_type_new(Potion *, PNType, PN);
PN potion_type_new2(Potion *, PNType, PN, PN);
void potion_type_call_is(PN, PN);
void potion_type_callset_is(PN, PN);
void potion_type_constructor_is(PN, PN);
char potion_type_char(PNType);
PN potion_class(Potion *, PN, PN, PN);
PN potion_ivars(Potion *, PN, PN, PN);
PN potion_obj_get_call(Potion *, PN);
PN potion_obj_get_callset(Potion *, PN);
PN potion_obj_get(Potion *, PN, PN, PN);
PN potion_obj_set(Potion *, PN, PN, PN, PN);
PN potion_object_new(Potion *, PN, PN);
PN potion_delegated(Potion *, PN, PN);
PN potion_call(Potion *, PN, PN_SIZE, PN * volatile);
PN potion_lookup(Potion *, PN, PN, PN);
PN potion_bind(Potion *, PN, PN);
PN potion_message(Potion *, PN, PN);
PN potion_closure_new(Potion *, PN_F, PN, PN_SIZE);
PN potion_callcc(Potion *, PN, PN);
PN potion_ref(Potion *, PN);
PN potion_sig(Potion *, char *);
int potion_sig_find(Potion *, PN, PN);
PN potion_double(Potion *, double);
PN potion_strtod(Potion *, char *, int);
PN potion_num_pow(Potion *, PN, PN, PN);
PN potion_num_rand(Potion *, PN, PN);
PN potion_srand(Potion *, PN, PN, PN);
PN potion_rand(Potion *, PN, PN, PN);
PN potion_sig_at(Potion *, PN, int);
PN potion_sig_name_at(Potion *, PN, int);
int potion_sig_arity(Potion *, PN);
int potion_sig_minargs(Potion *, PN);
PN potion_closure_arity(Potion *, PN, PN);
PN potion_closure_minargs(Potion *, PN, PN);
void potion_define_global(Potion *, PN, PN);

PN potion_obj_add(Potion *, PN, PN);
PN potion_obj_sub(Potion *, PN, PN);
PN potion_obj_mult(Potion *, PN, PN);
PN potion_obj_div(Potion *, PN, PN);
PN potion_obj_rem(Potion *, PN, PN);
PN potion_obj_bitn(Potion *, PN);
PN potion_obj_bitl(Potion *, PN, PN);
PN potion_obj_bitr(Potion *, PN, PN);
PN potion_any_cmp(Potion *, PN, PN, PN);

PN potion_tuple_empty(Potion *);
PN potion_tuple_with_size(Potion *, unsigned long);
PN potion_tuple_new(Potion *, PN);
PN potion_tuple_push(Potion *, PN, PN);
PN_SIZE potion_tuple_push_unless(Potion *, PN, PN);
PN_SIZE potion_tuple_find(Potion *, PN, PN);
PN potion_tuple_at(Potion *, PN, PN, PN);
PN potion_tuple_shift(Potion *, PN, PN);
PN potion_tuple_bsearch(Potion *, PN, PN, PN);
PN potion_tuple_ins_sort(Potion *, PN, PN, PN);
PN potion_table_empty(Potion *);
PN potion_table_put(Potion *, PN, PN, PN, PN);
PN potion_table_set(Potion *, PN, PN, PN);
PN potion_table_at(Potion *, PN, PN, PN);
PN potion_lick(Potion *, PN, PN, PN);
PN potion_source_compile(Potion *, PN, PN, PN, PN);
PN potion_source_load(Potion *, PN, PN);
PN potion_source_dump(Potion *, PN, PN, PN, PN);
PN potion_source_dumpbc(Potion *, PN, PN, PN);
PN potion_greg_parse(Potion *, PN);
PN potion_sig_string(Potion *, PN, PN);
PN potion_filename_find(Potion *, PN);
PN potion_filename_push(Potion *, PN);

Potion *potion_gc_boot(void *);
void potion_lobby_init(Potion *);
void potion_object_init(Potion *);
void potion_error_init(Potion *);
void potion_primitive_init(Potion *);
void potion_num_init(Potion *);
void potion_str_hash_init(Potion *);
void potion_str_init(Potion *);
void potion_table_init(Potion *);
void potion_source_init(Potion *);
void potion_lick_init(Potion *);
void potion_compiler_init(Potion *);
void potion_vm_init(Potion *);
void potion_cont_init(Potion *);
#ifndef SANDBOX
void potion_file_init(Potion *);
void potion_loader_init(Potion *);
void potion_loader_add(Potion *, PN path);
PN potion_load(Potion *, PN, PN, PN);
char *potion_find_file(Potion *, char *str, PN_SIZE str_len);
#endif
//XXX add this and the ext initializer dynamically to config.h
#if defined(STATIC) || defined(SANDBOX)
void Potion_Init_readline(Potion *);
void Potion_Init_buffile(Potion *);
void Potion_Init_aio(Potion *);
#endif

void potion_dump_stack(Potion *);
PN potion_num_string(Potion *, PN, PN);
PN potion_gc_reserved(Potion *, PN, PN);
PN potion_gc_actual(Potion *, PN, PN);
PN potion_gc_fixed(Potion *, PN, PN);

PN potion_parse(Potion *, PN, char*);
PN potion_vm_proto(Potion *, PN, PN, ...);
PN potion_vm_class(Potion *, PN, PN);
PN potion_vm(Potion *, PN, PN, PN, PN_SIZE, PN * volatile);
PN potion_eval(Potion *, PN);
PN potion_run(Potion *, PN, int);
PN_F potion_jit_proto(Potion *, PN);

PN potion_class_find(Potion *, PN);
PNType potion_class_type(Potion *, PN);

#endif
