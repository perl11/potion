//
// table.h
// the central table type, based on khash
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#ifndef POTION_TABLE_H
#define POTION_TABLE_H

#if __WORDSIZE != 64
KHASH_MAP_INIT_INT(PN, PN);
#else
KHASH_MAP_INIT_INT64(PN, PN);
#endif

KHASH_MAP_INIT_INT(str, PN);

struct PNTable {
  PN_OBJECT_HEADER
  kh_PN_t *kh;
};

#endif
