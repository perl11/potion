//
// objmodel.c
// much of this is based on the work of ian piumarta
// <http://www.piumarta.com/pepsi/objmodel.pdf>
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "p2.h"
#include "internal.h"
#include "khash.h"
#include "table.h"
#include "asm.h"

PN potion_closure_new(Potion *P, PN_F meth, PN sig, PN_SIZE extra) {
  PN_SIZE i;
  vPN(Closure) c = PN_ALLOC_N(PN_TCLOSURE, struct PNClosure, extra * sizeof(PN));
  c->method = meth;
  if (PN_IS_TUPLE(sig) && PN_TUPLE_LEN(sig) > 0)
    c->sig = sig;
  c->extra = extra;
  for (i = 0; i < c->extra; i++)
    c->data[i] = PN_NIL;
  return (PN)c;
}

PN potion_closure_code(Potion *P, PN cl, PN self) {
  if (PN_CLOSURE(self)->extra > 0 && PN_IS_PROTO(PN_CLOSURE(self)->data[0])) 
    return PN_CLOSURE(self)->data[0];
  return PN_NIL;
}

PN potion_closure_string(Potion *P, PN cl, PN self, PN len) {
  int x = 0;
  PN out = potion_byte_str(P, "function(");
  if (PN_IS_TUPLE(PN_CLOSURE(self)->sig)) {
    PN_TUPLE_EACH(PN_CLOSURE(self)->sig, i, v, {
      if (PN_IS_STR(v)) {
        if (x++ > 0) pn_printf(P, out, ", ");
        potion_bytes_obj_string(P, out, v);
      }
    });
  }
  pn_printf(P, out, ")");
  return PN_STR_B(out);
}

long potion_arity(Potion *P, PN closure) {
  if (PN_IS_TUPLE(PN_CLOSURE(closure)->sig))
    return PN_TUPLE_LEN(PN_CLOSURE(closure)->sig) / 2;
  return 0;
}

PN potion_closure_arity(Potion *P, PN cl, PN self) {
  return PN_NUM(potion_arity(P, self));
}

PN potion_no_call(Potion *P, PN cl, PN self) {
  return self;
}

void potion_add_metaclass(Potion *P, vPN(Vtable) vt) {
  struct PNVtable *meta = vt->meta = PN_CALLOC_N(PN_TVTABLE, struct PNVtable, 0);
  meta->type = PN_FLEX_SIZE(P->vts) + PN_TNIL;
  PN_FLEX_NEEDS(1, P->vts, PN_TFLEX, PNFlex, TYPE_BATCH_SIZE);
  PN_FLEX_SIZE(P->vts)++;
  meta->name = PN_NIL;
  meta->parent = PN_TVTABLE;
  meta->methods = (struct PNTable *)potion_table_empty(P);
  meta->ctor = PN_FUNC(potion_no_call, 0);
  PN_VTABLE(meta->type) = (PN)meta;
  meta->meta = PN_NIL;
  PN_TOUCH(P->vts);
}

PN potion_type_new(Potion *P, PNType t, PN self) {
  vPN(Vtable) vt = PN_CALLOC_N(PN_TVTABLE, struct PNVtable, 0);
  vt->type = t;
  vt->name = PN_NIL;
  vt->parent = self ? ((struct PNVtable *)self)->type : 0;
  vt->methods = (struct PNTable *)potion_table_empty(P);
  vt->ctor = PN_FUNC(potion_no_call, 0);
  PN_VTABLE(t) = (PN)vt;
  potion_add_metaclass(P, vt);
  return (PN)vt;
}

PN potion_type_new2(Potion *P, PNType t, PN self, PN name) {
  vPN(Vtable) vt = (vPN(Vtable))potion_type_new(P, t, self);
  vt->name = name;
  return (PN)vt;
}

void potion_type_call_is(PN vt, PN cl) {
  ((struct PNVtable *)vt)->call = cl;
}

PN potion_obj_get_call(Potion *P, PN obj) {
  PN cl = ((struct PNVtable *)PN_VTABLE(PN_TYPE(obj)))->call;
  if (cl == PN_NIL) cl = P->call;
  return cl;
}

