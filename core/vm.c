/**\file vm.c
the vm execution loop, the "bytecode interpreter". correct but slower than the jit.

usage: -B or automatically when no jit is available for your architecture.

Potion uses a register-based bytecode VM that is nearly a
word-for-word copy of Lua's. The code is all different, but the
bytecode is nearly identical.  See
http://luaforge.net/docman/83/98/ANoFrillsIntroToLua51VMInstructions.pdf
or http://www.lua.org/doc/jucs05.pdf

  - MOVE (pos)		 	copy value between registers R(A)=R(B)
  - LOADK (pos, need)  	 	load a constant into a register R(A)=value[B]
  - LOADPN (pos)	 	load a PN value into a register R(A)=B
  - SELF (pos, need)   	 	prepare an object method for calling
   			 	R(A+1) := R(B); R(A) := R(B)[RK(C)]
  - GETLOCAL (pos, regs)	read a local into a register R(A)=local[B])
  - SETLOCAL (pos, regs)	write a register value into a local local[A]=R(B)
  - GETUPVAL (pos, lregs)	read an upvalue (upper scope) R(A)=upval[B]
  - SETUPVAL (pos, lregs)	write to an upvalue upval[A]=R(B)
  - GLOBAL (pos, need)	 	returns a global (for get or set) name R(A)= value R(B)
  - NEWTUPLE (pos, need)	create tuple  R(A)=()
  - GETTUPLE (pos, need)       	get tuple key R(A)=R(A)(B&1024?R(B):B)
  - SETTUPLE (pos, need)	write register into tuple key R(A)=push(R(A), R(B)
  - GETTABLE (pos, need)       	get table entry R(A)=R(A){B}
  - SETTABLE (pos, need)	write register into a table entry R(A) = R(A){R(A+1)} = R(B)
  - NEWLICK (pos, need)	 	create lick. R(A) := {} (size = B,C)
  - GETPATH (pos, need)	 	read obj field obj_get(R(A), R(B)) 
  - SETPATH (pos, need)	 	write into obj field obj_set(R(A), R(A+1), R(B))
  - ADD (pos, need)	 	R(A) = R(A) + R(B)
  - SUB (pos, need)	 	R(A) = R(A) - R(B)
  - MULT (pos, need)                -"-
  - DIV (pos, need)
  - REM (pos, need)
  - POW (pos, need)
  - NOT (pos)		 	R(A) = !R(A)
  - CMP (pos)                   R(A) = int( R(B) - R(A) )
  - NEQ (pos)                   R(A) = R(A) != R(B)
  - EQ (pos)		 	R(A) = R(A) == R(B)
  - LT (pos)		 	R(A) = R(A) < R(B)
  - LTE (pos)		 	R(A) = R(A) <= R(B)
  - GT (pos)                    R(A) = R(A) > R(B)
  - GTE (pos)                   R(A) = R(A) >= R(B)
  - BITN (pos, need)            R(A) = ~R(B)
  - BITL (pos, need)            R(A) = R(A) << R(B)
  - BITR (pos, need)            R(A) = R(A) >> R(B)
  - DEF (pos, need)	 	def_method(R(A), R(A+1), R(B)) define a method for an object 
  - BIND (pos, need)   	 	bind(R(A), R(B)) method lookup for receiver A and message B
   				http://piumarta.com/software/cola/colas-whitepaper.pdf
  - MSG (pos, need)	 	message(R(A), R(B))  call a method of an object
  - JMP (pos, jmps, offs, &jmpc) pos += R(A)
  - TEST (pos)		 	R(A) = bool R(A)
  - TESTJMP (pos, jmps, offs, &jmpc) if R(A) pos += R(B)
  - NOTJMP (pos, jmps, offs, &jmpc)  if !R(A) pos += R(B)
  - NAMED (pos, need)	 	assign named args before a CALL
  - CALL (pos, need)	 	call a function. R(A),...:= R(A)(R(A+1),...,R(A+B-1)
  - CALLSET (pos, need)		get default writer for R(B) R(A)=get_callset(R(B))
  - TAILCALL(pos, need)	 	? jumps back to the function entry (TODO)
  - RETURN (pos)	 	return R(A), ... ,R(A+B-2)
  - PROTO (&pos, lregs, need, regs) define a method prototype
  - CLASS (pos, need)    	R(A) = vm_class(R(B), R(A)) creates class from closure B and parent A
  - DEBUG (pos)	                set ast to lineno

(c) 2008 why the lucky stiff, the freelance professor
(c) 2013-2014 by perl11 org
*/
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include "potion.h"
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

