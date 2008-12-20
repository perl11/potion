//
// potion.h
//
// (c) 2008 why the lucky stiff, the freelance professor
//

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

#define PN_TNONE        (-1)
#define PN_TNIL         0
#define PN_TNUMBER      1
#define PN_TBOOLEAN     2
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

struct PNTuple {
  unsigned long len;
  PN *set[0];
};

struct PNObject {
  unsigned long vt;
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

//
// the Potion functions
//
void potion_parse(char *);
void potion_run();
