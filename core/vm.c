/**\file vm.c
the vm execution loop, the "bytecode interpreter". correct but slower than the jit.

usage: -B or automatically when no jit is available for your architecture.

Potion uses a register-based bytecode VM that is nearly a
word-for-word copy of Lua's. The code is all different, but the
bytecode is nearly identical.  See
http://luaforge.net/docman/83/98/ANoFrillsIntroToLua51VMInstructions.pdf
or http://www.lua.org/doc/jucs05.pdf

  - MOVE (pos)		 	copy value between registers
  - LOADPN (pos)	 	load a PN value into a register
  - LOADK (pos, need)  	 	load a constant into a register
  - SELF (pos, need)   	 	prepare an object method for calling
   			 	R(A+1) := R(B); R(A) := R(B)[RK(C)]
  - GETLOCAL (pos, regs)	read a local into a register
  - SETLOCAL (pos, regs)	write a register value into a local
  - GETUPVAL (pos, lregs)	read an upvalue (upper scope)
  - SETUPVAL (pos, lregs)	write to an upvalue
  - GLOBAL (pos, need)	 	returns a global (for get or set)
  - NEWTUPLE (pos, need)	create tuple
  - SETTUPLE (pos, need)	write register into tuple key
  - SETTABLE (pos, need)	write register into a table entry
  - NEWLICK (pos, need)	 	create lick. R(A) := {} (size = B,C)
  - GETPATH (pos, need)	 	read obj field into register
  - SETPATH (pos, need)	 	write into obj field
  - ADD (pos, need)	 	a = b + c
  - SUB (pos, need)	 	a = b - c
  - MULT (pos, need)
  - DIV (pos, need)
  - REM (pos, need)
  - POW (pos, need)
  - NEQ (pos)
  - EQ (pos)		 	if ((RK(B) == RK(C) ~= A) then PC++
  - LT (pos)		 	if ((RK(B) < RK(C) ~= A) then PC++
  - LTE (pos)		 	if ((RK(B) <= RK(C) ~= A) then PC++
  - GT (pos)
  - GTE (pos)
  - BITN (pos, need)
  - BITL (pos, need)
  - BITR (pos, need)
  - DEF (pos, need)	 	define a method for an object
  - BIND (pos, need)   	 	extend obj by set a binding
   				http://piumarta.com/software/cola/colas-whitepaper.pdf
  - MSG (pos, need)	 	call a method of an object
  - JMP (pos, jmps, offs, &jmpc) PC += sBx
  - TEST (pos)		 	if not (R(A) <=> C) then PC++
  - NOT (pos)		 	a = not b
  - CMP (pos)
  - TESTJMP (pos, jmps, offs, &jmpc)
  - NOTJMP (pos, jmps, offs, &jmpc)
  - NAMED (pos, need)	 	assign named args before a CALL
  - CALL (pos, need)	 	call a function. R(A),...:= R(A)(R(A+1),...,R(A+B-1)
  - CALLSET (pos, need)		? set return register to write to
  - RETURN (pos)	 	return R(A), ... ,R(A+B-2)
  - PROTO (&pos, lregs, need, regs) define function prototype
  - CLASS (pos, need)    	find class for register value
  - DEBUG (pos, need)	        set lineno and filename

(c) 2008 why the lucky stiff, the freelance professor
(c) 2013 by perl11 org
*/
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include "p2.h"
#include "internal.h"
#include "opcodes.h"
#include "asm.h"
#include "khash.h"
#include "table.h"

#if defined(POTION_JIT_TARGET) && defined(JIT_DEBUG)
#  if defined(HAVE_LIBDISASM)
#    include <libdis.h>
#  else
#    if defined(HAVE_LIBUDIS86)
#      include <udis86.h>
#    endif
// libdistorm64 has no usable headers yet
#  endif
#endif

#if DEBUG
extern const struct {
  const char *name;
  const u8 args;
} potion_ops[];

