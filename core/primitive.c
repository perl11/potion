///\file primitive.c
/// methods for the immediate primitive types PN_NIL, PNBoolean, PNAny
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include "potion.h"
#include "internal.h"

///memberof PN_NIL
/// "nil?" method (non-p2)
///\return PN_TRUE for PN_NIL, PN_TRUE for PNAny
static PN potion_nil_is_nil(Potion *P, PN closure, PN self) {
  return PN_TRUE;
}

///\memberof PNBoolean
/// "number" method
///\return 0 or 1
static PN potion_bool_number(Potion *P, PN closure, PN self) {
  return PN_NUM(PN_TEST(self));
}

///\memberof PNBoolean
/// "string" method
///\return "true" or "false" as PNString
static PN potion_bool_string(Potion *P, PN closure, PN self) {
  if (PN_TEST(self)) return potion_str(P, "true");
  return potion_str(P, "false");
}

PN potion_any_is_nil(Potion *P, PN closure, PN self) {
  return PN_FALSE;
}

void potion_primitive_init(Potion *P) {
  PN nil_vt = PN_VTABLE(PN_TNIL);
  PN boo_vt = PN_VTABLE(PN_TBOOLEAN);
  potion_method(nil_vt, "nil?", potion_nil_is_nil, 0);
  potion_method(nil_vt, "number", potion_bool_number, 0);
  potion_send(nil_vt, PN_def, PN_string, potion_str(P, "nil"));
  potion_method(boo_vt, "number", potion_bool_number, 0);
  potion_method(boo_vt, "string", potion_bool_string, 0);
}
