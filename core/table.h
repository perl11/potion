///\file table.h
/// the central table type, based on core/khash.h
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#ifndef POTION_TABLE_H
#define POTION_TABLE_H

#include "potion.h"
#include "internal.h"
#include "khash.h"

#ifndef MAX_INS_SORT
# define MAX_INS_SORT 10
#endif

typedef PN (*PN_MCACHE_FUNC)(unsigned int hash);
// TODO: ensure the random PNUniq is truly unique for strings
typedef PN (*PN_IVAR_FUNC)(PNUniq hash);

/// the central vtable, see io http://www.piumarta.com/pepsi/objmodel.pdf
/// \image html p2-mop.png
/// \see objmodel.c
struct PNVtable {
  PN_OBJECT_HEADER;  ///< PNType vt; PNUniq uniq
  PNType parent;     ///< parent type, default: for P->lobby: PN_VTABLE(PN_TOBJECT)
  PNType type;       ///< current type
  PN name;           ///< classes/types need to be found by name. \see potion_class_find
  int ivlen;         ///< PN_TUPLE_LEN(ivars)
  PN ivars;          ///< PNTuple of all our or the parents inherited vars
  vPN(Table) methods;///< methods hash, PNTable: name => closures
  vPN(Vtable) meta;  /// meta PNVtable
  PN ctor;           ///< store the bound closure (or its parents)
  PN call, callset;
  PN_MCACHE_FUNC mcache; ///< (yet unused) method cache
  PN_IVAR_FUNC ivfunc;
};

/// the table class, based on khash
struct PNTable {
  PN_OBJECT_HEADER;  ///< PNType vt; PNUniq uniq
  PN_TABLE_HEADER;   ///< PN_SIZE n_buckets, size, n_occupied, upper_bound
  char table[];
};

KHASH_MAP_INIT_PN(PN, struct PNTable)
KHASH_MAP_INIT_STR(str, struct PNTable)

#endif
