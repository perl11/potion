//
// number.c
// simple math
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include "internal.h"
#include "potion.h"

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

void potion_num_init(Potion *P)
{
  PN num_vt = PN_VTABLE(PN_TNUMBER);
  potion_send(num_vt, PN_def, potion_str(P, "+"), PN_FUNC(potion_add));
  potion_send(num_vt, PN_def, potion_str(P, "-"), PN_FUNC(potion_sub));
  potion_send(num_vt, PN_def, potion_str(P, "*"), PN_FUNC(potion_mult));
  potion_send(num_vt, PN_def, potion_str(P, "/"), PN_FUNC(potion_div));
}
