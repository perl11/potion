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

#ifdef JIT_MCACHE
typedef PN (*PN_MCACHE_FUNC)(unsigned int hash);
#endif
// TODO: ensure the random PNUniq is truly unique for strings
typedef PN (*PN_IVAR_FUNC)(PNUniq hash);

struct PNVtable {
  PN_OBJECT_HEADER
  PN parent;
  PNType type;
  int ivlen;
  PN ivars;
  PN ctor;
  PN call;
  PN callset;
#ifdef JIT_MCACHE
  PN_MCACHE_FUNC mcache;
#endif
  PN_IVAR_FUNC ivfunc;
  kh_PN_t kh[0];
};

struct PNStrTable {
  PN_OBJECT_HEADER
  kh_str_t kh[0];
};

struct PNTable {
  PN_OBJECT_HEADER
  kh_PN_t kh[0];
};

#endif
