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
  printf("nil");
  return PN_NIL;
}

static PN potion_bool_inspect(Potion *P, PN closure, PN self) {
  if (PN_TEST(self)) printf("true");
  else               printf("false");
  return PN_NIL;
}

void potion_primitive_init(Potion *P) {
  PN nil_vt = PN_VTABLE(PN_TNIL);
  PN boo_vt = PN_VTABLE(PN_TBOOLEAN);
  potion_method(nil_vt, "inspect", potion_nil_inspect, 0);
  potion_method(boo_vt, "inspect", potion_bool_inspect, 0);
}
