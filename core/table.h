//
// table.h
// the central table type, based on khash
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#ifndef POTION_TABLE_H
#define POTION_TABLE_H

KHASH_MAP_INIT_STR(str);
KHASH_MAP_INIT_PN(PN);

struct PNVtable {
  PN_OBJECT_HEADER
  PNType type;
  PN parent;
  PN_F func;
#ifdef JIT_MCACHE
  PN_MCACHE_FUNC mcache;
#endif
  int ivars;
  kh_PN_t kh[0];
};

struct PNTable {
  PN_OBJECT_HEADER
  kh_PN_t kh[0];
};

#endif
