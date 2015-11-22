/**\file objmodel.c
  objects, classes, types, methods, weakrefs and mop.
  much of this is based on the work of ian piumarta
  <http://www.piumarta.com/pepsi/objmodel.pdf>
  \image html p2-mop.png
*/
//  (c) 2008 why the lucky stiff, the freelance professor
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "potion.h"
#include "internal.h"
#include "khash.h"
#include "table.h"
#include "asm.h"

PN potion_closure_new(Potion *P, PN_F meth, PN sig, PN_SIZE extra) {
  PN_SIZE i; PN cr;
  vPN(Closure) c = PN_ALLOC_N(PN_TCLOSURE, struct PNClosure, extra * sizeof(PN));
  c->method = meth;
  if (PN_IS_TUPLE(sig) && PN_TUPLE_LEN(sig) > 0) {
    c->sig = sig;
    c->arity = potion_sig_arity(P, sig);
    c->minargs = potion_sig_minargs(P, sig);
  } else {
    c->sig = PN_NIL;
    c->arity = 0;
    c->minargs = 0;
  }
  c->extra = extra;
  for (i = 0; i < c->extra; i++)
    c->data[i] = PN_NIL;
  cr = (PN)c;
  return cr;
}

/**\memberof PNClosure
 "send" method, call a method on self by name
 \param self
 \param method
 \return the result of the method call */
PN potion_closure_code(Potion *P, PN cl, PN self) {
  if (PN_CLOSURE(self)->extra > 0 && PN_IS_PROTO(PN_CLOSURE(self)->data[0])) 
    return PN_CLOSURE(self)->data[0];
  return PN_NIL;
}

/**\memberof PNClosure
 "string" of a function proto, unserializable. without body.
 \param maxlen (ignored)
 \return rough protoype as string. "function(arg1, arg2)"  */
PN potion_closure_string(Potion *P, PN cl, PN self, PN maxlen) {
  PN out = potion_byte_str(P, "function");
  if (PN_CLOSURE(self)->name)
    pn_printf(P, out, " %s", PN_STR_PTR(PN_CLOSURE(self)->name));
  pn_printf(P, out, "(");
  potion_bytes_obj_string(P, out, potion_sig_string(P,cl,PN_CLOSURE(self)->sig));
  pn_printf(P, out, ")");
  return PN_STR_B(out);
}
/**\memberof PNClosure
 "arity" method, optional args do count.
 \return number of args as PNInteger */
PN potion_closure_arity(Potion *P, PN cl, PN self) {
  /// closure_new aka PN_FUNC sets arity, always use the cached value
  return PN_NUM(PN_CLOSURE(self)->arity);
}
/**\memberof PNClosure
 Number of mandatory arguments, without optional args.
 \return number of args as PNInteger */
PN potion_closure_minargs(Potion *P, PN cl, PN self) {
  /// closure_new aka PN_FUNC sets arity, always use the cached value
  return PN_NUM(PN_CLOSURE(self)->minargs);
}

/** number of args of sig tuple, implements the potion_closure_arity method.
    sigs are encoded as tuples of len 2-3, (name type|modifier [default-value])
    names String, modifiers Num.
    types also Num, but want to switch to special SIG VTable.
    default-value can be a value of any type and is prefixed with ':'
  \return number of args as integer */
int potion_sig_arity(Potion *P, PN sig) {
  if (PN_IS_TUPLE(sig)) {
    //return PN_TUPLE_LEN(PN_CLOSURE(closure)->sig) / 2;
    int count = 0;
    struct PNTuple * volatile t = (struct PNTuple *)potion_fwd(sig);
    if (t->len != 0) {
      PN_SIZE i;
      for (i = 0; i < t->len; i++) {
	PN v = (PN)t->set[i];
	if (PN_IS_STR(v)) count++; // names
	// but not string default values
	if (PN_IS_INT(v) && v == PN_NUM(':') && PN_IS_STR((PN)t->set[i+1])) count--;
      }
    }
    return count;
  }
  else if (sig == PN_NIL)
    return 0;
  else {
    potion_fatal("Invalid signature type for sig_arity");
    return 0;
  }
}
/** number of mandatory args, without any optional arguments
  \return number of args as integer */
int potion_sig_minargs(Potion *P, PN sig) {
  if (PN_IS_TUPLE(sig)) {
    int count = 0;
    struct PNTuple * volatile t = (struct PNTuple *)potion_fwd(sig);
    if (t->len != 0) {
      PN_SIZE i;
      for (i = 0; i < t->len; i++) {
	PN v = (PN)t->set[i];
	if (PN_IS_STR(v)) count++; // count only names
	if (PN_IS_INT(v) && v == PN_NUM('|')) break;
	if (PN_IS_INT(v) && v == PN_NUM(':')) { count--; break; }
      }
    }
    return count;
  }
  else if (sig == PN_NIL)
    return 0;
  else {
    potion_fatal("wrong sig type for sig_minargs");
    return 0;
  }
}

