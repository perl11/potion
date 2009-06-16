//
// lick.c
// the interleaved data format
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include "potion.h"
#include "internal.h"

PN potion_lick(Potion *P, PN name, PN inner) {
  vPN(Lick) lk = PN_ALLOC(PN_TLICK, struct PNLick);
  lk->name = name;
  lk->inner = inner;
  return (PN)lk;
}

PN potion_lick_name(Potion *P, PN cl, PN self) {
  return ((struct PNLick *)self)->name;
}

void potion_lick_init(Potion *P) {
  PN lk_vt = PN_VTABLE(PN_TLICK);
  potion_method(lk_vt, "name", potion_lick_name, 0);
}
