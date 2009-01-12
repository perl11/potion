//
// primitive.c
// methods for the primitive types
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include "potion.h"
#include "internal.h"

static PN potion_nil_inspect(Potion *P, PN closure, PN self) {
  return potion_byte_str(P, "nil");
}

static PN potion_nil_is_nil(Potion *P, PN closure, PN self) {
  return PN_TRUE;
}

static PN potion_bool_inspect(Potion *P, PN closure, PN self) {
  if (PN_TEST(self)) return potion_byte_str(P, "true");
  return potion_byte_str(P, "false");
}

PN potion_any_is_nil(Potion *P, PN closure, PN self) {
  return PN_FALSE;
}

void potion_primitive_init(Potion *P) {
  PN nil_vt = PN_VTABLE(PN_TNIL);
  PN boo_vt = PN_VTABLE(PN_TBOOLEAN);
  potion_method(nil_vt, "inspect", potion_nil_inspect, 0);
  potion_method(nil_vt, "nil?", potion_nil_is_nil, 0);
  potion_method(boo_vt, "inspect", potion_bool_inspect, 0);
}
