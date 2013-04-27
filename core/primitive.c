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

static PN potion_any_is_nil(Potion *P, PN closure, PN self) {
  return PN_FALSE;
}

///\memberof Lobby
/// "cmp" method. compare given value against argument, possibly casting value
///\param value PN
///\return PNNumber 1, 0 or -1
PN potion_any_cmp(Potion *P, PN cl, PN self, PN value) {
  return potion_send(P, PN_cmp, self, value);
}
///memberof PN_NIL
/// "cmp" method. nil is 0 as cmp context
static PN potion_nil_cmp(Potion *P, PN cl, PN self, PN value) {
  return potion_send(P, PN_cmp, PN_NUM(0), value);
}

/// fw to num
static PN potion_bool_cmp(Potion *P, PN cl, PN self, PN value) {
  return potion_send(P, PN_cmp, PN_NUM(PN_TEST(self)), value);
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

void potion_primitive_init(Potion *P) {
  PN nil_vt = PN_VTABLE(PN_TNIL);
  PN boo_vt = PN_VTABLE(PN_TBOOLEAN);
  potion_method(nil_vt, "nil?", potion_nil_is_nil, 0);
  potion_method(P->lobby, "nil?",  potion_any_is_nil, 0);
  potion_method(nil_vt, "number", potion_bool_number, 0);
  potion_send(nil_vt, PN_def, PN_string, potion_str(P, "nil"));
  potion_method(boo_vt, "number", potion_bool_number, 0);
  potion_method(boo_vt, "string", potion_bool_string, 0);
  potion_method(P->lobby, "cmp",  potion_any_cmp, "value=o");
  potion_method(nil_vt, "cmp",    potion_nil_cmp, "value=o");
  potion_method(boo_vt, "cmp",    potion_bool_cmp, "value=o");
}