///\return sig tuple at index, zero-based
PN potion_sig_at(Potion *P, PN sig, int index) {
  PN result = PN_NIL;
  if (PN_IS_TUPLE(sig)) {
    int count = -1;
    struct PNTuple * volatile t = (struct PNTuple *)potion_fwd(sig);
    if (t->len > 0) {
      PN_SIZE i;
      for (i = 0; i < t->len; i++) {
	PN v = (PN)t->set[i];
	if (PN_IS_STR(v)) count++;
	if (PN_IS_INT(v) && v == PN_NUM(':') && PN_IS_STR((PN)t->set[i+1])) count--;
	if (count == index) {
	  result = potion_tuple_new(P, v);
	  if (i+1 < t->len && PN_IS_INT((PN)t->set[i+1])) {
	    PN typ = (PN)t->set[i+1];
	    result = potion_tuple_push(P, result, typ);
	    if (i+2 < t->len && typ == PN_NUM(':'))
	      result = potion_tuple_push(P, result, (PN)t->set[i+2]);
	  }
	  return result;
	}
      }
    }
    return result;
  }
  else if (sig == PN_NIL)
    return result;
  else {
    potion_fatal("wrong sig type for sig_at");
    return 0;
  }
}

///\return sig name at index, zero-based
PN potion_sig_name_at(Potion *P, PN sig, int index) {
  if (PN_IS_TUPLE(sig)) {
    int count = -1;
    struct PNTuple * volatile t = (struct PNTuple *)potion_fwd(sig);
    if (t->len > 0) {
      PN_SIZE i;
      for (i = 0; i < t->len; i++) {
	PN v = (PN)t->set[i];
	if (PN_IS_STR(v)) count++;
	if (PN_IS_INT(v) && v == PN_NUM(':') && PN_IS_STR((PN)t->set[i+1])) count--;
	if (count == index)
	  return v;
      }
    }
    return 0;
  }
  else if (sig == PN_NIL)
    return 0;
  else {
    potion_fatal("wrong sig type for sig_at");
    return 0;
  }
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
  meta->meta = NULL;
  PN_TOUCH(P->vts);
}
/// create a non-user type, derived from self
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
/// create a named type
PN potion_type_new2(Potion *P, PNType t, PN self, PN name) {
  vPN(Vtable) vt = (vPN(Vtable))potion_type_new(P, t, self);
  vt->name = name;
  return (PN)vt;
}
/// sets the default call method of the PNVtable
void potion_type_call_is(PN vt, PN cl) {
  ((struct PNVtable *)vt)->call = cl;
}
/// get the default accessor (usually "at")
PN potion_obj_get_call(Potion *P, PN obj) {
  PN cl = ((struct PNVtable *)PN_VTABLE(PN_TYPE(obj)))->call;
  if (cl == PN_NIL) cl = P->call;
  return cl;
}
/// set default writer
void potion_type_callset_is(PN vt, PN cl) {
  ((struct PNVtable *)vt)->callset = cl;
}
/// get default writer
PN potion_obj_get_callset(Potion *P, PN obj) {
  PN cl = ((struct PNVtable *)PN_VTABLE(PN_TYPE(obj)))->callset;
  if (cl == PN_NIL) cl = P->callset;
  return cl;
}
/// set default constructor
void potion_type_constructor_is(PN vt, PN cl) {
  ((struct PNVtable *)vt)->ctor = cl;
}
/// create a user-class (ie type)
///\param cl:   PNClosure or nil, set the ctor, uses the parent ctor if nil
///\param self: PNVtable the parent class, lobby or another type
///\param ivars: PNTuple of object members
PN potion_class(Potion *P, PN cl, PN self, PN ivars) {
  PN parent = ((!self || self == P->lobby) ? PN_VTABLE(PN_TOBJECT) : self);
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

/// find class by name. At first only system metaclasses (types), no user classes yet.
PN potion_class_find(Potion *P, PN name) {
  int i;
  for (i=0; i < PN_FLEX_SIZE(P->vts); i++) {
    vPN(Vtable) vt = (struct PNVtable *)PN_FLEX_AT(P->vts, i);
    if (vt && vt->name == name)
      return (PN)vt;
  }
  return PN_NIL;
}
/// \return type for the vtable (struct PNVtable only defined in table.h, not exported)
PNType potion_class_type(Potion *P, PN class) {
  struct PNVtable *vt = (struct PNVtable *)potion_fwd(class);
  return vt->type;
}

PN potion_ivars(Potion *P, PN cl, PN self, PN ivars) {
  struct PNVtable *vt = (struct PNVtable *)self;
#ifdef POTION_JIT_TARGET
  // TODO: allocate assembled instructions together into single pages
  // since many times these tables are <100 bytes.
  PNAsm * volatile asmb = potion_asm_new(P);
  P->target.ivars(P, ivars, &asmb);
  vt->ivfunc = (PN_IVAR_FUNC)PN_ALLOC_FUNC(asmb->len);
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
    if (ivars != PN_NIL) {
      PN found = potion_tuple_bsearch(P, 0, ivars, ivar);
      return found == PN_FALSE ? -1 : found;
    }
  }
  return -1;
}

