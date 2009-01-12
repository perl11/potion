//
// number.c
// simple math
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "potion.h"
#include "internal.h"

PN potion_pow(Potion *P, PN cl, PN num, PN sup) {
  return PN_NUM((int)pow((double)PN_INT(num), (double)PN_INT(sup)));
}

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
  char ints[21];
  sprintf(ints, "%ld", PN_INT(self));
  return potion_byte_str(P, ints);
}

void potion_num_init(Potion *P) {
  PN num_vt = PN_VTABLE(PN_TNUMBER);
  potion_method(num_vt, "+", potion_add,  "value=N");
  potion_method(num_vt, "-", potion_sub,  "value=N");
  potion_method(num_vt, "*", potion_mult, "value=N");
  potion_method(num_vt, "/", potion_div,  "value=N");
  potion_method(num_vt, "inspect", potion_num_inspect, 0);
}
