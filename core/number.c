//
// number.c
// simple math
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include "potion.h"
#include "internal.h"

static PN potion_add(Potion *P, PN closure, PN self, PN num) {
  return PN_NUM(PN_INT(self) + PN_INT(num));
}

static PN potion_sub(Potion *P, PN closure, PN self, PN num) {
  return PN_NUM(PN_INT(self) - PN_INT(num));
}

static PN potion_mult(Potion *P, PN closure, PN self, PN num) {
  return PN_NUM(PN_INT(self) * PN_INT(num));
}

static PN potion_div(Potion *P, PN closure, PN self, PN num) {
  return PN_NUM(PN_INT(self) / PN_INT(num));
}

static PN potion_num_inspect(Potion *P, PN closure, PN self) {
  printf("%ld", PN_INT(self));
  return PN_NIL;
}

void potion_num_init(Potion *P) {
  PN num_vt = PN_VTABLE(PN_TNUMBER);
  potion_method(num_vt, "+", potion_add,  "value=N");
  potion_method(num_vt, "-", potion_sub,  "value=N");
  potion_method(num_vt, "*", potion_mult, "value=N");
  potion_method(num_vt, "/", potion_div,  "value=N");
  potion_method(num_vt, "inspect", potion_num_inspect, 0);
}
