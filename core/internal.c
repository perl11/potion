//
// internal.c
// memory allocation and innards
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "potion.h"
#include "internal.h"

#define TYPE_BATCH_SIZE 64

PN PN_allocate, PN_break, PN_call, PN_compile, PN_continue,
   PN_def, PN_delegated, PN_else, PN_elsif, PN_if, PN_inspect,
   PN_lookup, PN_loop, PN_print, PN_while, PN__link;

static void potion_init(Potion *P) {
  PN vtable = potion_type_new(P, PN_TVTABLE, 0);
  PN obj_vt = potion_type_new(P, PN_TOBJECT, vtable);
  potion_type_new(P, PN_TNIL, obj_vt);
  potion_type_new(P, PN_TNUMBER, obj_vt);
  potion_type_new(P, PN_TBOOLEAN, obj_vt);
  potion_type_new(P, PN_TSTRING, obj_vt);
  potion_type_new(P, PN_TTABLE, obj_vt);
  potion_type_new(P, PN_TCLOSURE, obj_vt);
  potion_type_new(P, PN_TTUPLE, obj_vt);
  potion_type_new(P, PN_TSTATE, obj_vt);
  potion_type_new(P, PN_TSOURCE, obj_vt);
  potion_type_new(P, PN_TBYTES, obj_vt);
  potion_type_new(P, PN_TPROTO, obj_vt);
  potion_type_new(P, PN_TLOBBY, obj_vt);
  potion_type_new(P, PN_TWEAK, obj_vt);
  potion_str_hash_init(P);
  potion_lobby_init(P);

  PN_allocate = potion_str(P, "allocate");
  PN_break = potion_str(P, "break");
  PN_call = potion_str(P, "call");
  PN_continue = potion_str(P, "continue");
  PN_def = potion_str(P, "def");
  PN_delegated = potion_str(P, "delegated");
  PN_compile = potion_str(P, "compile");
  PN_else = potion_str(P, "else");
  PN_elsif = potion_str(P, "elsif");
  PN_if = potion_str(P, "if");
  PN_inspect = potion_str(P, "inspect");
  PN_lookup = potion_str(P, "lookup");
  PN_loop = potion_str(P, "loop");
  PN_print = potion_str(P, "print");
  PN_while = potion_str(P, "while");
  PN__link = potion_str(P, "~link");

  potion_def_method(P, 0, vtable, PN_lookup, PN_FUNC(potion_lookup, 0));
  potion_def_method(P, 0, vtable, PN_def, PN_FUNC(potion_def_method, 0));

  potion_send(vtable, PN_def, PN_allocate, PN_FUNC(potion_allocate, 0));
  potion_send(vtable, PN_def, PN_delegated, PN_FUNC(potion_delegated, 0));

  potion_object_init(P);
  potion_primitive_init(P);
  potion_num_init(P);
  potion_str_init(P);
  potion_table_init(P);
  potion_source_init(P);
  potion_compiler_init(P);
}

Potion *potion_create() {
  Potion *P = PN_ALLOC(Potion);
  PN_MEMZERO(P, Potion);
  PN_GB(P);
  P->vt = PN_TSTATE;
  P->typea = TYPE_BATCH_SIZE;
  P->typen = PN_TUSER;
  P->vts = PN_ALLOC_N(PN, P->typea);
  potion_init(P);
  return P;
}

PN potion_delegated(Potion *P, PN closure, PN self) {
  PNType t = P->typen++;
  PN vt = potion_type_new(P, t, self);

  // TODO: expand the main vtable if full
  if (P->typea == P->typen)
    printf("Vtable out of room!\n");
  return vt;
}

PN potion_call(Potion *P, PN cl, PN_SIZE argc, PN *argv) {
  struct PNClosure *c = PN_CLOSURE(cl);
  switch (argc) {
    case 0:
    return c->method(P, cl, cl);
    case 1:
    return c->method(P, cl, argv[0]);
    case 2:
    return c->method(P, cl, argv[0], argv[1]);
    case 3:
    return c->method(P, cl, argv[0], argv[1], argv[2]);
    case 4:
    return c->method(P, cl, argv[0], argv[1], argv[2], argv[3]);
    case 5:
    return c->method(P, cl, argv[0], argv[1], argv[2], argv[3], argv[4]);
    case 6:
    return c->method(P, cl, argv[0], argv[1], argv[2], argv[3], argv[4],
        argv[5]);
    case 7:
    return c->method(P, cl, argv[0], argv[1], argv[2], argv[3], argv[4],
        argv[5], argv[6]);
    case 8:
    return c->method(P, cl, argv[0], argv[1], argv[2], argv[3], argv[4],
        argv[5], argv[6], argv[7]);
    case 9:
    return c->method(P, cl, argv[0], argv[1], argv[2], argv[3], argv[4],
        argv[5], argv[6], argv[7], argv[8]);
    case 10:
    return c->method(P, cl, argv[0], argv[1], argv[2], argv[3], argv[4],
        argv[5], argv[6], argv[7], argv[8], argv[9]);
    case 11:
    return c->method(P, cl, argv[0], argv[1], argv[2], argv[3], argv[4],
        argv[5], argv[6], argv[7], argv[8], argv[9], argv[10]);
    case 12:
    return c->method(P, cl, argv[0], argv[1], argv[2], argv[3], argv[4],
        argv[5], argv[6], argv[7], argv[8], argv[9], argv[10], argv[11]);
    case 13:
    return c->method(P, cl, argv[0], argv[1], argv[2], argv[3], argv[4],
        argv[5], argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
        argv[12]);
    case 14:
    return c->method(P, cl, argv[0], argv[1], argv[2], argv[3], argv[4],
        argv[5], argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
        argv[12], argv[13]);
    case 15:
    return c->method(P, cl, argv[0], argv[1], argv[2], argv[3], argv[4],
        argv[5], argv[6], argv[7], argv[8], argv[9], argv[10], argv[11],
        argv[12], argv[13], argv[14]);
  }
  return PN_NIL; // TODO: error "too many arguments"
}

PNType potion_kind_of(PN obj) {
  return potion_type(obj);
}