//#define DEBUG_PROTO_DEBUG_LOOP
#define DEBUG_IN_C
#include <dlfcn.h>
#ifdef DEBUG_IN_C
enum {
  DBG_STEP = 0,
  DBG_NEXT,
  DBG_RUN,
  DBG_QUIT,
  DBG_EXIT
};
static PN (*pn_readline)(Potion *, PN, PN, PN);
#else
#include "ast.h"
#endif

#if DEBUG
extern const struct {
  const char *name;
  const u8 args;
} potion_ops[];

#define STRINGIFY(_obj) ({PN str=potion_send(_obj,PN_string);str?PN_STR_PTR(str):"";})
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

/** implements the class op
    creates a class (or type) from a closure and parent class with obj ivars
 */
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
  /* single-byte indirect -(regn)%ebp addressing only works
     up to 14 (64bit) or 29 (32bit) words */
#ifdef DEBUG
  if (((need + 1) * sizeof(PN)) >= 0x7f)
    DBG_c("Warning: %ld registers too many, used %ld, max %d.\n",
	  need-30/(PN_SIZE_T/4)+1, need, 30/(PN_SIZE_T/4)-1);
#endif
  //assert((((need + 1) * sizeof(PN)) < 0x7f));
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
        if (num != PN_NONE)
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

    switch (PN_OP_AT(f->asmb, pos).code) {
      CASE_OP(MOVE, (P, f, &asmb, pos))		// copy value between registers
      CASE_OP(LOADK, (P, f, &asmb, pos, need))  // load a constant into a register
      CASE_OP(LOADPN, (P, f, &asmb, pos))	// load a value into a register
      CASE_OP(SELF, (P, f, &asmb, pos, need))   // prepare an object method for calling
						// R[a+1] := R[b]; R[a] := R[b][RK[c]]
      CASE_OP(GETLOCAL, (P, f, &asmb, pos, regs))// read a local into a register
      CASE_OP(SETLOCAL, (P, f, &asmb, pos, regs))// write a register value into a local
      CASE_OP(GETUPVAL, (P, f, &asmb, pos, lregs))// read an upvalue (upper scope)
      CASE_OP(SETUPVAL, (P, f, &asmb, pos, lregs))// write to an upvalue
      CASE_OP(GLOBAL, (P, f, &asmb, pos, need))	 // returns a global (for get or set)
      CASE_OP(NEWTUPLE, (P, f, &asmb, pos, need))// create tuple
      CASE_OP(GETTUPLE, (P, f, &asmb, pos, need))// get tuple key (fast and unsafe)
      CASE_OP(SETTUPLE, (P, f, &asmb, pos, need))// write register into tuple key
      CASE_OP(GETTABLE, (P, f, &asmb, pos, need))// get table key
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
      //CASE_OP(TAILCALL, (P, f, &asmb, pos, need))//? jump back to the function
      CASE_OP(RETURN, (P, f, &asmb, pos))	// return R[a], ... ,R[a+b-2]
      CASE_OP(PROTO, (P, f, &asmb, &pos, lregs, need, regs))// define function prototype
      CASE_OP(CLASS, (P, f, &asmb, pos, need)) // find class for register value
      //CASE_OP(DEBUG, (P, f, &asmb, pos, need)) // set lineno and filename
      case OP_DEBUG: break; // skip ast debugging in jit
    }
  }

  target->finish(P, f, &asmb);

  fn = (u8*)PN_ALLOC_FUNC(asmb->len);
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

static PN potion_sig_check(Potion *P, struct PNClosure *cl, int arity, int numargs) {
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
  return PN_NIL;
}

