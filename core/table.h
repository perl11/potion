//
// table.h
// the central table type, based on khash
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#ifndef POTION_TABLE_H
#define POTION_TABLE_H

#if __WORDSIZE != 64
KHASH_MAP_INIT_INT(_PN, _PN);
#else
KHASH_MAP_INIT_INT64(_PN, _PN);
#endif

KHASH_MAP_INIT_STR(str, _PN);
KHASH_MAP_INIT_INT(id, _PN);

struct PNVtable {
  PN_OBJECT_HEADER
  PNType type;
  PN parent;
  PN_F func;
#ifdef JIT_MCACHE
  PN_MCACHE_FUNC mcache;
#endif
  int ivars;
  kh_id_t kh[0];
};

struct PNTable {
  PN_OBJECT_HEADER
  kh__PN_t kh[0];
};

#endif
