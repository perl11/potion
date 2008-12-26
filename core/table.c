//
// table.c
// the central table type, based on khash
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "internal.h"
#include "potion.h"
#include "khash.h"
#include "table.h"

PN potion_table_new(Potion *P, PN closure, PN self) {
  struct PNTable *t = PN_OBJ_ALLOC(struct PNTable, PN_TTABLE, 0);
}

void potion_table_init(Potion *P) {
  PN tbl_vt = PN_VTABLE(PN_TTABLE);
  potion_method(tbl_vt, "new", potion_table_new, 0);
}