PN potion_debug(Potion *P, struct PNProto *f, PN self, PN_OP op, PN* reg, PN* stack) {
  if (P->flags & EXEC_DEBUG) {
    PN ast;
    PN *upvals, *locals;
    PN *current = stack;
    upvals = current;
    locals = upvals + f->upvalsize;

    if (PN_IS_TUPLE(f->debugs) && op.b >= 0 && op.b < PN_TUPLE_LEN(f->debugs))
      ast = PN_TUPLE_AT(f->debugs, op.b);
    else
      ast = PN_NIL;
#ifndef DEBUG_IN_C
    // get AST from debug op
    DBG_t("\nEntering debug loop\n");
    DBG_vt("calling debug loop(src, proto)\n");
    PN flags = (PN)P->flags; P->flags = EXEC_VM; //turn off tracing,debugging,...
# ifdef DEBUG_PROTO_DEBUG_LOOP // This is the planned way to go.
    // debug.pn loaded, debug object in upvals, call the init and loop method on it
    PN debug = potion_message(P, self, PN_STR("debug")); // find debug object =>0
    if (!debug) return PN_NUM(0);
    PN loopmeth = potion_message(P, debug, PN_STR("loop"));
    PN code = potion_vm_proto(P, (PN)PN_CLOSURE_F(loopmeth), debug, ast, (PN)f);
# else
    // avoid the GC-unsafe parser, and there's no other way yet to inject
    // the current AST into the parser. (ast object?)
    // - expr (msg ("debug"), msg ("loop" list (expr (src), ...))
    PN code = (PN)PN_AST_(CODE,
      PN_TUP(PN_AST_(EXPR, PN_PUSH(PN_TUP(PN_AST_(MSG, PN_STR("debug"))),
	PN_AST2_(MSG, PN_STR("loop"),
	  PN_AST_(LIST, PN_PUSH(PN_TUP(PN_AST_(MSG, ast)), PN_AST_(MSG, (PN)f))))))));
    code = potion_send(code, PN_compile, (PN)f, PN_NIL);
    locals = locals;
    code = potion_vm(P, code, P->lobby, PN_NIL, f->upvalsize, upvals);
# endif // DEBUG_PROTO_DEBUG_LOOP
    locals = locals;
    P->flags = flags;
    if (code >= PN_NUM(5)) { // :q, :exit
      P->flags &= ~EXEC_DEBUG);
      if (code == PN_NUM(6)) // :exit
        exit(0);
    }
#else // DEBUG_IN_C This is a hack and will go away
    int loop = 1;
    locals = locals;
    // TODO: check for breakpoints
    vPN(Source) t = (struct PNSource*)ast;
    if (t) {
      if (t->line) {
	PN fn = PN_TUPLE_AT(pn_filenames, t->loc.fileno);
	if (fn) printf("(%s:%d):\t%s\n", PN_STR_PTR(fn), t->loc.lineno, PN_STR_PTR(t->line));
	else    printf("(:%d):\t%s\n", t->loc.lineno, PN_STR_PTR(t->line));
      }
      while (loop) {
	PN str = pn_readline(P, self, self, PN_STRN("> ", 2));
	if (str && potion_cp_strlen_utf8(PN_STR_PTR(str)) > 1
	    && PN_STR_PTR(str)[0] == ':')
	{
	  if (str == PN_STR(":c"))         { break; }
	  else if (str == PN_STR(":q"))    { P->flags -= EXEC_DEBUG; break; }
	  else if (str == PN_STR(":exit")) { exit(0); }
	  else if (str == PN_STR(":h"))    {
	    printf("c readline debugger (no breakpoints and lexical env yet)\n"
		   ":q      quit debugger and continue\n"
		   ":exit   quit debugger and exit\n"
		   ":c      continue\n"
		   ":b line set breakpoint (nyi)\n"
		   ":B line unset breakpoint (nyi)\n"
		   ":n      step to next line (nyi)\n"
		   ":s      step into function (nyi)\n"
		   "/r      registers\n"
		   "/l      locals\n" /* paths in debug */
		   "/u      upvals\n"
		   "/v      values\n"
		   "/p      paths\n"
		   "expr    eval expr");
	  }
	  else {
	    printf("sorry, no debugger commands yet\n");
	  }
	}
	else if (str && str != PN_STR("")) {
	  PN flags = (PN)P->flags; P->flags = (Potion_Flags)EXEC_VM;
#if 0
          vPN(Closure) cl;
          vPN(Tuple) sig;
          vPN(Tuple) regs;
          int i;
          PN_F debug_fn;
          PN_SIZE pos;
          PN oldsig = f->sig;
#endif
	  PN code = potion_parse(P, potion_send(str, PN_STR("bytes")), "-d");

          code = potion_send(code, PN_compile, (PN)f, PN_NIL);
          printf("%s\n", AS_STR(potion_run(P, code, P->flags)));
#if 0
          /* TODO cannot access the current upvals, regs, locals, values, paths yet */
	  sig  = (struct PNTuple *) potion_tuple_with_size(P, 5);
          PN_TUPLE_AT(sig, 0) = PN_STR("r"); //regs
          PN_TUPLE_AT(sig, 1) = PN_STR("l"); //locals
          PN_TUPLE_AT(sig, 2) = PN_STR("u"); //upvals
          PN_TUPLE_AT(sig, 3) = PN_STR("v"); //values
          PN_TUPLE_AT(sig, 4) = PN_STR("p"); //paths
          cl = (struct PNClosure *)potion_closure_new(P, (PN_F)potion_vm_proto, (PN)sig,
                                                      PN_TUPLE_LEN(f->upvals) + 6);
          cl->data[0] = (PN)f;
          pos = 0;
          PN_TUPLE_COUNT(f->upvals, i, {
              pos++;
              op = PN_OP_AT(f->asmb, pos);
              if (op.code == OP_GETUPVAL) {
                cl->data[i+1] = upvals[op.b];
              } else if (op.code == OP_GETLOCAL) {
                cl->data[i+1] = locals[op.b] = (PN)potion_ref(P, locals[op.b]);
              } else {
                fprintf(stderr, "** missing an upval to proto %p\n", (void *)f);
              }
          });
          regs = (struct PNTuple *) potion_tuple_with_size(P, PN_INT(f->stack));
          for (i=0; i < PN_INT(f->stack); i++) { regs->set[i] = reg[i]; }
          debug_fn = PN_CLOSURE_F(cl);
          printf("%s\n", AS_STR(debug_fn(P, (PN)cl, code /*, regs, f->locals,
                                         f->upvals, f->values, f->paths */)));
          f->sig = oldsig;
#endif
	  P->flags = flags;
	}
	else loop=0;
      }
    }
#endif // DEBUG_IN_C
  }
  return PN_NUM(0);
}