#define STRINGIFY(_obj) PN_STR_PTR(potion_send(_obj, PN_string))
#endif

#ifdef POTION_JIT_TARGET
#if (POTION_JIT_TARGET == POTION_X86)
extern PNTarget potion_target_x86;
#elif (POTION_JIT_TARGET == POTION_PPC)
extern PNTarget potion_target_ppc;
#elif (POTION_JIT_TARGET == POTION_ARM)
extern PNTarget potion_target_arm;
#endif
#endif

void potion_vm_init(Potion *P) {
#ifdef POTION_JIT_TARGET
#if (POTION_JIT_TARGET == POTION_X86)
  P->target = potion_target_x86;
#elif (POTION_JIT_TARGET == POTION_PPC)
  P->target = potion_target_ppc;
#elif (POTION_JIT_TARGET == POTION_ARM)
  P->target = potion_target_arm;
#endif
#endif
}

/**
 entrypoint for all bytecode methods from the C api. \see potion_test_eval()
 \param cl PNClosure to be called
 \param self PN - sets self (ignored for most user defined functions).
                  if self is a int < 10 it defines the number of args provided
 \param ... - arguments to the cl
x
 \verbatim
   add = potion_eval(P, potion_str(P, "(x=N|y=N): x + y."));
   addfn = PN_CLOSURE_F(add); // i.e. potion_vm_proto
   num = addfn(P, add, 2, PN_NUM(3), PN_NUM(5));
   num = addfn(P, add, 1, PN_NUM(3)); // 1 arg provided, 2nd arg=0
 \endverbatim

 Note that methods with optional arguments ... are not very safe to call.
 potion_vm_proto() does not know the number of arguments on the stack.
 So it checks for all optional args the matching type.
 You can help by providing numargs as self argument if numargs < 10. */
PN potion_vm_proto(Potion *P, PN cl, PN self, ...) {
  PN ary = PN_NIL;
  vPN(Proto) f = (struct PNProto *)PN_CLOSURE(cl)->data[0];
  if (PN_IS_TUPLE(f->sig)) {
    PN_SIZE i;
    int arity = PN_CLOSURE(cl)->arity;
    int minargs = PN_CLOSURE(cl)->minargs;
    int numargs = 0;
    va_list args;
    ary = PN_TUP0();
    va_start(args, self);
    if (self < 10) {
      numargs = self;
      self = 0;
    }
    for (i=0; i < arity; i++) {
      PN s = potion_sig_at(P, f->sig, i);
      if (i < minargs || PN_TUPLE_LEN(s) < 2) { //mandatory or no type
        ary = PN_PUSH(ary, va_arg(args, PN));
      } else { //vararg call heuristic: check type of stack var, replace with default
        PN arg = va_arg(args, PN);
        char type = (char)(PN_TUPLE_LEN(s) > 2
			   ? potion_type_char(PN_TYPE(PN_TUPLE_AT(s,2)))
			   : PN_TUPLE_LEN(s) > 1
			     ? PN_INT(PN_TUPLE_AT(s,1)) : 0);
        if ((numargs && (i >= numargs)) //numargs provided via self
	  || PN_IS_FFIPTR(arg)
	  || (type && (potion_type_char(PN_TYPE(arg)) != type))) { //replace with default
          // default value or 0
          ary = PN_PUSH(ary, PN_TUPLE_LEN(s) == 3 ? PN_TUPLE_AT(s,2) : potion_type_default(type));
        } else {
          ary = PN_PUSH(ary, arg);
        }
      }
    }
    va_end(args);
  }
  return potion_vm(P, (PN)f, self, ary,
    PN_CLOSURE(cl)->extra - 1, &PN_CLOSURE(cl)->data[1]);
}