void potion_type_callset_is(PN vt, PN cl) {
  ((struct PNVtable *)vt)->callset = cl;
}

PN potion_obj_get_callset(Potion *P, PN obj) {
  PN cl = ((struct PNVtable *)PN_VTABLE(PN_TYPE(obj)))->callset;
  if (cl == PN_NIL) cl = P->callset;
  return cl;
}

void potion_type_constructor_is(PN vt, PN cl) {
  ((struct PNVtable *)vt)->ctor = cl;
}

PN potion_class(Potion *P, PN cl, PN self, PN ivars) {
  PN parent = (self == P->lobby ? PN_VTABLE(PN_TOBJECT) : self);
  PN pvars = ((struct PNVtable *)parent)->ivars;
  PNType t = PN_FLEX_SIZE(P->vts) + PN_TNIL;
  PN_FLEX_NEEDS(1, P->vts, PN_TFLEX, PNFlex, TYPE_BATCH_SIZE);
  PN_FLEX_SIZE(P->vts)++;
  self = potion_type_new(P, t, parent);
  if (PN_IS_TUPLE(pvars)) {
    if (!PN_IS_TUPLE(ivars)) ivars = PN_TUP0();
    PN_TUPLE_EACH(pvars, i, v, {PN_PUT(ivars, v);});
  }
  if (PN_IS_TUPLE(ivars))
    potion_ivars(P, PN_NIL, self, ivars);

  if (!PN_IS_CLOSURE(cl))
    cl = ((struct PNVtable *)parent)->ctor;
  ((struct PNVtable *)self)->ctor = cl;

  PN_TOUCH(P->vts);
  return self;
}

PN potion_ivars(Potion *P, PN cl, PN self, PN ivars) {
  struct PNVtable *vt = (struct PNVtable *)self;
#ifdef POTION_JIT_TARGET
  // TODO: allocate assembled instructions together into single pages
  // since many times these tables are <100 bytes.
  PNAsm * volatile asmb = potion_asm_new(P);
  P->target.ivars(P, ivars, &asmb);
  vt->ivfunc = PN_ALLOC_FUNC(asmb->len);
  PN_MEMCPY_N(vt->ivfunc, asmb->ptr, u8, asmb->len);
#endif
  vt->ivlen = PN_TUPLE_LEN(ivars);
  vt->ivars = ivars;
  return self;
}

static inline long potion_obj_find_ivar(Potion *P, PN self, PN ivar) {
  PNType t = PN_TYPE(self);
  vPN(Vtable) vt = (struct PNVtable *)PN_VTABLE(t);
  if (vt->ivfunc != NULL)
    return vt->ivfunc(PN_UNIQ(ivar));

  if (t > PN_TUSER) {
    PN ivars = ((struct PNVtable *)PN_VTABLE(t))->ivars;
    if (ivars != PN_NIL)
      return potion_tuple_binary_search(ivars, ivar);
  }
  return -1;
}

PN potion_obj_get(Potion *P, PN cl, PN self, PN ivar) {
  long i = potion_obj_find_ivar(P, self, ivar);
  if (i >= 0)
    return ((struct PNObject *)self)->ivars[i];
  return PN_NIL;
}

PN potion_obj_set(Potion *P, PN cl, PN self, PN ivar, PN value) {
  long i = potion_obj_find_ivar(P, self, ivar);
  if (i >= 0) {
    ((struct PNObject *)self)->ivars[i] = value;
    PN_TOUCH(self);
  }
  return value;
}

PN potion_proto_method(Potion *P, PN cl, PN self, PN args) {
  return potion_vm(P, PN_CLOSURE(cl)->data[0], P->lobby, args, 0, NULL);
}

PN potion_getter_method(Potion *P, PN cl, PN self) {
  return PN_CLOSURE(cl)->data[0];
}

