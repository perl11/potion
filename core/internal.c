//
// internal.c
// memory allocation and innards
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include "internal.h"
#include "potion.h"

Potion *potion_create() {
  Potion *P = PN_ALLOC(Potion);
  PN_MEMZERO(P, Potion);
  return P;
}

void potion_destroy(Potion *P) {
  PN_FREE(P);
}
