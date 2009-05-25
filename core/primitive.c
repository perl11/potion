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

static PN potion_nil_is_nil(Potion *P, PNv closure, PNv self) {
  return PN_TRUE;
}

static PN potion_bool_string(Potion *P, PNv closure, PNv self) {
  if (PN_TEST(self)) return potion_str(P, "true");
  return potion_str(P, "false");
}

PN potion_any_is_nil(Potion *P, PNv closure, PNv self) {
  return PN_FALSE;
}

void potion_primitive_init(Potion *P) {
  PNv nil_vt = PN_VTABLE(PN_TNIL);
  PNv boo_vt = PN_VTABLE(PN_TBOOLEAN);
  potion_method(nil_vt, "nil?", potion_nil_is_nil, 0);
  potion_send(nil_vt, PN_def, PN_string, potion_str(P, "nil"));
  potion_method(boo_vt, "string", potion_bool_string, 0);
}
