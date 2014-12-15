/** \file internal.c
 memory allocation and innards

 (c) 2008 why the lucky stiff, the freelance professor */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include "potion.h"
#include "internal.h"
#include "table.h"
#include "gc.h"

PN PN_allocate, PN_break, PN_call, PN_class, PN_compile, PN_continue, PN_def,
   PN_delegated, PN_else, PN_elsif, PN_if, PN_lookup, PN_loop, PN_print,
   PN_return, PN_self, PN_string, PN_while;
PN PN_add, PN_sub, PN_mult, PN_div, PN_rem, PN_bitn, PN_bitl, PN_bitr;
PN PN_cmp, PN_number, PN_name, PN_length, PN_size, PN_STR0;
PN PN_extern, PN_integer;
#ifdef P2
PN PN_use, PN_no;
#endif
PN pn_filenames;

PN potion_allocate(Potion *P, PN cl, PN self, PN len) {
  struct PNData *obj = PN_ALLOC_N(PN_TUSER, struct PNData, PN_INT(len));
  obj->siz = len;
  return (PN)obj;
}

static void potion_init(Potion *P) {
  PN vtable, obj_vt, num_vt;
  P->lobby = potion_type_new(P, PN_TLOBBY, 0);       // named Lobby resp. P2
  vtable = potion_type_new(P, PN_TVTABLE, P->lobby); // named Mixin
  obj_vt = potion_type_new(P, PN_TOBJECT, P->lobby);
  num_vt = potion_type_new(P, PN_TNUMBER, obj_vt);
  potion_type_new(P, PN_TDOUBLE, num_vt);
  potion_type_new(P, PN_TINTEGER, num_vt);
  potion_type_new(P, PN_TNIL, obj_vt);               // named NilKind resp. Undef
  potion_type_new(P, PN_TBOOLEAN, obj_vt);
  potion_type_new(P, PN_TSTRING, obj_vt);
  potion_type_new(P, PN_TTABLE, obj_vt);
  potion_type_new(P, PN_TCLOSURE, obj_vt);
  potion_type_new(P, PN_TTUPLE, obj_vt);
  potion_type_new(P, PN_TFILE, obj_vt);
  potion_type_new(P, PN_TSTATE, obj_vt); 	     // named Potion
  potion_type_new(P, PN_TSOURCE, obj_vt);
  potion_type_new(P, PN_TBYTES, obj_vt);
  potion_type_new(P, PN_TPROTO, obj_vt);
  potion_type_new(P, PN_TWEAK, obj_vt);
  potion_type_new(P, PN_TLICK, obj_vt);
  potion_type_new(P, PN_TERROR, obj_vt);
  potion_type_new(P, PN_TCONT, obj_vt);

  potion_str_hash_init(P);
  PN_STR0 = PN_STRN("", 0);
  PN_add  = PN_STRN("+", 1);
  PN_sub  = PN_STRN("-", 1);
  PN_mult = PN_STRN("*", 1);
  PN_div  = PN_STRN("/", 1);
  PN_rem  = PN_STRN("%", 1);
  PN_bitn = PN_STRN("~", 1);
  PN_bitl = PN_STRN("<<", 2);
  PN_bitr = PN_STRN(">>", 2);
  PN_if   = PN_STRN("if", 2);
  PN_def = PN_STRN("def", 3);
  PN_cmp = PN_STRN("cmp", 3);
  PN_call = PN_STRN("call", 4);
  PN_else = PN_STRN("else", 4);
  PN_loop = PN_STRN("loop", 4);
  PN_self = PN_STRN("self", 4);
  PN_name = PN_STRN("name", 4);
  PN_size = PN_STRN("size", 4);
  PN_break = PN_STRN("break", 5);
  PN_class = PN_STRN("class", 5);
  PN_elsif = PN_STRN("elsif", 5);
  PN_print = PN_STRN("print", 5);
  PN_while = PN_STRN("while", 5);
  PN_length = PN_STRN("length", 6);
  PN_return = PN_STRN("return", 6);
  PN_string = PN_STRN("string", 6);
  PN_lookup = PN_STRN("lookup", 6);
  PN_number = PN_STRN("number", 6);
  PN_extern = PN_STRN("extern", 6);
  PN_compile = PN_STRN("compile", 7);
  PN_integer = PN_STRN("integer", 7);
  PN_allocate = PN_STRN("allocate", 8);
  PN_continue = PN_STRN("continue", 8);
  PN_delegated = PN_STRN("delegated", 9);

  potion_def_method(P, 0, vtable, PN_lookup, PN_FUNC(potion_lookup, 0));
  potion_def_method(P, 0, vtable, PN_def, PN_FUNC(potion_def_method, "name=S,block=&"));
  potion_send(vtable, PN_def, PN_allocate, PN_FUNC(potion_allocate, 0));
  potion_send(vtable, PN_def, PN_delegated, PN_FUNC(potion_delegated, 0));

  potion_vm_init(P);
  potion_lobby_init(P);
  potion_object_init(P);
  potion_error_init(P);
#ifndef DISABLE_CALLCC
  potion_cont_init(P);
#endif
  potion_primitive_init(P);
  potion_num_init(P);
  potion_str_init(P);
  potion_table_init(P);
  potion_source_init(P);
  potion_lick_init(P);
  potion_compiler_init(P);
#ifndef SANDBOX
  potion_file_init(P);
  potion_loader_init(P);
#endif

  pn_filenames = PN_TUP0();

  GC_PROTECT(P);
}

