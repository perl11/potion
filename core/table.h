//
// table.h
// the central table type, based on khash
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#ifndef POTION_TABLE_H
#define POTION_TABLE_H

typedef PN (*PN_MCACHE_FUNC)(unsigned int hash);
// TODO: ensure the random PNUniq is truly unique for strings
typedef PN (*PN_IVAR_FUNC)(PNUniq hash);

struct PNVtable {
  PN_OBJECT_HEADER
  PN parent;
  PNType type;
  int ivlen;
  PN ivars;
  vPN(Table) methods;
  PN ctor, call, callset;
  PN_MCACHE_FUNC mcache;
  PN_IVAR_FUNC ivfunc;
};

struct PNTable {
  PN_OBJECT_HEADER
  PN_TABLE_HEADER
  char table[0];
};

KHASH_MAP_INIT_PN(PN, struct PNTable);
KHASH_MAP_INIT_STR(str, struct PNTable);

#endif