PN potion_def_method(Potion *P, PN closure, PN self, PN key, PN method) {
  int ret;
  PN cl;
  vPN(Vtable) vt = (struct PNVtable *)self;
  unsigned k = kh_put(PN, vt->methods, key, &ret);
  PN_QUICK_FWD(struct PNTable *, vt->methods);
  PN_TOUCH(vt->methods);

  if (!PN_IS_CLOSURE(method)) {
    if (PN_IS_PROTO(method))
      cl = potion_closure_new(P, (PN_F)potion_proto_method, PN_PROTO(method)->sig, 1);
    else
      cl = potion_closure_new(P, (PN_F)potion_getter_method, PN_NIL, 1);
    PN_CLOSURE(cl)->data[0] = method;
    method = cl;
  }

  kh_val(PN, vt->methods, k) = method;
  PN_TOUCH(self);

#ifdef JIT_MCACHE
  // TODO: make this more flexible, store in fixed gc, see ivfunc TODO also
  // this is disabled until method weakrefs can be stored in fixed memory
  if (P->target.mcache != NULL) {
    PNAsm * volatile asmb = potion_asm_new(P);
    P->target.mcache(P, vt, &asmb);
    if (asmb->len <= 4096) {
      if (vt->mcache == NULL)
        vt->mcache = PN_ALLOC_FUNC(4096);
      PN_MEMCPY_N(vt->mcache, asmb->ptr, u8, asmb->len);
    } else if (vt->mcache != NULL) {
      potion_munmap(vt->mcache, 4096);
      vt->mcache = NULL;
    }
  }
#endif
  return method;
}

PN potion_lookup(Potion *P, PN closure, PN self, PN key) {
  vPN(Vtable) vt = (struct PNVtable *)self;
#ifdef JIT_MCACHE
  if (vt->mcache != NULL)
    return vt->mcache(PN_UNIQ(key));
#endif
  unsigned k = kh_get(PN, vt->methods, key);
  if (k != kh_end(vt->methods)) return kh_val(PN, vt->methods, k);
  return PN_NIL;
}

PN potion_bind(Potion *P, PN rcv, PN msg) {
  PN closure = PN_NIL;
  PN vt = PN_NIL;
  PNType t = PN_TYPE(rcv);
  if (!PN_TYPECHECK(t)) return PN_NIL;
  if (t == PN_TVTABLE && !PN_IS_METACLASS(rcv)) {
    vt = (PN)((struct PNVtable *)rcv)->meta;
  } else {
    vt = PN_VTABLE(t);
  }
  while (1) {
    closure = ((msg == PN_lookup) && (t == PN_TVTABLE))
      ? potion_lookup(P, 0, vt, msg)
      : potion_send(vt, PN_lookup, msg);
    if (closure || !((struct PNVtable *)vt)->parent) break;
    vt = PN_VTABLE(((struct PNVtable *)vt)->parent);
  }
  return closure;
}

PN potion_message(Potion *P, PN rcv, PN msg) {
  PN cl = potion_bind(P, rcv, msg);
  if (PN_IS_CLOSURE(cl) && PN_CLOSURE(cl)->sig == PN_NIL)
    return PN_CLOSURE(cl)->method(P, cl, rcv, PN_NIL);
  return cl;
}

PN potion_obj_add(Potion *P, PN a, PN b) {
  return potion_send(a, PN_add, b);
}

PN potion_obj_sub(Potion *P, PN a, PN b) {
  return potion_send(a, PN_sub, b);
}

PN potion_obj_mult(Potion *P, PN a, PN b) {
  return potion_send(a, PN_mult, b);
}

PN potion_obj_div(Potion *P, PN a, PN b) {
  return potion_send(a, PN_div, b);
}

PN potion_obj_rem(Potion *P, PN a, PN b) {
  return potion_send(a, PN_rem, b);
}

PN potion_obj_bitn(Potion *P, PN a) {
  return potion_send(a, PN_bitn);
}

PN potion_obj_bitl(Potion *P, PN a, PN b) {
  return potion_send(a, PN_bitl, b);
}

PN potion_obj_bitr(Potion *P, PN a, PN b) {
  return potion_send(a, PN_bitr, b);
}

PN potion_ref(Potion *P, PN data) {
  if (PN_IS_REF(data)) return data;
  vPN(WeakRef) ref = PN_ALLOC(PN_TWEAK, struct PNWeakRef);
  ref->data = data;
  return (PN)ref;
}

PN potion_ref_string(Potion *P, PN cl, PN self, PN len) {
  return potion_str(P, "<ref>");
}

