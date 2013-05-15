/**\file p2.c
  perl specific functions. global namespaces, ...
  %main:: = {
    hash of symbols, ...
    + hash of subpackages::
  }
  P->nstuple: stack of nested package blocks, beginning with main::
  the last is always the current package.

  - pkg: namespace helpers work on the current active package/namespace
  - namespace: ie PNTable, works as method on the given namespace
  - nstuple: PNTuple, stack of namespaces, first=main::, last=active

 (c) 2013 by perl11 org
*/

#include <stdio.h>
#include <stdlib.h>
#include "p2.h"
#include "internal.h"
#include "table.h"
#include "ast.h"

#define CUR_PKG potion_tuple_last(P, 0, (PN)P->nstuple)
static PN potion_pkg_upper(Potion *P, PN cl, PN name);

/**\methodof PNNamespace
   create a new subpackage under the given namespace.
   \param PNString pkg */
PN potion_namespace_create(Potion *P, PN cl, PN self, PN pkg) {
  char *p = PN_STR_PTR(pkg);
  int len = PN_STR_LEN(pkg);
  if (p[len -2] == ':' && p[len - 1] == ':') {
    PN upper = potion_pkg_upper(P, cl, pkg);
    potion_table_set(P, cl, upper, pkg);
  }
  PN ns = potion_table_set(P, cl, self, pkg);
  return ns;
}
/**\methodof PNNamespace
  put/intern a new symbol and value into the namespace
  \param PNString name */
PN potion_namespace_put(Potion *P, PN cl, PN self, PN name, PN value) {
  return potion_table_put(P, cl, self, name, value);
}
/**\methodof PNNamespace
   "find" a symbol in the namespace, return its value.
   \param PNString name */
PN potion_namespace_at(Potion *P, PN cl, PN self, PN key) {
  return potion_table_at(P, cl, self, key);
}

/**
   set a new namespace, overriding old, but not the first - main::.
   \param PNString name
   Needed for package NAME; */
PN potion_nstuple_set(Potion *P, PN name) {
  PN t = (PN)P->nstuple;
  PN ns = potion_table_empty(P);
  PN_SIZE len = PN_TUPLE_LEN(t);
  // prev ns
  if (len == 1) {
    return potion_tuple_push(P, t, ns);
  } else {
    return potion_tuple_put(P, 0, t, len-1, ns);
  }
}
/**
   "push" a new namespace to the namespace stack.
   Initialized with "main"
   \param PNString name
   Needed at the beginning of scoped package NAME {} blocks. */
PN potion_nstuple_push(Potion *P, PN name) {
  return potion_tuple_push(P, (PN)P->nstuple, potion_strcat(P, PN_STR_PTR(name), "::"));
}
/**
   "pop" the last namespace from the namespace stack and return it.
   Needed at the end of scoped package NAME {} blocks. */
PN potion_nstuple_pop(Potion *P) {
  return potion_tuple_pop(P, 0, (PN)P->nstuple);
}

/** create a new subpackage under the current package
   \param PNString pkg */
PN potion_pkg_create(Potion *P, PN pkg) {
  return potion_table_set(P, 0, CUR_PKG,
                          potion_strcat(P, PN_STR_PTR(pkg),"::"));
}
/** put/intern a new symbol and value into the current namespace
   \param PNString name */
PN potion_pkg_put(Potion *P, PN name, PN value) {
  return potion_table_put(P, 0, CUR_PKG, name, value);
}
/** "find" a symbol in the namespace, return its value.
   \param PNString name */
PN potion_pkg_at(Potion *P, PN cl, PN self, PN key) {
  return potion_table_at(P, cl, CUR_PKG, key);
}

// sets ns and p for symbol name
#define DO_PKG_TRAVERSE(name)                           \
  char *p = PN_STR_PTR(name);                           \
  char *c;                                              \
  PN ns1;                                               \
  PN ns =  potion_tuple_first(P, 0, (PN)P->nstuple);    \
  if (memcmp(p, "main::", 6)) {                         \
    p += 6;                                             \
  }                                                     \
  while ((c = strstr(p, "::"))) {                       \
    ns1 = potion_table_at(P, 0, ns, PN_STRN(p, p - c)); \
    if (ns1) ns = ns1;                                  \
    p = c + 2;                                          \
  }

/** 
    \param PNString name
    \returns upper the namespace from name.
    i.e. My::Class => My, My => main */
static PN potion_pkg_upper(Potion *P, PN cl, PN name) {
  DO_PKG_TRAVERSE(name)
  return ns;
}

/** PNNamespace i.e. a Hash of names
    we maintain a stack of current namespaces for scoped package NAME {} blocks, with
    main:: always the first, and the current always the last.
   \param self if PNString return the package of the symbol name,
               if nil, return the current package namespace
   \return current namespace, i.e. package hash */
PN potion_pkg(Potion *P, PN cl, PN self) {
  if (!self)
    return CUR_PKG;
  if (PN_IS_STR(self)) {
    DO_PKG_TRAVERSE(self)
    return ns;
  }
}
/** "find" a symbol by traversing the namespace parts+hashes, return its value.
   My::Class::$value => main::->My::->Class->$value
   \param PNString name */
PN potion_sym_at(Potion *P, PN name) {
  DO_PKG_TRAVERSE(name)
  if (!ns) return PN_NIL;
  return potion_table_at(P, 0, ns, PN_STR(p));
}

void potion_p2_init(Potion *P) {
  PN ns_vt = potion_type_new2(P, PN_TNAMESPACE, PN_VTABLE(PN_TTABLE), potion_str(P, "Namespace"));
  potion_method(P->lobby, "package", potion_pkg, 0);
  // derive all namespace methods from PNTable
  //potion_method(ns_vt, "name",   potion_namespace_name, 0);
  potion_method(ns_vt, "create", potion_namespace_create, "name=S"); //intern
  potion_method(ns_vt, "put",    potion_namespace_put, "name=S,value=o");
  potion_method(ns_vt, "at",     potion_namespace_at, "key=S");
  PN main_ns = potion_namespace_create(P, 0, PN_NIL, PN_STR("main::"));
  potion_tuple_push(P, (PN)P->nstuple, main_ns);
}