/** implements the class op */
PN potion_vm_class(Potion *P, PN cl, PN self) {
  if (PN_TYPE(cl) == PN_TCLOSURE) {
    vPN(Proto) proto = PN_PROTO(PN_CLOSURE(cl)->data[0]);
    PN ivars = potion_tuple_with_size(P, PN_TUPLE_LEN(proto->paths));
    PN_TUPLE_EACH(proto->paths, i, v, {
      PN_TUPLE_AT(ivars, i) = PN_TUPLE_AT(proto->values, PN_INT(v));
    });
    return potion_class(P, cl, self, ivars);
  }

  return potion_class(P, PN_NIL, self, cl);
}

#define STACK_MAX 4096
#define JUMPS_MAX 1024

#define CASE_OP(name, args) case OP_##name: target->op[OP_##name]args; break;

PN_F potion_jit_proto(Potion *P, PN proto) {
  long regs = 0, lregs = 0, need = 0, rsp = 0, argx = 0, protoargs = 4;
  PN_SIZE pos;
  PNJumps jmps[JUMPS_MAX]; size_t offs[JUMPS_MAX]; int jmpc = 0, jmpi = 0;
  vPN(Proto) f = (struct PNProto *)proto;
  long upc = PN_TUPLE_LEN(f->upvals);
  PNAsm * volatile asmb = potion_asm_new(P);
  u8 *fn;
  PNTarget *target = &P->target;
  target->setup(P, f, &asmb);
  DBG_t("-- run-time --\n");

  // calculate needed stackspace. nested protos may need more.
  if (PN_TUPLE_LEN(f->protos) > 0) {
    PN_SIZE j;
    vPN(Tuple) tp = (vPN(Tuple)) potion_fwd(f->protos);
    DBG_vt(";  %d subprotos\n", tp->len);
    for (j=0; j < tp->len; j++) {
      PN proto2 = (PN)tp->set[j];
      vPN(Proto) f2 = (struct PNProto *)proto2;
      long p2args;
      f2->arity = potion_sig_arity(P, f2->sig);
      p2args = 3 + f2->arity;
      if (f2->jit == NULL)
        potion_jit_proto(P, proto2);
      if (p2args > protoargs) {
	DBG_vt(";  extend stack from %ld to %ld\n", protoargs, p2args);
        protoargs = p2args;
      }
    }
  }

  regs = PN_INT(f->stack);
  lregs = regs + PN_TUPLE_LEN(f->locals);
  need = lregs + upc + 3;
  rsp = (need + protoargs) * sizeof(PN);

  target->stack(P, f, &asmb, rsp);
  target->registers(P, f, &asmb, need);

  // Read locals
  if (PN_IS_TUPLE(f->sig)) {
    PN_SIZE i;
    vPN(Tuple) t = (vPN(Tuple)) potion_fwd(f->sig);
    for (i=0; i < t->len; i++) {
      PN v = (PN)t->set[i];
      // arg names, except string default
      if (PN_IS_STR(v) && !(i>0 && PN_IS_NUM(t->set[i-1]) && t->set[i-1] == PN_NUM(':'))) {
        PN_SIZE num = PN_GET(f->locals, v);
        target->local(P, f, &asmb, regs + num, argx);
        argx++;
      }
    }
  }
  DBG_t("; %ld locals, %ld regs, %ld upc, sig=%s\n", argx, regs, upc, AS_STR(f->sig));
  f->arity = argx;

  // if CL passed in with upvals, load them
  if (upc > 0)
    target->upvals(P, f, &asmb, lregs, need, upc);

  for (pos = 0; pos < PN_FLEX_SIZE(f->asmb) / sizeof(PN_OP); pos++) {
    offs[pos] = asmb->len;
    for (jmpi = 0; jmpi < jmpc; jmpi++) {
      if (jmps[jmpi].to == pos) {
        unsigned char *asmj = asmb->ptr + jmps[jmpi].from;
        target->jmpedit(P, f, &asmb, asmj, asmb->len - (jmps[jmpi].from + 4));
      }
    }

    // TODO: cgoto (does not check boundaries, est. ~10-20% faster)
    switch (PN_OP_AT(f->asmb, pos).code) {
      CASE_OP(MOVE, (P, f, &asmb, pos))		// copy value between registers
      CASE_OP(LOADPN, (P, f, &asmb, pos))	// load a value into a register
      CASE_OP(LOADK, (P, f, &asmb, pos, need))  // load a constant into a register
      CASE_OP(SELF, (P, f, &asmb, pos, need))   // prepare an object method for calling
						// R[a+1] := R[b]; R[a] := R[b][RK[c]]
      CASE_OP(GETLOCAL, (P, f, &asmb, pos, regs))// read a local into a register
      CASE_OP(SETLOCAL, (P, f, &asmb, pos, regs))// write a register value into a local
      CASE_OP(GETUPVAL, (P, f, &asmb, pos, lregs))// read an upvalue (upper scope)
      CASE_OP(SETUPVAL, (P, f, &asmb, pos, lregs))// write to an upvalue
      CASE_OP(GLOBAL, (P, f, &asmb, pos, need))	// returns a global (for get or set)
      CASE_OP(NEWTUPLE, (P, f, &asmb, pos, need))// create tuple
      CASE_OP(SETTUPLE, (P, f, &asmb, pos, need))// write register into tuple key
      CASE_OP(SETTABLE, (P, f, &asmb, pos, need))// write register into a table entry
      CASE_OP(NEWLICK, (P, f, &asmb, pos, need))// create lick. R[a] := {} (size = b,c)
      CASE_OP(GETPATH, (P, f, &asmb, pos, need))// read obj field into register
      CASE_OP(SETPATH, (P, f, &asmb, pos, need))// write into obj field
      CASE_OP(ADD, (P, f, &asmb, pos, need))	// a = b + c
      CASE_OP(SUB, (P, f, &asmb, pos, need))	// a = b - c
      CASE_OP(MULT, (P, f, &asmb, pos, need))
      CASE_OP(DIV, (P, f, &asmb, pos, need))
      CASE_OP(REM, (P, f, &asmb, pos, need))
      CASE_OP(POW, (P, f, &asmb, pos, need))
      CASE_OP(NEQ, (P, f, &asmb, pos))
      CASE_OP(EQ, (P, f, &asmb, pos))		// if ((RK[b] == RK[c]) ~= a) then PC++
      CASE_OP(LT, (P, f, &asmb, pos))		// if ((RK[b] <  RK[c]) ~= a) then PC++
      CASE_OP(LTE, (P, f, &asmb, pos))		// if ((RK[b] <= RK[c]) ~= a) then PC++
      CASE_OP(GT, (P, f, &asmb, pos))
      CASE_OP(GTE, (P, f, &asmb, pos))
      CASE_OP(BITN, (P, f, &asmb, pos, need))
      CASE_OP(BITL, (P, f, &asmb, pos, need))
      CASE_OP(BITR, (P, f, &asmb, pos, need))
      CASE_OP(DEF, (P, f, &asmb, pos, need))	// define a method for an object
      CASE_OP(BIND, (P, f, &asmb, pos, need))   // extend obj by set a binding
						// http://piumarta.com/software/cola/colas-whitepaper.pdf
      CASE_OP(MSG, (P, f, &asmb, pos, need))	// call a method of an object
      CASE_OP(JMP, (P, f, &asmb, pos, jmps, offs, &jmpc)) // PC += sBx
      CASE_OP(TEST, (P, f, &asmb, pos))		// if not (R[a] <=> C) then PC++
      CASE_OP(NOT, (P, f, &asmb, pos))		// a = not b
      CASE_OP(CMP, (P, f, &asmb, pos))
      CASE_OP(TESTJMP, (P, f, &asmb, pos, jmps, offs, &jmpc))
      CASE_OP(NOTJMP, (P, f, &asmb, pos, jmps, offs, &jmpc))
      CASE_OP(NAMED, (P, f, &asmb, pos, need))	// assign named args before a CALL
      CASE_OP(CALL, (P, f, &asmb, pos, need))	// call a function. R[a],...:= R[a]( R[a+1],...,R[a+b-1] )
      CASE_OP(CALLSET, (P, f, &asmb, pos, need))//? set return register to write to
      CASE_OP(RETURN, (P, f, &asmb, pos))	// return R[a], ... ,R[a+b-2]
      CASE_OP(PROTO, (P, f, &asmb, &pos, lregs, need, regs))// define function prototype
      CASE_OP(CLASS, (P, f, &asmb, pos, need)) // find class for register value
      CASE_OP(DEBUG, (P, f, &asmb, pos, need)) // set lineno and filename
    }
  }

  target->finish(P, f, &asmb);

  fn = PN_ALLOC_FUNC(asmb->len);
#if defined(JIT_DEBUG)
  if (P->flags & DEBUG_JIT) {
    #include "vm-dis.c"
  }
#endif
  PN_MEMCPY_N(fn, asmb->ptr, u8, asmb->len);

  return f->jit = (PN_F)fn;
}