PN potion_object_string(Potion *P, PN cl, vPN(Object) self) {
  struct PNVtable *vt = (struct PNVtable *)PN_VTABLE(self->vt);
  if (vt->name != PN_NIL) {
    PN str = potion_byte_str2(P, NULL, 0);
    pn_printf(P, str, "<%s %x>", PN_STR_PTR(vt->name), (PN)self);
    return potion_send(str, PN_string);
  }
  
  return potion_str(P, "<object>");
}

PN potion_object_forward(Potion *P, PN cl, PN self, PN method) {
  printf("#<object>");
  return PN_NIL;
}

PN potion_object_send(Potion *P, PN cl, PN self, PN method) {
  return potion_send(self, method);
}

PN potion_object_new(Potion *P, PN cl, PN self) {
  vPN(Vtable) vt = (struct PNVtable *)self;
  if (PN_IS_METACLASS(vt)) // TODO: error
    return PN_NIL;
  if (vt->type == PN_TVTABLE) // TODO: error
    return PN_NIL;
  return (PN)PN_ALLOC_N(vt->type, struct PNObject,
    potion_type_size(P, (struct PNObject *)self) - sizeof(struct PNObject) + vt->ivlen * sizeof(PN));
}

PN potion_get_metaclass(Potion *P, PN cl, vPN(Vtable) self) {
  return (PN)self->meta;
}

static PN potion_lobby_self(Potion *P, PN cl, PN self) {
  return self;
}

PN potion_lobby_string(Potion *P, PN cl, PN self) {
  PN str = ((struct PNVtable *)self)->name;
  return (void *)str != PN_NIL ? str :
    PN_IS_METACLASS(self) ? potion_str(P, "<metaclass>") : potion_str(P, "<class>");
}

PN potion_lobby_kind(Potion *P, PN cl, PN self) {
  PNType t = PN_TYPE(self);
  if (!PN_TYPECHECK(t)) return PN_NIL; // TODO: error
  return PN_VTABLE(t);
}

static void potion_init_class_reference(Potion *P, PN name, PN vt) {
  potion_send(P->lobby, PN_def, name, vt);
  ((struct PNVtable *)vt)->name = name;
  char meta_str[strlen("<metaclass: >") + PN_STR_LEN(name) + 1];
  sprintf(meta_str, "<metaclass: %s>", PN_STR_PTR(name));
  ((struct PNVtable *)vt)->meta->name = potion_str(P, meta_str);
}

void potion_define_global(Potion *P, PN name, PN val) {
  if (PN_TYPE(val) == PN_TVTABLE && !PN_IS_METACLASS(val)) {
    potion_init_class_reference(P, name, val);
  } else {
    potion_send(P->lobby, PN_def, name, val);
  }
}

PN potion_about(Potion *P, PN cl, PN self) {
  PN about = potion_table_empty(P);
  potion_table_put(P, PN_NIL, about, potion_str(P, "_why"),
    potion_str(P, "“I love _why, but learning Ruby from him is like trying to learn to pole vault "
      "by having Salvador Dali punch you in the face.” - Steven Frank"));
  potion_table_put(P, PN_NIL, about, potion_str(P, "minimalism"),
    potion_str(P, "“The sad thing about ‘minimalism’ is that it has a name.” "
      "- Steve Dekorte"));
  potion_table_put(P, PN_NIL, about, potion_str(P, "stage fright"),
    potion_str(P, "“Recently no move on Potion. I git pull everyday.” "
      "- matz"));
  potion_table_put(P, PN_NIL, about, potion_str(P, "terms of use"),
    potion_str(P, "“Setting up my new anarchist bulletin board so that during registration, if you accept "
      "the terms and conditions, you are banned forever.” - Dr. Casey Hall"));
  potion_table_put(P, PN_NIL, about, potion_str(P, "help"),
    potion_str(P, "`man which` - Evan Weaver"));
  potion_table_put(P, PN_NIL, about, potion_str(P, "ts"),
    potion_str(P, "“pigeon%” - Guy Decoux (1955 - 2008)"));
  potion_table_put(P, PN_NIL, about, potion_str(P, "summary"),
    potion_str(P, "“I smell as how a leprechaun looks.” - Alana Post"));
  return about;
}

