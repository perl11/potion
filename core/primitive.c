///\file primitive.c
/// methods for the immediate primitive types PN_NIL, PNBoolean, PNAny
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include "p2.h"
#include "internal.h"

#ifdef P2
///memberof PN_NIL
/// "defined" method (p2 only)
///\return PN_FALSE for PN_NIL, PN_TRUE for PNAny
static PN potion_nil_is_defined(Potion *P, PN closure, PN self) {
  return PN_FALSE;
}
PN potion_any_is_defined(Potion *P, PN closure, PN self) {
  return PN_TRUE;
}
#else
///memberof PN_NIL
/// "nil?" method (non-p2)
///\return PN_TRUE for PN_NIL, PN_TRUE for PNAny
static PN potion_nil_is_nil(Potion *P, PN closure, PN self) {
  return PN_TRUE;
}
PN potion_any_is_nil(Potion *P, PN closure, PN self) {
  return PN_FALSE;
}
#endif

/**\memberof Lobby
  "cmp" method. compare given value against argument.
  \param value PN
  \return PNNumber -1 if less, 0 if equal or 1 if greater */
PN potion_any_cmp(Potion *P, PN cl, PN self, PN value) {
  return potion_send(self, PN_cmp, value);
}
/** memberof NilKind
 "cmp" method. nil is 0 or "" or FALSE as cmp context
  otherwise it is always less.
 */
static PN potion_nil_cmp(Potion *P, PN cl, PN self, PN value) {
  switch (potion_type(value)) {
  case PN_TNIL:
    return 0;
  case PN_TNUMBER:
    return potion_send(PN_NUM(0), PN_cmp, value);
  case PN_TBOOLEAN:
    return potion_send(PN_FALSE, PN_cmp, value);
  case PN_TSTRING:
    return potion_send(PN_STR(""), PN_cmp, value);
  default:
    return PN_NUM(-1);
  }
}

/// fw to num
static PN potion_bool_cmp(Potion *P, PN cl, PN self, PN value) {
  switch (potion_type(value)) {
  case PN_TBOOLEAN:
    return self < value ? -1 : self == value ? 0 : 1;
  case PN_TNUMBER:
    return potion_send(PN_NUM(PN_TEST(self)), PN_cmp, value);
  case PN_TNIL:
  case PN_TSTRING: // false < ".." < true
  default:
    return value == PN_FALSE ? -1 : 1; //false < any < true
  }
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
#ifdef P2
  potion_method(P->lobby, "defined", potion_any_is_defined, 0);
  potion_method(nil_vt, "defined", potion_nil_is_defined, 0);
#else
  potion_method(P->lobby, NIL_NAME"?", potion_any_is_nil, 0);
  potion_method(nil_vt, NIL_NAME"?", potion_nil_is_nil, 0);
#endif
  potion_method(nil_vt, "number", potion_bool_number, 0);
  potion_send(nil_vt, PN_def, PN_string, potion_str(P, NIL_NAME));
  potion_method(boo_vt, "number", potion_bool_number, 0);
  potion_method(boo_vt, "string", potion_bool_string, 0);
  potion_method(P->lobby, "cmp",  potion_any_cmp, "value=o");
  potion_method(nil_vt, "cmp",    potion_nil_cmp, "value=o");
  potion_method(boo_vt, "cmp",    potion_bool_cmp, "value=o");
}
