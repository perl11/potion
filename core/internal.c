//
// internal.c
// memory allocation and innards
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include "p2.h"
#include "internal.h"
#include "table.h"
#include "gc.h"

PN PN_allocate, PN_break, PN_call, PN_class, PN_compile, PN_continue, PN_def,
   PN_delegated, PN_else, PN_elsif, PN_if, PN_lookup, PN_loop, PN_print,
   PN_return, PN_self, PN_string, PN_while;
PN PN_add, PN_sub, PN_mult, PN_div, PN_rem, PN_bitn, PN_bitl, PN_bitr;

PN potion_allocate(Potion *P, PN cl, PN self, PN len) {
  struct PNData *obj = PN_ALLOC_N(PN_TUSER, struct PNData, PN_INT(len));
  obj->siz = len;
  return (PN)obj;
}

static void potion_init(Potion *P) {
  PN vtable, obj_vt;
  P->lobby = potion_type_new(P, PN_TLOBBY, 0);       // named Lobby resp. P2
  vtable = potion_type_new(P, PN_TVTABLE, P->lobby); // named Mixin
  obj_vt = potion_type_new(P, PN_TOBJECT, P->lobby);
  potion_type_new(P, PN_TNIL, obj_vt);               // named NilKind resp. Undef
  potion_type_new(P, PN_TNUMBER, obj_vt);
  potion_type_new(P, PN_TBOOLEAN, obj_vt);
  potion_type_new(P, PN_TSTRING, obj_vt);
  potion_type_new(P, PN_TTABLE, obj_vt);
  potion_type_new(P, PN_TCLOSURE, obj_vt);
  potion_type_new(P, PN_TTUPLE, obj_vt);
  potion_type_new(P, PN_TFILE, obj_vt);
  potion_type_new(P, PN_TSTATE, obj_vt); // named Potion
  potion_type_new(P, PN_TSOURCE, obj_vt);
  potion_type_new(P, PN_TBYTES, obj_vt);
  potion_type_new(P, PN_TPROTO, obj_vt);
  potion_type_new(P, PN_TWEAK, obj_vt);
  potion_type_new(P, PN_TLICK, obj_vt);
  potion_type_new(P, PN_TERROR, obj_vt);
  potion_type_new(P, PN_TCONT, obj_vt);
  potion_str_hash_init(P);

  PN_allocate = potion_str(P, "allocate");
  PN_break = potion_str(P, "break");
  PN_call = potion_str(P, "call");
  PN_continue = potion_str(P, "continue");
  PN_def = potion_str(P, "def");
  PN_delegated = potion_str(P, "delegated");
  PN_class = potion_str(P, "class");
  PN_compile = potion_str(P, "compile");
  PN_else = potion_str(P, "else");
  PN_elsif = potion_str(P, "elsif");
  PN_if = potion_str(P, "if");
  PN_lookup = potion_str(P, "lookup");
  PN_loop = potion_str(P, "loop");
  PN_print = potion_str(P, "print");
  PN_return = potion_str(P, "return");
  PN_self = potion_str(P, "self");
  PN_string = potion_str(P, "string");
  PN_while = potion_str(P, "while");

  PN_add = potion_str(P, "+");
  PN_sub = potion_str(P, "-");
  PN_mult = potion_str(P, "*");
  PN_div = potion_str(P, "/");
  PN_rem = potion_str(P, "%");
  PN_bitn = potion_str(P, "~");
  PN_bitl = potion_str(P, "<<");
  PN_bitr = potion_str(P, ">>");

  potion_def_method(P, 0, vtable, PN_lookup, PN_FUNC(potion_lookup, 0));
  potion_def_method(P, 0, vtable, PN_def, PN_FUNC(potion_def_method, "name=S,block=&"));

  potion_send(vtable, PN_def, PN_allocate, PN_FUNC(potion_allocate, 0));
  potion_send(vtable, PN_def, PN_delegated, PN_FUNC(potion_delegated, 0));

  potion_vm_init(P);
  potion_lobby_init(P);
  potion_object_init(P);
  potion_error_init(P);
  potion_cont_init(P);
  potion_primitive_init(P);
  potion_num_init(P);
  potion_str_init(P);
  potion_table_init(P);
  potion_source_init(P);
  potion_lick_init(P);
  potion_compiler_init(P);
  potion_file_init(P);
  potion_loader_init(P);

  GC_PROTECT(P);
}

Potion *potion_create(void *sp) {
  Potion *P = potion_gc_boot(sp);
  P->vt = PN_TSTATE;
  P->uniq = (PNUniq)potion_rand_int();
  PN_FLEX_NEW(P->vts, PN_TFLEX, PNFlex, TYPE_BATCH_SIZE);
  PN_FLEX_SIZE(P->vts) = PN_TYPE_ID(PN_TUSER) + 1;
  P->prec = PN_PREC;
  potion_init(P);
  return P;
}

void potion_destroy(Potion *P) {
  potion_gc_release(P);
}

PN potion_delegated(Potion *P, PN closure, PN self) {
  PNType t = PN_FLEX_SIZE(P->vts) + PN_TNIL;
  PN_FLEX_NEEDS(1, P->vts, PN_TFLEX, PNFlex, TYPE_BATCH_SIZE);
  return potion_type_new(P, t, self);
}

PN potion_call(Potion *P, PN cl, PN_SIZE argc, PN * volatile argv) {
  vPN(Closure) c = PN_CLOSURE(cl);
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

PN potion_error(Potion *P, PN msg, long lineno, long charno, PN excerpt) {
  struct PNError *e = PN_ALLOC(PN_TERROR, struct PNError);
  e->message = msg;
  e->line = PN_NUM(lineno);
  e->chr = PN_NUM(charno);
  e->excerpt = excerpt;
  return (PN)e;
}

PN potion_error_string(Potion *P, PN cl, PN self) {
  vPN(Error) e = (struct PNError *)self;
  if (e->excerpt == PN_NIL)
    return potion_str_format(P, "** %s\n", PN_STR_PTR(e->message));
  return potion_str_format(P, "** %s\n"
    "** Where: (line %ld, character %ld) %s\n", PN_STR_PTR(e->message),
    PN_INT(e->line), PN_INT(e->chr), PN_STR_PTR(e->excerpt));
}

void potion_error_init(Potion *P) {
  PN err_vt = PN_VTABLE(PN_TERROR);
  potion_method(err_vt, "string", potion_error_string, 0);
}

#define PN_EXIT_ERROR 1
#define PN_EXIT_FATAL 2

void potion_fatal(char *message) {
  fprintf(stderr, "** %s\n", message);
  exit(PN_EXIT_FATAL);
}

void potion_allocation_error(void) {
  potion_fatal("Couldn't allocate memory.");
}

// say
void potion_p(Potion *P, PN x) {
  potion_send(potion_send(x, PN_string), PN_print);
  printf("\n");
}

void potion_esp(void **esp) {
  PN x;
  *esp = (void *)&x;
}

void potion_dump_stack(Potion *P) {
  PN_SIZE n;
  PN *end, *ebp, *start = P->mem->cstack;
  POTION_ESP(&end);
  POTION_EBP(&ebp);
#if POTION_STACK_DIR > 0
  n = end - start;
#else
  n = start - end + 1;
  start = end;
  end = P->mem->cstack;
#endif

  printf("-- dumping %u from %p to %p --\n", n, start, end);
  printf("   ebp = %p, *ebp = %lx\n", ebp, *ebp);
  while (n--) {
    printf("   stack(%u) = %lx\n", n, *start);
    start++;
  }
}