/** the bytecode run-loop */
PN potion_vm(Potion *P, PN proto, PN self, PN vargs, PN_SIZE upc, PN *upargs) {
  vPN(Proto) f = (struct PNProto *)proto;

  // these variables persist as we jump around. TODO: static PN (auto-zeroed)
  PN stack[STACK_MAX];
  PN val = PN_NIL;

  // these variables change from proto to proto
  // current = upvals | locals | self | reg
  PN_SIZE pos = 0;
  PN *args = NULL, *upvals, *locals, *reg;
  PN *current = stack;

#ifdef DEBUG_IN_C
  if (P->flags & EXEC_DEBUG) {
    pn_readline = (PN (*)(Potion *, PN, PN, PN))dlsym(RTLD_DEFAULT, "pn_readline");
    if (!pn_readline) {
#ifndef SANDBOX
      void *handle = dlopen(potion_find_file(P,"readline",0), RTLD_LAZY);
      if (!handle) potion_fatal("readline library not loaded");
      pn_readline = (PN (*)(Potion *, PN, PN, PN))dlsym(handle, "pn_readline");
#endif
      if (!pn_readline) potion_fatal("pn_readline function not loaded");
    }
    DBG_t("\nEntering c debug mode");
    printf("\nc debug (:h for help, <enter> for continue)\n");
  }
#endif

  if (vargs != PN_NIL) args = PN_GET_TUPLE(vargs)->set;
  memset((void*)stack, 0, STACK_MAX*sizeof(PN));
  DBG_t("-- run-time --\n");

reentry:
  if (current - stack >= STACK_MAX) { // 4096
    potion_fatal("Out of stack, all registers used up!");
  }

  upvals = current;
  locals = upvals + f->upvalsize;
  reg = locals + f->localsize + 1;

  if (pos == 0) {
    reg[-1] = reg[0] = self;
    //if (f->localsize)
    //  memset((void*)locals, 0, sizeof(PN) * f->localsize);
    if (upc > 0 && upargs != NULL) {
      PN_SIZE i;
      for (i = 0; i < upc; i++) {
        upvals[i] = upargs[i];
      }
    }
    if (args != NULL) {
      long argx;
      for (argx = 0; argx < potion_sig_arity(P, f->sig); argx++) {
        PN s = potion_sig_name_at(P, f->sig, argx);
        PN_SIZE num = PN_GET(f->locals, s);
        if (num != PN_NONE)
          locals[num] = args[argx];
      }
    }
  }

  PN_SIZE len = PN_OP_LEN(f->asmb);
  while (pos < len) {
    PN_OP op = PN_OP_AT(f->asmb, pos);
    DBG_t("[%2d] %-8s %d ", pos+1, potion_ops[op.code].name, op.a);
#ifdef DEBUG
    if (P->flags & DEBUG_TRACE) {
      if (potion_ops[op.code].args > 1)
	fprintf(stderr, "%d", op.b);
      if (op.code == OP_DEBUG) fprintf(stderr, "\n");
    }
#endif

// computed goto jmptable instead of switch does not check boundaries, ~3-11% faster
#ifdef CGOTO
#define L(op) L_##op

    static void *jmptbl[] = {
      &&L(NONE), &&L(MOVE), &&L(LOADK), &&L(LOADPN), &&L(SELF), &&L(NEWTUPLE),
      &&L(GETTUPLE), &&L(SETTUPLE), &&L(GETLOCAL), &&L(SETLOCAL), &&L(GETUPVAL),
      &&L(SETUPVAL), &&L(GLOBAL), &&L(GETTABLE), &&L(SETTABLE), &&L(NEWLICK),
      &&L(GETPATH), &&L(SETPATH), &&L(ADD), &&L(SUB), &&L(MULT), &&L(DIV), &&L(REM),
      &&L(POW), &&L(NOT), &&L(CMP), &&L(EQ), &&L(NEQ), &&L(LT), &&L(LTE), &&L(GT),
      &&L(GTE), &&L(BITN), &&L(BITL), &&L(BITR), &&L(DEF), &&L(BIND), &&L(MSG), &&L(JMP),
      &&L(TEST), &&L(TESTJMP), &&L(NOTJMP), &&L(NAMED), &&L(CALL), &&L(CALLSET),
      &&L(TAILCALL), &&L(RETURN), &&L(PROTO), &&L(CLASS), &&L_DEBUG
    };

#define SWITCH_START(op) goto *jmptbl[op.code];
#define CASE(op, block) L(op): { block; } goto L_end;
#define SWITCH_END      L_end: ;
#else
#define SWITCH_START(op) switch (op.code) {
#define CASE(op, block) case OP_##op: block; break;
#define SWITCH_END       }
#endif

    SWITCH_START(op)
      CASE(MOVE,   reg[op.a] = reg[op.b] )
      CASE(LOADK,  reg[op.a] = PN_TUPLE_AT(f->values, op.b) )
      CASE(LOADPN, reg[op.a] = (PN)op.b )
      CASE(SELF,   reg[op.a] = reg[-1] )
      CASE(GETLOCAL,
        if (PN_IS_REF(locals[op.b])) {
	  DBG_vt(";  deref locals %d\n", op.b);
          reg[op.a] = PN_DEREF(locals[op.b]);
        } else {
          reg[op.a] = locals[op.b];
	}
      )
      CASE(SETLOCAL,
        if (PN_IS_REF(locals[op.b])) {
	  DBG_vt(";  deref locals %d\n", op.b);
          PN_DEREF(locals[op.b]) = reg[op.a];
          PN_TOUCH(locals[op.b]);
        } else
          locals[op.b] = reg[op.a]
      )
      CASE(GETUPVAL, reg[op.a] = PN_DEREF(upvals[op.b]) )
      CASE(SETUPVAL,
        PN_DEREF(upvals[op.b]) = reg[op.a];
        PN_TOUCH(upvals[op.b])
      )
      CASE(GLOBAL,
           potion_define_global(P, reg[op.a], reg[op.b]);
	   reg[op.a] = reg[op.b])
      CASE(NEWTUPLE,
	   reg[op.a] = PN_TUP0())
      CASE(GETTUPLE, /* fast unsafe version, not bound checked */
	   DBG_CHECK_TUPLE(reg[op.a]);
           long i = op.b & ASM_TPL_IMM ? PN_INT(reg[op.b - ASM_TPL_IMM]) : op.b;
	   reg[op.a] =  PN_TUPLE_AT(reg[op.a], i));
      CASE(SETTUPLE,
	   reg[op.a] = PN_PUSH(reg[op.a], reg[op.b]))
      CASE(GETTABLE,
	   reg[op.a] = potion_table_at(P, PN_NIL, reg[op.a], reg[op.b]))
      CASE(SETTABLE,
	   potion_table_set(P, reg[op.a], reg[op.a + 1], reg[op.b]))
      CASE(NEWLICK, {
        PN attr = op.b > op.a ? reg[op.a + 1] : PN_NIL;
        PN inner = op.b > op.a + 1 ? reg[op.b] : PN_NIL;
        reg[op.a] = potion_lick(P, reg[op.a], attr, inner);
      })
      CASE(GETPATH,
	   reg[op.a] = potion_obj_get(P, PN_NIL, reg[op.a], reg[op.b]))
      CASE(SETPATH,
	   potion_obj_set(P, PN_NIL, reg[op.a], reg[op.a + 1], reg[op.b]))
      CASE(ADD, PN_VM_MATH(add, +))
      CASE(SUB, PN_VM_MATH(sub, -))
      CASE(MULT,PN_VM_MATH(mult, *))
      CASE(DIV, PN_VM_MATH(div, /))
      CASE(REM, PN_VM_MATH(rem, %))
      CASE(POW, reg[op.a] = PN_NUM((int)pow((double)PN_INT(reg[op.a]),
					    (double)PN_INT(reg[op.b]))))
#ifdef P2
      CASE(NOT, reg[op.a] = PN_ZERO == reg[op.a] ? PN_TRUE : PN_BOOL(!PN_TEST(reg[op.a])))
#else
      CASE(NOT, reg[op.a] = PN_BOOL(!PN_TEST(reg[op.a])))
#endif
      CASE(CMP, reg[op.a] = PN_NUM(PN_INT(reg[op.b]) - PN_INT(reg[op.a])))
      CASE(NEQ,
           DBG_t("\t; %s!=%s", STRINGIFY(reg[op.a]), STRINGIFY(reg[op.b]));
	   reg[op.a] = PN_BOOL(reg[op.a] != reg[op.b]))
      CASE(EQ,
           DBG_t("\t; %s==%s", STRINGIFY(reg[op.a]), STRINGIFY(reg[op.b]));
	   reg[op.a] = PN_BOOL(reg[op.a] == reg[op.b]))
      CASE(LT,
           DBG_t("\t; %s<%s", STRINGIFY(reg[op.a]), STRINGIFY(reg[op.b]));
	   reg[op.a] = PN_BOOL((long)(reg[op.a]) < (long)(reg[op.b])))
      CASE(LTE,
	   DBG_t("\t; %s<=%s", STRINGIFY(reg[op.a]), STRINGIFY(reg[op.b]));
	   reg[op.a] = PN_BOOL((long)(reg[op.a]) <= (long)(reg[op.b])))
      CASE(GT,
	   DBG_t("\t; %s>%s", STRINGIFY(reg[op.a]), STRINGIFY(reg[op.b]));
	   reg[op.a] = PN_BOOL((long)(reg[op.a]) > (long)(reg[op.b])))
      CASE(GTE,
	   DBG_t("\t; %s>=%s", STRINGIFY(reg[op.a]), STRINGIFY(reg[op.b]));
	   reg[op.a] = PN_BOOL((long)(reg[op.a]) >= (long)(reg[op.b])))
      CASE(BITN,
	   reg[op.a] = PN_IS_NUM(reg[op.b]) ? PN_NUM(~PN_INT(reg[op.b])) : potion_obj_bitn(P, reg[op.b]))
      CASE(BITL, PN_VM_MATH(bitl, <<))
      CASE(BITR, PN_VM_MATH(bitr, >>))
      CASE(DEF,
	   reg[op.a] = potion_def_method(P, PN_NIL, reg[op.a], reg[op.a + 1], reg[op.b]))
      CASE(BIND,
	   reg[op.a] = potion_bind(P, reg[op.b], reg[op.a]))
      CASE(MSG,
	   reg[op.a] = potion_message(P, reg[op.b], reg[op.a]))
      CASE(JMP,
	   pos += op.a)
      CASE(TEST,
	   reg[op.a] = PN_BOOL(PN_TEST1(reg[op.a])))
      CASE(TESTJMP,
	   if (PN_TEST1(reg[op.a])) pos += op.b)
      CASE(NOTJMP,
	   if (!PN_TEST1(reg[op.a])) pos += op.b)
      CASE(NAMED,  {
        int x = potion_sig_find(P, reg[op.a], reg[op.b - 1]);
        if (x >= 0) reg[op.a + x + 2] = reg[op.b];
        else potion_fatal("named parameter not found in signature");
        DBG_t("\t; %s=%s at %d", STRINGIFY(reg[op.b-1]), STRINGIFY(reg[op.b]), x);
	})
      CASE(CALL,  /* R[a]( R[a+1],...,R[a+b-1] ) */
        switch (PN_TYPE(reg[op.a])) {
          case PN_TVTABLE:
	    DBG_vt(" VTABLE\n");
            reg[op.a + 1] = potion_object_new(P, PN_NIL, reg[op.a]);
            reg[op.a] = ((struct PNVtable *)reg[op.a])->ctor;
          case PN_TCLOSURE:
	  {
	    vPN(Closure) cl = PN_CLOSURE(reg[op.a]);
	    int i;
	    PN sig = cl->sig;
	    int numargs = op.b - op.a - 1;
            if (cl->method != (PN_F)potion_vm_proto) { //call into a lib or jit or ffi
	      //DBG_vt(" ext");
              if (PN_IS_TUPLE(sig)) {
		int arity = cl->arity;
		PN err = potion_sig_check(P, cl, arity, numargs);
		if (err) return err;
                for (i=numargs; i < arity; i++) { // fill in defaults
                  PN s = potion_sig_at(P, sig, i);
                  if (s) // default or zero: && !filled by NAMED (?)
                    reg[op.a + i + 2] = PN_TUPLE_LEN(s) == 3
                      ? PN_TUPLE_AT(s, 2)
                      : potion_type_default(PN_INT(PN_TUPLE_AT(s,1)));
                  op.b++;
                }
              }
              reg[op.a] = potion_call(P, reg[op.a], op.b - op.a, reg + op.a + 1);
            } else if (((reg - stack) + PN_INT(f->stack) + f->upvalsize + f->localsize + 8) >= STACK_MAX) {
	      DBG_vt(" >stack");
              PN argt = potion_tuple_with_size(P, (op.b - op.a) - 1);
              for (i = 2; i < op.b - op.a; i++)
                PN_TUPLE_AT(argt, i - 2) = reg[op.a + i];
              reg[op.a] = potion_vm(P, cl->data[0], reg[op.a + 1], argt,
                cl->extra - 1, &cl->data[1]);
            } else {
	      DBG_vt(" stack");
              self = reg[op.a + 1];
              args = &reg[op.a + 2];
              if (PN_IS_TUPLE(sig)) {
		int arity = cl->arity;
		PN err = potion_sig_check(P, cl, arity, numargs);
		if (err) return err;
                for (i=numargs; i < arity; i++) { // fill in defaults. TODO compile-time
                  PN s = potion_sig_at(P, sig, i);
                  if (s) // default or zero: && !filled by NAMED (?)
                    reg[op.a + i + 2] = PN_TUPLE_LEN(s) == 3
		      ? PN_TUPLE_AT(s, 2)
		      : potion_type_default(PN_INT(PN_TUPLE_AT(s,1)));
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
            if (PN_IS_CLOSURE(reg[op.a])) {
	      //DBG_vt(" def");
              reg[op.a] = potion_call(P, reg[op.a], op.b - op.a, &reg[op.a + 1]);
	    }
	    DBG_t("\t; %s\n", STRINGIFY(reg[op.a]));
          }
          break;
        })
      CASE(CALLSET,
	   reg[op.a] = potion_obj_get_callset(P, reg[op.b]))
      CASE(TAILCALL,
	   potion_fatal("OP_TAILCALL not implemented"))
      CASE(NONE, )     // ignored
      CASE(RETURN,
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
      )
      CASE(PROTO, { /* define a method */
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
      })
      CASE(CLASS, reg[op.a] = potion_vm_class(P, reg[op.b], reg[op.a]))
#ifdef CGOTO
      L_DEBUG:
#else
      case OP_DEBUG: 
#endif
        potion_debug(P, f, self, op, reg, stack);

    SWITCH_END

#ifdef DEBUG
    if (P->flags & DEBUG_TRACE) {
      if (op.code == OP_JMP || op.code == OP_NOTJMP || op.code == OP_TESTJMP ||
	  op.code == OP_NAMED)
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
