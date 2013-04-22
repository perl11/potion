///
/// table.h
/// the central table type, based on khash
///
/// (c) 2008 why the lucky stiff, the freelance professor
///
#ifndef POTION_TABLE_H
#define POTION_TABLE_H

#include "potion.h"
#include "internal.h"
#include "khash.h"

typedef PN (*PN_MCACHE_FUNC)(unsigned int hash);
// TODO: ensure the random PNUniq is truly unique for strings
typedef PN (*PN_IVAR_FUNC)(PNUniq hash);

/// the central vtable, see io http://www.piumarta.com/pepsi/objmodel.pdf
struct PNVtable {
  PN_OBJECT_HEADER
  PNType parent, type;
  PN name;
  int ivlen;
  PN ivars;
  vPN(Table) methods;
  vPN(Vtable) meta;
  PN ctor, call, callset;
  PN_MCACHE_FUNC mcache;
  PN_IVAR_FUNC ivfunc;
};

/// the table class, based on khash
struct PNTable {
  PN_OBJECT_HEADER
  PN_TABLE_HEADER /// PN_SIZE n_buckets, size, n_occupied, upper_bound
  char table[0];
};

KHASH_MAP_INIT_PN(PN, struct PNTable);
KHASH_MAP_INIT_STR(str, struct PNTable);

#endif
