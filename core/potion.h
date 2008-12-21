//
// potion.h
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#ifndef POTION_H
#define POTION_H

#define POTION_VERSION  "0.0"
#define POTION_DATE     "2009-01-01"
#define POTION_MINOR    0
#define POTION_MAJOR    0
#define POTION_COMMIT   "afc6509"
#define POTION_SIG      "p\07\10n"

//
// types
//
typedef unsigned long PN;

struct PNGarbage;
struct PNTuple;
struct PNObject;
struct PNClosure;

#define PN_TNONE        (-1)
#define PN_TNIL         0
#define PN_TNUMBER      1
#define PN_TBOOLEAN     2
#define PN_TSTRING      3
#define PN_TTABLE       4
#define PN_TCLOSURE     5
#define PN_TTUPLE       6

#define PN_TYPE(x)      potion_type((PN)(x))
#define PN_VTYPE(x)     (((struct PNObject *)(x))->vt)

#define PN_NIL          ((PN)0)
#define PN_TRUE         ((PN)2)
#define PN_FALSE        ((PN)4)
#define PN_TUPLE        ((PN)6)
#define PN_PRIMITIVE    6

#define PN_TEST(v)      (((PN)(v) & ~PN_NIL) != 0)
#define PN_IS_NIL(v)    ((PN)(v) == PN_NIL)
#define PN_IS_BOOL(v)   ((PN)(v) == PN_FALSE || (PN)(v) == PN_TRUE)
#define PN_IS_NUM(v)    ((PN)(v) & PN_NUM_FLAG)
#define PN_IS_TUPLE(v)  ((PN)(v) & PN_TUPLE_FLAG)

#define PN_NUM_FLAG     0x01
#define PN_TUPLE_FLAG   0x06

#define PN_NUM(i)       ((PN)(((long)(i))<<1 | PN_NUM_FLAG))
#define PN_INT(x)       (((long)(x))>>1)
#define PN_GB(x,o,m)    (x).next = o; (x).marked = m

struct PNGarbage {
  struct PNGarbage *next;
  unsigned char marked;
};

struct PNTuple {
  struct PNGarbage *next;
  unsigned long len;
  PN *set[0];
};

#define PN_OBJECT_HEADER \
  struct PNGarbage gb; \
  unsigned long vt;

struct PNObject {
  PN_OBJECT_HEADER
  char *data[0];
};

typedef PN (*imp_t)(struct PNClosure *closure, PN receiver, ...);

struct PNClosure {
  PN_OBJECT_HEADER
  imp_t method;
  PN value;
};

// the potion type is the 't' in the vtable tuple (m,t)
static inline int potion_type(PN obj) {
  if (PN_IS_NUM(obj))  return PN_TNUMBER;
  if (obj == 0)        return PN_NIL;
  if (obj & PN_PRIMITIVE)
  {
    if (obj == PN_FALSE) return PN_TBOOLEAN;
    return (obj & PN_PRIMITIVE);
  }
  return PN_VTYPE(obj);
}

static inline PN potion_obj_alloc(size_t size) {
  return (PN)calloc(1, sizeof(struct PNObject) + size);
}

//
// the Potion functions
//
PN potion_bind(PN rcv, PN msg);
PN potion_closure_new(imp_t meth, PN val);

void potion_parse(char *);
void potion_run();

#endif