/// implements OP_GETPATH
PN potion_obj_get(Potion *P, PN cl, PN self, PN ivar) {
  long i = potion_obj_find_ivar(P, self, ivar);
  if (i >= 0)
    return ((struct PNObject *)self)->ivars[i];
  return PN_NIL;
}
/// implements OP_SETPATH
PN potion_obj_set(Potion *P, PN cl, PN self, PN ivar, PN value) {
  long i = potion_obj_find_ivar(P, self, ivar);
  if (i >= 0) {
    ((struct PNObject *)self)->ivars[i] = value;
    PN_TOUCH(self);
  }
  return value;
}
/// only used in def_method
PN potion_proto_method(Potion *P, PN cl, PN self, PN args) {
  return potion_vm(P, PN_CLOSURE(cl)->data[0], P->lobby, args, 0, NULL);
}
/// only used in def_method
PN potion_getter_method(Potion *P, PN cl, PN self) {
  return PN_CLOSURE(cl)->data[0];
}
/// define a method for a class
PN potion_def_method(Potion *P, PN closure, PN self, PN key, PN method) {
  int ret;
  PN cl;
  vPN(Vtable) vt = (struct PNVtable *)self;
  if (PN_TVTABLE != PN_TYPE(self)) return potion_type_error_want(P, "self.def_method", self, "VTable (a class)");
  unsigned k = kh_put(PN, vt->methods, key, &ret);
  PN_QUICK_FWD(struct PNTable *, vt->methods);
  PN_TOUCH(vt->methods);

  if (!PN_IS_CLOSURE(method)) {
    if (PN_IS_PROTO(method)) {
      cl = potion_closure_new(P, (PN_F)potion_proto_method, PN_PROTO(method)->sig, 1);
      PN_PROTO(method)->name = key;
    }
    else
      cl = potion_closure_new(P, (PN_F)potion_getter_method, PN_NIL, 1);
    PN_CLOSURE(cl)->data[0] = method;
    PN_CLOSURE(cl)->name = key;
    method = cl;
  }
  else
    PN_CLOSURE(method)->name = key;

  kh_val(PN, vt->methods, k) = method;
  PN_TOUCH(self);

#ifdef JIT_MCACHE
  // TODO: make JIT_MCACHE more flexible, store in fixed gc, see ivfunc TODO also
  // TODO: this is disabled until method weakrefs can be stored in fixed memory
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
/// used in bind and def_method
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

/// find method for given receiver and message (method lookup)
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

PN potion_lobby_methods(Potion *P, PN closure, PN self) {
  PNType t = PN_TYPE(self);
  if (!PN_TYPECHECK(t)) return PN_NIL;
  if (t == PN_TVTABLE && !PN_IS_METACLASS(self)) {
    //self = (PN)((struct PNVtable *)self)->meta;
  } else {
    self = PN_VTABLE(t);
  }
  vPN(Vtable) vt = (struct PNVtable *)self;
  if(vt->methods != NULL)
    return (PN)vt->methods;
  return PN_NIL;
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

/**\memberof PNWeakRef
  PNWeakRef constructor, get or set the attached ref data
 \param data - non-volatile but mutable
 \return if is_ref data returns data, else create and return a new PNWeakRef to data
*/
PN potion_ref(Potion *P, PN data) {
  if (PN_IS_REF(data)) return data;
  vPN(WeakRef) ref = PN_ALLOC(PN_TWEAK, struct PNWeakRef);
  ref->data = data;
  return (PN)ref;
}

/**\memberof PNWeakRef
 "string" of a ref, a PNWeakRef object
 \returns "\<ref>" */
PN potion_ref_string(Potion *P, PN cl, PN self, PN len) {
  return potion_str(P, "<ref>");
}

/**\memberof PNObject
 "string" method
 \return PNString representation of self: \<name ptr> if named or \<object> */
PN potion_object_string(Potion *P, PN cl, vPN(Object) self) {
  struct PNVtable *vt = (struct PNVtable *)PN_VTABLE(self->vt);
  if (vt->name != PN_NIL) {
    PN str = potion_byte_str2(P, NULL, 0);
    pn_printf(P, str, "<%s %lx>", PN_STR_PTR(vt->name), (PN)self);
    return potion_send(str, PN_string);
  }
  
  return potion_str(P, "<object>");
}

/**\memberof PNObject
 \c "forward" method. Does nothing for the base object
 \return PN_NIL and prints "\<#object>" */
PN potion_object_forward(Potion *P, PN cl, PN self, PN method) {
  printf("#<object>");
  return PN_NIL;
}

/**\memberof PNObject
 \c "send" method, call a method on self by name
 \param self
 \param method
 \return the result of the method call */
PN potion_object_send(Potion *P, PN cl, PN self, PN method) {
  return potion_send(self, method);
}

/**\memberof PNVtable
  PNObject constructor.
  Fails on metaclass and type.
  \return PNObject or PN_NIL */
PN potion_object_new(Potion *P, PN cl, PN self) {
  vPN(Vtable) vt = (struct PNVtable *)self;
  if (PN_IS_METACLASS(vt)) // TODO: error
    return PN_NIL;
  if (vt->type == PN_TVTABLE) // TODO: error
    return PN_NIL;
  return (PN)PN_ALLOC_N(vt->type, struct PNObject,
    potion_type_size(P, (struct PNObject *)self) - sizeof(struct PNObject) + vt->ivlen * sizeof(PN));
}
/**\memberof PNObject
   \returns potion object size in as number for the gc
   Note that older versions just returned a raw integer, which made it unusable as method. */
PN potion_object_size(Potion *P, PN cl, PN self) {
  vPN(Object) obj = (struct PNObject *)self;
  if (!PN_TYPECHECK(PN_VTYPE(obj)))
    return PN_ZERO;
  if (!PN_IS_PTR(obj))
    return PN_NUM(sizeof(PN));
  return PN_NUM(sizeof(struct PNObject)
                + (((struct PNVtable *)PN_VTABLE(obj->vt))->ivlen * sizeof(PN)));
}
/**\memberof Lobby
 global \c "isa?" method: kind self == obj kind
 \return returns TRUE or FALSE if the object has the same type as the class */
PN potion_lobby_isa(Potion *P, PN cl, PN self, vPN(Vtable) vtable) {
  PNType t  = PN_TYPE(self);
  PNType t1 = vtable->type;
  PN_CHECK_TYPE(vtable, PN_TVTABLE);
  if (!PN_TYPECHECK(t1)) return potion_type_error(P, (PN)vtable);
  if (PN_IS_PTR(self) && !PN_TYPECHECK(t)) return potion_type_error(P, self);
  return t == t1  ? PN_TRUE : PN_FALSE;
}
/**\memberof PNObject
 \c "subclass?" method (only single inheritence yet)
 TODO: http://www.eecs.berkeley.edu/~jrb/pve/ (fast subclass test via Packed Vector Encoding)
 \return returns TRUE or FALSE if the object derives from the class */
PN potion_object_subclass(Potion *P, PN cl, PN self, vPN(Vtable) vtable) {
  PNType t = PN_TYPE(self);
  PNType t0, p;
  if (potion_lobby_isa(P, cl, self, vtable) == PN_TRUE) return PN_TRUE;
  if (!PN_IS_PTR(self))
    return vtable->type == PN_TNUMBER
      ? (t == PN_TINTEGER ? PN_TRUE : PN_FALSE) // Integer is a subclass of Number
      : PN_FALSE;                               // other primitives have no parents
  t0 = vtable->type;
  while ((p = ((struct PNVtable *)PN_VTABLE(t))->parent)) {
    if (t0 == p) return PN_TRUE;
    t = p;
  }
  return PN_FALSE;
}

/**\memberof PNVtable
   \return metaclass */
PN potion_get_metaclass(Potion *P, PN cl, vPN(Vtable) self) {
  return (PN)self->meta;
}

/**\memberof Lobby
 global \c "self" method, object identity
 \return self */
static PN potion_lobby_self(Potion *P, PN cl, PN self) {
  return self;
}

/**\memberof Lobby
 global \c "string" method
 \return the name of self */
PN potion_lobby_string(Potion *P, PN cl, PN self) {
  PN str = ((struct PNVtable *)self)->name;
  return (void *)str != PN_NIL ? str :
    PN_IS_METACLASS(self) ? potion_str(P, "<metaclass>") : potion_str(P, "<class>");
}

/**\memberof Lobby
 global \c "kind" method
 \return the PNVtable (type) of self */
PN potion_lobby_kind(Potion *P, PN cl, PN self) {
  PNType t = PN_TYPE(self);
  if (!PN_TYPECHECK(t)) return PN_NIL; // TODO: error
  return PN_VTABLE(t);
}

/**\memberof Lobby
 global \c "parent" method
 \return the parent of self */
PN potion_lobby_parent(Potion *P, PN cl, PN self) {
  if (!PN_IS_PTR(self) || self == P->lobby)
    self = potion_lobby_kind(P, cl, self);
  PNType t = ((struct PNVtable *)self)->parent;
  if(!PN_TYPECHECK(t))
    return PN_VTABLE(PN_TLOBBY);
  return PN_VTABLE(((struct PNVtable *)self)->parent);
}

/**\memberof Lobby
 \c "can" the object call the named method?
 \return true or false */
PN potion_lobby_can(Potion *P, PN cl, PN self, PN method) {
  return potion_bind(P, self, method) ? PN_TRUE : PN_FALSE;
}

/**\memberof Lobby
 \c "print" the stringification of any object */
PN potion_lobby_print(Potion *P, PN cl, PN self) {
  return potion_send(potion_send(self, PN_string), PN_print);
}
/**\memberof Lobby
 \c "print" object and newline.
 \returns nil */
PN potion_lobby_say(Potion *P, PN cl, PN self) {
  potion_send(potion_send(self, PN_string), PN_print);
  printf("\n");
  return PN_STR0;
}

static void potion_init_class_reference(Potion *P, PN name, PN vt) {
  char meta_str[strlen("<metaclass: >") + PN_STR_LEN(name) + 1];
  potion_send(P->lobby, PN_def, name, vt);
  ((struct PNVtable *)vt)->name = name;
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

/**\memberof Lobby
 global \c "about" method
 \returns a table with various quotes */
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

/**\memberof Lobby
  global \c "exit" method. \c exit(0) */
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
  potion_method(clo_vt, "minargs", potion_closure_minargs, 0);
  potion_method(ref_vt, "string", potion_ref_string, 0);
  potion_method(obj_vt, "forward", potion_object_forward, 0);
  potion_method(obj_vt, "send", potion_object_send, 0);
  potion_method(obj_vt, "string", potion_object_string, 0);
  potion_method(obj_vt, "size", potion_object_size, 0);
  potion_method(obj_vt, "subclass?", potion_object_subclass, "value=o");
}

/**\class Lobby
 root namespace, the global environment and parent class of all builtins.
 */
void potion_lobby_init(Potion *P) {
  potion_init_class_reference(P, potion_str(P, "Lobby"),        P->lobby);
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
  potion_init_class_reference(P, potion_str(P, "Integer"),      PN_VTABLE(PN_TINTEGER));
  potion_init_class_reference(P, potion_str(P, "Double"),       PN_VTABLE(PN_TDOUBLE));

  P->call = P->callset = PN_FUNC(potion_no_call, 0);
  
  PN mixin_vt = PN_VTABLE(PN_TVTABLE);
  potion_type_call_is(mixin_vt, PN_FUNC(potion_object_new, 0));
  potion_method(mixin_vt, "meta",  potion_get_metaclass, 0);
  
  potion_method(P->lobby, "about", potion_about, 0);
#ifndef DISABLE_CALLCC
  potion_method(P->lobby, "here",  potion_callcc, 0);
#endif
  potion_method(P->lobby, "exit",  potion_exit, 0);
  potion_method(P->lobby, "kind",  potion_lobby_kind, 0);
  potion_method(P->lobby, "isa?",  potion_lobby_isa, "value=o");
  potion_method(P->lobby, "srand", potion_srand, "seed=N");
  potion_method(P->lobby, "rand",  potion_rand, "|bound=N");
  potion_method(P->lobby, "self",  potion_lobby_self, 0);
  potion_method(P->lobby, "string", potion_lobby_string, 0);
  potion_method(P->lobby, "can",   potion_lobby_can, "method=S");
  potion_method(P->lobby, "print", potion_lobby_print, 0);
  potion_method(P->lobby, "say",   potion_lobby_say, 0);
  potion_method(P->lobby, "methods", potion_lobby_methods, 0);
  potion_method(P->lobby, "parent", potion_lobby_parent, 0);
}