PN potion_exit(Potion *P, PN cl, PN self) {
  potion_destroy(P);
  exit(0);
}

void potion_object_init(Potion *P) {
  PN clo_vt = PN_VTABLE(PN_TCLOSURE);
  PN ref_vt = PN_VTABLE(PN_TWEAK);
  PN obj_vt = PN_VTABLE(PN_TOBJECT);
  potion_method(clo_vt, "code", potion_closure_code, 0);
  potion_method(clo_vt, "string", potion_closure_string, 0);
  potion_method(clo_vt, "arity", potion_closure_arity, 0);
  potion_method(ref_vt, "string", potion_ref_string, 0);
  potion_method(obj_vt, "forward", potion_object_forward, 0);
  potion_method(obj_vt, "send", potion_object_send, 0);
  potion_method(obj_vt, "string", potion_object_string, 0);
}

void potion_lobby_init(Potion *P) {

# ifdef P2
#   define LOBBY_NAME "P2"
#   define NILKIND_NAME "Undef"
# else
#   define LOBBY_NAME "Lobby"
#   define NILKIND_NAME "NilKind"
# endif
  potion_init_class_reference(P, potion_str(P, LOBBY_NAME),     P->lobby);
  potion_init_class_reference(P, potion_str(P, "Mixin"),        PN_VTABLE(PN_TVTABLE));
  potion_init_class_reference(P, potion_str(P, "Object"),       PN_VTABLE(PN_TOBJECT));
  potion_init_class_reference(P, potion_str(P, NILKIND_NAME),   PN_VTABLE(PN_TNIL));
  potion_init_class_reference(P, potion_str(P, "Number"),       PN_VTABLE(PN_TNUMBER));
  potion_init_class_reference(P, potion_str(P, "Boolean"),      PN_VTABLE(PN_TBOOLEAN));
  potion_init_class_reference(P, potion_str(P, "String"),       PN_VTABLE(PN_TSTRING));
  potion_init_class_reference(P, potion_str(P, "Table"),        PN_VTABLE(PN_TTABLE));
  potion_init_class_reference(P, potion_str(P, "Function"),     PN_VTABLE(PN_TCLOSURE));
  potion_init_class_reference(P, potion_str(P, "Tuple"),        PN_VTABLE(PN_TTUPLE));
  potion_init_class_reference(P, potion_str(P, "File"),         PN_VTABLE(PN_TFILE));
  potion_init_class_reference(P, potion_str(P, "Potion"),       PN_VTABLE(PN_TSTATE));
  potion_init_class_reference(P, potion_str(P, "Source"),       PN_VTABLE(PN_TSOURCE));
  potion_init_class_reference(P, potion_str(P, "Bytes"),        PN_VTABLE(PN_TBYTES));
  potion_init_class_reference(P, potion_str(P, "Compiled"),     PN_VTABLE(PN_TPROTO));
  potion_init_class_reference(P, potion_str(P, "Ref"),          PN_VTABLE(PN_TWEAK));
  potion_init_class_reference(P, potion_str(P, "Lick"),         PN_VTABLE(PN_TLICK));
  potion_init_class_reference(P, potion_str(P, "Error"),        PN_VTABLE(PN_TERROR));
  potion_init_class_reference(P, potion_str(P, "Continuation"), PN_VTABLE(PN_TCONT));

  P->call = P->callset = PN_FUNC(potion_no_call, 0);
  
  PN mixin_vt = PN_VTABLE(PN_TVTABLE);
  potion_type_call_is(mixin_vt, PN_FUNC(potion_object_new, 0));
  potion_method(mixin_vt, "meta", potion_get_metaclass, 0);
  
  potion_method(P->lobby, "about", potion_about, 0);
  potion_method(P->lobby, "here", potion_callcc, 0);
  potion_method(P->lobby, "exit", potion_exit, 0);
  potion_method(P->lobby, "kind", potion_lobby_kind, 0);
  potion_method(P->lobby, "srand", potion_srand, "seed=N");
  potion_method(P->lobby, "rand", potion_rand, 0);
  potion_method(P->lobby, "self", potion_lobby_self, 0);
  potion_method(P->lobby, "string", potion_lobby_string, 0);
}
