/**\file p2.c
  perl specific functions

  global namespaces (dynamic symbol table lookup)
  %main:: = {
    hash of symbols, ...
    + hash of subpackages::
  }
  P->nstuple: stack of nested package blocks, beginning with main::
  the last is always the current package.
  TODO: put namespace into the env (cl PNClosure->symbols)

  - pkg: namespace helpers work on the current active package/namespace via strings
  - namespace: ie PNLick with PNTable as attr
                  parent as inner
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
static PN potion_pkg_parent(Potion *P, PN cl, PN name);

/**\methodof PNNamespace
   create a new subpackage or symbol as child of the given namespace.
   \param PNString pkg. if ending with :: creates a subpackage,
                        if not creates a new symbol */
PN potion_namespace_create(Potion *P, PN cl, PN self, PN pkg) {
  char *p = PN_STR_PTR(pkg);
  int len = PN_STR_LEN(pkg);
  if (p[len -2] == ':' && p[len - 1] == ':') {
    PN parent = potion_pkg_parent(P, cl, pkg);
    PN new = potion_table_set(P, cl, parent, pkg);
    if (!self) {
      PN ns = potion_lick(P, pkg, parent, potion_table_set(P, cl, new, pkg));
      PN_VTYPE(ns) = PN_TNAMESPACE;
      return ns;
    }
  }
  if (self) {
    PN ns = potion_lick(P, pkg, self, potion_table_set(P, cl, self, pkg));
    PN_VTYPE(ns) = PN_TNAMESPACE;
    return ns;
  }
}
/**\methodof PNNamespace
  put/intern a new symbol and value into the namespace
  \param PNString name */
PN potion_namespace_put(Potion *P, PN cl, PN self, PN name, PN value) {
  return potion_table_put(P, cl, PN_LICK(self)->attr, name, value);
}
/**\methodof PNNamespace
   "find" a symbol in the namespace, return its value. (the lookup method)
   \param PNString name */
PN potion_namespace_at(Potion *P, PN cl, PN self, PN key) {
  return potion_table_at(P, cl, PN_LICK(self)->attr, key);
}
/**\methodof PNNamespace
  \returns the full name (PNString), such as "main::My::Class::"
 */
PN potion_namespace_name(Potion *P, PN cl, PN self) {
  return PN_LICK(self)->name;
}
/**\methodof PNNamespace
   \returns a string repr */
PN potion_namespace_string(Potion *P, PN cl, PN self) {
  return potion_str_format(P, "<Namespace %s>", AS_STR(PN_LICK(self)->name));
}

/**
   set a new namespace in nstuple, overriding old, but not the first - main::.
   \param PNString name
   Needed for package NAME; */
PN potion_nstuple_set(Potion *P, PN name) {
  PN t = (PN)P->nstuple;
  PN ns = potion_namespace_create(P, 0, 0, name);
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
  return potion_tuple_push(P, (PN)P->nstuple,
    potion_namespace_create(P, 0, 0, potion_strcat(P, PN_STR_PTR(name), "::")));
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
  return potion_table_put(P, 0, PN_LICK(CUR_PKG)->attr, name, value);
}
/** "find" a symbol in the namespace, return its value.
   \param PNString name */
PN potion_pkg_at(Potion *P, PN cl, PN self, PN key) {
  return potion_table_at(P, cl, PN_LICK(CUR_PKG)->attr, key);
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
    ns1 = potion_table_at(P, 0, PN_LICK(ns)->attr, PN_STRN(p, p - c)); \
    if (ns1) ns = ns1;                                  \
    p = c + 2;                                          \
  }

/** 
   \param PNString name
   \returns parent the namespace from name.
   i.e. My::Class => My, My => main */
static PN potion_pkg_parent(Potion *P, PN cl, PN name) {
  //DO_PKG_TRAVERSE(name)
  char *p = PN_STR_PTR(name);                           
  char *c;                                              
  PN ns1;                                               
  PN ns =  potion_tuple_first(P, 0, (PN)P->nstuple);    
  if (memcmp(p, "main::", 6)) {                         
    p += 6;                                             
  }                                                     
  while ((c = strstr(p, "::"))) {                       
    ns1 = potion_table_at(P, 0, PN_LICK(ns)->attr, PN_STRN(p, p - c)); 
    if (ns1) ns = ns1;                                  
    p = c + 2;                                          
  }
  return ns;
}

static PN potion_namespace_parent(Potion *P, PN cl, PN self) {
  return PN_LICK(self)->inner;
  //return potion_pkg_parent(P, cl, potion_namespace_name(P, cl, self));
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
  return CUR_PKG;
}
/** "find" a symbol by traversing the namespace parts+hashes, return its value.
   My::Class::$value => main::->My::->Class->$value
   \param PNString name */
PN potion_sym_at(Potion *P, PN name) {
  DO_PKG_TRAVERSE(name)
  if (!ns) return PN_NIL;
  return potion_table_at(P, 0, PN_LICK(ns)->attr, PN_STR(p));
}

/// return AST for global or lexical symbol
PN potion_find_symbol(Potion *P, PN ast) {
  PN global;
  if (PN_TYPE(ast) == PN_TSOURCE && PN_SRC(ast)->part == AST_MSG) {
    PN name = PN_S(ast,0);
    if ((global = potion_pkg_at(P, 0, 0, name)))
      PN_S_(ast, 0) = (struct PNSource *)global;
    return ast;
  } else if (PN_IS_STR(ast)) {
    if ((global = potion_pkg_at(P, 0, 0, ast)))
      return global;
    else
      return ast;
  }
}

void potion_p2_init(Potion *P) {
  PN main_ns;
  PN ns_vt = potion_type_new2(P, PN_TNAMESPACE, PN_VTABLE(PN_TLICK), potion_str(P, "Namespace"));
  //PN sym_vt = potion_type_new2(P, PN_TSYMBOL, PN_VTABLE(PN_TLICK), potion_str(P, "Symbol"));
  potion_method(P->lobby, "package", potion_pkg, 0);
  potion_method(ns_vt, "create",  potion_namespace_create, "name=S");  //new subpackage or intern
  potion_method(ns_vt, "name",    potion_namespace_name, 0);
  potion_method(ns_vt, "string",  potion_namespace_string, 0);
  potion_method(ns_vt, "parent",  potion_namespace_parent, 0);
  //potion_method(ns_vt, "lookup",  potion_namespace_at, "name=S");
  potion_type_call_is(ns_vt, PN_FUNC(potion_namespace_at, "name=S")); //symbol-value by name
  potion_type_callset_is(ns_vt, PN_FUNC(potion_namespace_put, "name=S,value=o")); //intern + set
  main_ns = potion_lick(P, PN_STRN("main::",6), 0, potion_table_empty(P)); // name, inner, attr
  PN_VTYPE(main_ns) = PN_TNAMESPACE;
  P->nstuple = (struct PNTuple *)potion_tuple_push(P, PN_TUP0(), main_ns);
}
