//
// primitive.c
// methods for the primitive types
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include "p2.h"
#include "internal.h"

#ifdef P2
static PN potion_nil_is_defined(Potion *P, PN closure, PN self) {
  return PN_FALSE;
}
PN potion_any_is_defined(Potion *P, PN closure, PN self) {
  return PN_TRUE;
}
#else
static PN potion_nil_is_nil(Potion *P, PN closure, PN self) {
  return PN_TRUE;
}
PN potion_any_is_nil(Potion *P, PN closure, PN self) {
  return PN_FALSE;
}
#endif

static PN potion_bool_number(Potion *P, PN closure, PN self) {
  return PN_NUM(PN_TEST(self));
}

static PN potion_bool_string(Potion *P, PN closure, PN self) {
  if (PN_TEST(self)) return potion_str(P, "true");
  return potion_str(P, "false");
}

void potion_primitive_init(Potion *P) {
  PN nil_vt = PN_VTABLE(PN_TNIL);
  PN boo_vt = PN_VTABLE(PN_TBOOLEAN);
#ifdef P2
  potion_method(nil_vt, "defined", potion_nil_is_defined, 0);
#else
  potion_method(nil_vt, NIL_NAME"?", potion_nil_is_nil, 0);
#endif
  potion_method(nil_vt, "number", potion_bool_number, 0);
  potion_send(nil_vt, PN_def, PN_string, potion_str(P, NIL_NAME));
  potion_method(boo_vt, "number", potion_bool_number, 0);
  potion_method(boo_vt, "string", potion_bool_string, 0);
}