Potion *potion_create(void *sp) {
  Potion *P = potion_gc_boot(sp); //zeros P
  P->vt = PN_TSTATE;
  P->uniq = (PNUniq)potion_rand_int();
  PN_FLEX_NEW(P->vts, PN_TFLEX, PNFlex, TYPE_BATCH_SIZE);
  PN_FLEX_SIZE(P->vts) = PN_TYPE_ID(PN_TUSER) + 1;
  P->prec = PN_PREC;
  P->fileno = -1;
  //P->flags = (Potion_Flags)EXEC_VM;
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

/// valid signature types
/// syntax.y: arg-type = ('s' | 'S' | 'n' | 'N' | 'b' | 'B' | 'k' | 't' | 'o' | 'O' | '-' | '&')
/// valid signature modifiers: '|' optional, '.' end, ':' default
char potion_type_char(PNType type) {
  switch (type) {
  case PN_TNIL:  	return 'n'; //0 nil?  (unused)
  case PN_TNUMBER:	return 'N'; //1 Number = Integer or Double
  case PN_TBOOLEAN:	return 'B'; //2 Boolean (unused)
  case PN_TINTEGER:	return 'I'; //3
  case PN_TDOUBLE:    	return 'D'; //4
  case PN_TSTRING:	return 'S'; //5 String
  case PN_TWEAK:       	return 0;   //6 (illegal)
  case PN_TCLOSURE:    	return '&'; //7
  case PN_TTUPLE:      	return 'u'; //8 (used)
  case PN_TSTATE:      	return 's'; //9
  case PN_TFILE:       	return 'F'; //10
  case PN_TOBJECT:     	return 'o'; //11 any (used)
  case PN_TVTABLE:     	return 't'; //12 type (unused)
  case PN_TSOURCE:     	return 'a'; //13 ast or code (used in source_compile)
  case PN_TBYTES:      	return 'b'; //14 aio (used)
  case PN_TPROTO:      	return 'P'; //15
  case PN_TLOBBY:      	return 'l'; //16
  case PN_TTABLE:      	return 'T'; //17 (used)
  case PN_TLICK:       	return 'k'; //18
  case PN_TFLEX:       	return 'f'; //19
  case PN_TSTRINGS:    	return 'x'; //20
  case PN_TERROR:      	return 'r'; //21
  case PN_TCONT:       	return 'c'; //22
  case PN_TUSER:       	return 'm'; //23 generated mixins (unused)
  default:       	return 'm'; //23++
  }
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

PN potion_io_error(Potion *P, const char *msg) {
  return potion_error(P, potion_str_format(P, "Error %s: %s", msg, strerror(errno)),
                      0, 0, 0);
}

static inline char *potion_type_name(Potion *P, PN obj) {
  obj = potion_fwd(obj);
  return PN_IS_PTR(obj)
    ? AS_STR(potion_send(PN_VTABLE(PN_TYPE(obj)), PN_string))
    : PN_IS_NIL(obj) ? "nil"
      : PN_IS_INT(obj) ? "Integer"
        : "Boolean";
}

PN potion_type_error(Potion *P, PN obj) {
  return potion_error(P, potion_str_format(P, "Invalid type %s", potion_type_name(P, obj)),
                      0, 0, 0);
}
PN potion_type_error_want(Potion *P, const char *param, PN obj, const char *type) {
  return potion_error(P, potion_str_format(P, "Invalid type %s for %s, expected %s",
                                           potion_type_name(P, obj), param, type),
                      0, 0, 0);
}

#define PN_EXIT_ERROR 1
#define PN_EXIT_FATAL 2

void potion_fatal(char *message) {
  fprintf(stderr, "** %s\n", message);
  exit(PN_EXIT_FATAL);
}

void potion_syntax_error(Potion *P, const char *fmt, ...) {
  va_list args;
  PN out = potion_bytes(P, 36);
  ((struct PNBytes * volatile)out)->len = 0;
  va_start(args, fmt);
  pn_printf(P, out, fmt, args);
  va_end(args);
  fprintf(stderr, "** Syntax error %s\n", PN_STR_PTR(out));
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

#ifdef DEBUG
void potion_dump(Potion *P, PN data) {
  PN pd = potion_send(data, PN_string);
  PN pt = potion_send(PN_VTABLE(PN_TYPE(data)), PN_string);
  char *d = pd ? PN_STR_PTR(pd) : NIL_NAME;
  char *t = pt ? PN_STR_PTR(pt) : NILKIND_NAME;
  printf("%s (%s)\n", d, t);
}
#define pdump(data) potion_dump(P, data)

void potion_dump_stack(Potion *P) {
  long n;
  PN *end, *ebp, *start = P->mem->cstack;
  struct PNMemory *M = P->mem;
  POTION_ESP(&end);
  POTION_EBP(&ebp);
#if POTION_STACK_DIR > 0
  n = end - start;
#else
  n = start - end + 1;
  start = end;
  end = M->cstack;
#endif

  printf("-- dumping %ld stack from %p to %p --\n", n, start, end);
  printf("   ebp = %p, *ebp = 0x%lx\n", ebp, *ebp);
  while (n--) {
    vPN(Object) o = (struct PNObject*)*start;
    printf("   stack(%ld) = %p: 0x%lx", n, start, *start);
    if (IS_GC_PROTECTED(*start))
      printf(" vt=%x prot", PN_TYPE(o));
    else if (IN_BIRTH_REGION(*start))
      printf(" vt=%x birth", PN_TYPE(o));
    else if (IN_OLDER_REGION(*start))
      printf(" vt=%x OLD", PN_TYPE(o));

    if (*start == 0)
      printf(" nil\n");
    else if (*start & 1)
      printf(" %ld INT\n", PN_INT(*start));
    else if (*start & 2)
      printf(" %s BOOL\n", *start == 2 ? "false" : "true");
    else
      printf(" \n");
    start++;
  }
}
#endif