#define PN_VM_MATH(name, oper) \
  if (PN_IS_NUM(reg[op.a]) && PN_IS_NUM(reg[op.b])) \
    reg[op.a] = PN_NUM(PN_INT(reg[op.a]) oper PN_INT(reg[op.b])); \
  else \
    reg[op.a] = potion_obj_##name(P, reg[op.a], reg[op.b]);

/** the bytecode run-loop */
PN potion_vm(Potion *P, PN proto, PN self, PN vargs, PN_SIZE upc, PN *upargs) {
  vPN(Proto) f = (struct PNProto *)proto;

  // these variables persist as we jump around
  PN stack[STACK_MAX];
  PN val = PN_NIL;

  // these variables change from proto to proto
  // current = upvals | locals | self | reg
  PN_SIZE pos = 0;
  PN *args = NULL, *upvals, *locals, *reg;
  PN *current = stack;

  if (vargs != PN_NIL) args = PN_GET_TUPLE(vargs)->set;
  memset((void*)stack, 0, STACK_MAX*sizeof(PN));

reentry:
  if (current - stack >= STACK_MAX) {
    potion_fatal("Out of stack, all registers used up!");
  }

  upvals = current;
  locals = upvals + f->upvalsize;
  reg = locals + f->localsize + 1;

  if (pos == 0) {
    reg[-1] = reg[0] = self;
    if (upc > 0 && upargs != NULL) {
      PN_SIZE i;
      for (i = 0; i < upc; i++) {
        upvals[i] = upargs[i];
      }
    }
    if (args != NULL) {
      long argx;
      for (argx = 0; argx < potion_sig_arity(P, f->sig); argx++) {
        PN s = potion_sig_at(P, f->sig, argx);
        PN_SIZE num = PN_GET(f->locals, PN_TUPLE_AT(s, 0));
        locals[num] = args[argx];
      }
    }
  }
  DBG_t("-- run-time --\n");

  while (pos < PN_OP_LEN(f->asmb)) {
    PN_OP op = PN_OP_AT(f->asmb, pos);
    DBG_t("[%2d] %-8s %d ", pos+1, potion_ops[op.code].name, op.a);
#if DEBUG
    if (P->flags & DEBUG_TRACE) {
      if (potion_ops[op.code].args > 1)
	fprintf(stderr, "%d", op.b);
    }
#endif
    // TODO: cgoto (does not check boundaries, est. ~10-20% faster)
    switch (op.code) {
      case OP_MOVE:
        reg[op.a] = reg[op.b];
      break;
      case OP_LOADK:
        reg[op.a] = PN_TUPLE_AT(f->values, op.b);
      break;
      case OP_LOADPN:
        reg[op.a] = (PN)op.b;
      break;
      case OP_SELF:
        reg[op.a] = reg[-1];
      break;
      case OP_GETLOCAL:
        if (PN_IS_REF(locals[op.b]))
          reg[op.a] = PN_DEREF(locals[op.b]);
        else
          reg[op.a] = locals[op.b];
      break;
      case OP_SETLOCAL:
        if (PN_IS_REF(locals[op.b])) {
          PN_DEREF(locals[op.b]) = reg[op.a];
          PN_TOUCH(locals[op.b]);
        } else
          locals[op.b] = reg[op.a];
      break;
      case OP_GETUPVAL:
        reg[op.a] = PN_DEREF(upvals[op.b]);
      break;
      case OP_SETUPVAL:
        PN_DEREF(upvals[op.b]) = reg[op.a];
        PN_TOUCH(upvals[op.b]);
      break;
      case OP_GLOBAL:
        potion_define_global(P, reg[op.a], reg[op.b]);
        reg[op.a] = reg[op.b];
      break;
      case OP_NEWTUPLE:
        reg[op.a] = PN_TUP0();
      break;
      case OP_SETTUPLE:
        reg[op.a] = PN_PUSH(reg[op.a], reg[op.b]);
      break;
      case OP_SETTABLE:
        potion_table_set(P, reg[op.a], reg[op.a + 1], reg[op.b]);
      break;
      case OP_NEWLICK: {
        PN attr = op.b > op.a ? reg[op.a + 1] : PN_NIL;
        PN inner = op.b > op.a + 1 ? reg[op.b] : PN_NIL;
        reg[op.a] = potion_lick(P, reg[op.a], attr, inner);
      }
      break;
      case OP_GETPATH:
        reg[op.a] = potion_obj_get(P, PN_NIL, reg[op.a], reg[op.b]);
      break;
      case OP_SETPATH:
        potion_obj_set(P, PN_NIL, reg[op.a], reg[op.a + 1], reg[op.b]);
      break;
      case OP_ADD:
        PN_VM_MATH(add, +);
      break;
      case OP_SUB:
        PN_VM_MATH(sub, -);
      break;
      case OP_MULT:
        PN_VM_MATH(mult, *);
      break;
      case OP_DIV:
        PN_VM_MATH(div, /);
      break;
      case OP_REM:
        PN_VM_MATH(rem, %);
      break;
      case OP_POW:
        reg[op.a] = PN_NUM((int)pow((double)PN_INT(reg[op.a]),
          (double)PN_INT(reg[op.b])));
      break;
      case OP_NOT:
        reg[op.a] = PN_BOOL(!PN_TEST(reg[op.a]));
      break;
      case OP_CMP:
        reg[op.a] = PN_NUM(PN_INT(reg[op.b]) - PN_INT(reg[op.a]));
      break;
      case OP_NEQ:
        reg[op.a] = PN_BOOL(reg[op.a] != reg[op.b]);
      break;
      case OP_EQ:
        reg[op.a] = PN_BOOL(reg[op.a] == reg[op.b]);
      break;
      case OP_LT:
        reg[op.a] = PN_BOOL((long)(reg[op.a]) < (long)(reg[op.b]));
      break;
      case OP_LTE:
        reg[op.a] = PN_BOOL((long)(reg[op.a]) <= (long)(reg[op.b]));
      break;
      case OP_GT:
        reg[op.a] = PN_BOOL((long)(reg[op.a]) > (long)(reg[op.b]));
      break;
      case OP_GTE:
        reg[op.a] = PN_BOOL((long)(reg[op.a]) >= (long)(reg[op.b]));
      break;
      case OP_BITN:
        reg[op.a] = PN_IS_NUM(reg[op.b]) ? PN_NUM(~PN_INT(reg[op.b])) : potion_obj_bitn(P, reg[op.b]);
      break;
      case OP_BITL:
        PN_VM_MATH(bitl, <<);
      break;
      case OP_BITR:
        PN_VM_MATH(bitr, >>);
      break;
      case OP_DEF:
        reg[op.a] = potion_def_method(P, PN_NIL, reg[op.a], reg[op.a + 1], reg[op.b]);
      break;
      case OP_BIND:
        reg[op.a] = potion_bind(P, reg[op.b], reg[op.a]);
      break;
      case OP_MSG:
        reg[op.a] = potion_message(P, reg[op.b], reg[op.a]);
      break;
      case OP_JMP:
        pos += op.a;
      break;
      case OP_TEST:
        reg[op.a] = PN_BOOL(PN_TEST(reg[op.a]));
      break;
      case OP_TESTJMP:
        if (PN_TEST(reg[op.a])) pos += op.b;
      break;
      case OP_NOTJMP:
        if (!PN_TEST(reg[op.a])) pos += op.b;
      break;
      case OP_NAMED: {
        int x = potion_sig_find(P, reg[op.a], reg[op.b - 1]);
        if (x >= 0) reg[op.a + x + 2] = reg[op.b];
      }
      break;
      case OP_CALL: /* R[a]( R[a+1],...,R[a+b-1] ) */
        switch (PN_TYPE(reg[op.a])) {
          case PN_TVTABLE:
            reg[op.a + 1] = potion_object_new(P, PN_NIL, reg[op.a]);
            reg[op.a] = ((struct PNVtable *)reg[op.a])->ctor;
          case PN_TCLOSURE:
	  {
	    vPN(Closure) cl = PN_CLOSURE(reg[op.a]);
	    int i;
	    PN sig = cl->sig;
	    int numargs = op.b - op.a - 1;
            if (cl->method != (PN_F)potion_vm_proto) { //call into a lib or jit or ffi
              if (PN_IS_TUPLE(sig)) {
		int arity = cl->arity;
                if (numargs > 0) {  //allow fun() to return the closure
                  if (numargs < cl->minargs)
                    return potion_error
                      (P, (cl->minargs == arity
                           ? potion_str_format(P, "Not enough arguments to %s. Required %d, given %d",
                                               AS_STR(cl), arity, numargs)
                           : potion_str_format(P, "Not enough arguments to %s. Required %d to %d, given %d",
                                               AS_STR(cl), cl->minargs, arity, numargs)),
                       0, 0, 0);
                  if (numargs > arity)
                    return potion_error
                      (P, potion_str_format(P, "Too many arguments to %s. Allowed %d, given %d",
                                            AS_STR(cl), arity, numargs), 0, 0, 0);
                }
                for (i=numargs; i < arity; i++) { // fill in defaults
                  PN s = potion_sig_at(P, sig, i);
                  if (s) // default or zero: && !filled by NAMED (?)
                    reg[op.a + i + 2] = PN_TUPLE_LEN(s) == 3 ? PN_TUPLE_AT(s, 2) : potion_type_default(PN_INT(PN_TUPLE_AT(s,1)));
                  op.b++;
                }
              }
              reg[op.a] = potion_call(P, reg[op.a], op.b - op.a, reg + op.a + 1);
            } else if (((reg - stack) + PN_INT(f->stack) + f->upvalsize + f->localsize + 8) >= STACK_MAX) {
              PN argt = potion_tuple_with_size(P, (op.b - op.a) - 1);
              for (i = 2; i < op.b - op.a; i++)
                PN_TUPLE_AT(argt, i - 2) = reg[op.a + i];
              reg[op.a] = potion_vm(P, cl->data[0], reg[op.a + 1], argt,
                cl->extra - 1, &cl->data[1]);
            } else {
              self = reg[op.a + 1];
              args = &reg[op.a + 2];
              if (PN_IS_TUPLE(sig)) {
		int arity = cl->arity;
                if (numargs > 0) {  //allow fun() to return the closure
                  if (numargs < cl->minargs)
                    return potion_error
                      (P, (cl->minargs == arity
                           ? potion_str_format(P, "Not enough arguments to %s. Required %d, given %d",
                                               AS_STR(cl), arity, numargs)
                           : potion_str_format(P, "Not enough arguments to %s. Required %d to %d, given %d",
                                               AS_STR(cl), cl->minargs, arity, numargs)),
                       0, 0, 0);
                  if (numargs > arity)
                    return potion_error
                      (P, potion_str_format(P, "Too many arguments to %s. Allowed %d, given %d",
                                            AS_STR(cl), arity, numargs), 0, 0, 0);
                }
                for (i=numargs; i < arity; i++) { // fill in defaults
                  PN s = potion_sig_at(P, sig, i);
                  if (s) // default or zero: && !filled by NAMED (?)
                    reg[op.a + i + 2] = PN_TUPLE_LEN(s) == 3 ? PN_TUPLE_AT(s, 2) : potion_type_default(PN_INT(PN_TUPLE_AT(s,1)));
                  f->stack = PN_NUM(PN_INT(f->stack)+1);
                  op.b++;
                }
              }
              upc = cl->extra - 1;
              upargs = &cl->data[1];
              current = reg + PN_INT(f->stack) + 2;
              current[-2] = (PN)f;
              current[-1] = (PN)pos;

              f = PN_PROTO(cl->data[0]);
              pos = 0;
	      DBG_t("\t; %s\n", STRINGIFY(reg[op.a]));
              goto reentry;
            }
	  }
          break;
          
          default: {
            reg[op.a + 1] = reg[op.a];
            reg[op.a] = potion_obj_get_call(P, reg[op.a]);
            if (PN_IS_CLOSURE(reg[op.a]))
              reg[op.a] = potion_call(P, reg[op.a], op.b - op.a, &reg[op.a + 1]);
	      DBG_t("\t; %s\n", STRINGIFY(reg[op.a]));
          }
          break;
        }
      break;
      case OP_CALLSET:
        reg[op.a] = potion_obj_get_callset(P, reg[op.b]);
      break;
      case OP_RETURN:
        if (current != stack) {
          val = reg[op.a];

          f = PN_PROTO(current[-2]);
          pos = (PN_SIZE)current[-1];
          op = PN_OP_AT(f->asmb, pos);

          reg = current - (PN_INT(f->stack) + 2);
          current = reg - (f->localsize + f->upvalsize + 1);
          reg[op.a] = val;
          pos++;
	  DBG_t("\t; %s\n", STRINGIFY(val));
          goto reentry;
        } else {
          reg[0] = reg[op.a];
	  DBG_t("\t; %s\n", STRINGIFY(reg[op.a]));
          goto done;
        }
      break;
      case OP_PROTO: {
        vPN(Closure) cl;
        unsigned areg = op.a;
        proto = PN_TUPLE_AT(f->protos, op.b);
        cl = (struct PNClosure *)potion_closure_new(P, (PN_F)potion_vm_proto,
          PN_PROTO(proto)->sig, PN_TUPLE_LEN(PN_PROTO(proto)->upvals) + 1);
        cl->data[0] = proto;
        PN_TUPLE_COUNT(PN_PROTO(proto)->upvals, i, {
          pos++;
          op = PN_OP_AT(f->asmb, pos);

          if (op.code == OP_GETUPVAL) {
            cl->data[i+1] = upvals[op.b];
          } else if (op.code == OP_GETLOCAL) {
            cl->data[i+1] = locals[op.b] = (PN)potion_ref(P, locals[op.b]);
          } else {
            fprintf(stderr, "** missing an upval to proto %p\n", (void *)proto);
          }
        });
        reg[areg] = (PN)cl;
      }
      break;
      case OP_CLASS:
        reg[op.a] = potion_vm_class(P, reg[op.b], reg[op.a]);
      break;
      case OP_DEBUG:
	//check breakpoints vs lineno (a), switch to debugger
        //lineno: reg[op.a], filename: reg[op.b]
        reg[op.a] = reg[-1];
      break;
    }
#ifdef DEBUG
    if (P->flags & DEBUG_TRACE) {
      if (op.code == OP_JMP || op.code == OP_NOTJMP || op.code == OP_TESTJMP)
	fprintf(stderr, "\n");
      else
	fprintf(stderr, "\t; %s\n", STRINGIFY(reg[op.a]));
    }
#endif
    pos++;
  }

done:
  val = reg[0];
  return val;
}
