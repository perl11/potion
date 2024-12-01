/** \file compile.c
 * transform the ast to simple two-address lua-like bytecode.
 * A full three-address VM with possible SSA form is not needed.
 * This is for highly dynamic languages, and the typed parts can be optimized differently.
 *
 * implement the PNSource (AST) and PNProto (closure) methods,
 * special signature handling (parsed extra) and compile, bytecode load and dump methods.
 * Some special control methods are handled here and not in the parser. We do not need 
 * lexed keywords, and are free to extend everything dynamically.
 *
 * (c) 2008 why the lucky stiff, the freelance professor
 * (c) 2014 perl11.org */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include "potion.h"
#include "internal.h"
#include "ast.h"
#include "opcodes.h"
#include "asm.h"
#ifdef WITH_EXTERN
#include <dlfcn.h>
#endif

#define PN_ASM1(ins, _a)     f->asmb = (PN)potion_asm_op(P, (PNAsm *)f->asmb, (u8)ins, (int)_a, 0)
#define PN_ASM2(ins, _a, _b) f->asmb = (PN)potion_asm_op(P, (PNAsm *)f->asmb, (u8)ins, (int)_a, (int)_b)

const struct {
  const char *name;
  const u8 args;
} potion_ops[] = {
  {"noop", 0}, {"move", 2}, {"loadk", 2}, {"loadpn", 2}, {"self", 1},
  {"newtuple", 2}, {"gettuple", 2}, {"settuple", 2}, {"getlocal", 2}, {"setlocal", 2},
  {"getupval", 2}, {"setupval", 2}, {"global", 2}, {"gettable", 2},
  {"settable", 2}, {"newlick", 2}, {"getpath", 2}, {"setpath", 2},
  {"add", 2}, {"sub", 2}, {"mult", 2}, {"div", 2}, {"mod", 2}, {"pow", 2},
  {"not", 1}, {"cmp", 2}, {"eq", 2}, {"neq", 2}, {"lt", 2}, {"lte", 2},
  {"gt", 2}, {"gte", 2}, {"bitn", 2}, {"bitl", 2}, {"bitr", 2}, {"def", 2},
  {"bind", 2}, {"msg", 2}, {"jump", 1}, {"test", 2}, {"testjmp", 2},
  {"notjmp", 2}, {"named", 2}, {"call", 2}, {"callset", 2}, {"tailcall", 2},
  {"return", 1}, {"proto", 2}, {"class", 2}, {"debug", 2}
};

///\memberof PNProto
/// tree method of PNProto
///\return the original PNSource AST for the closure
PN potion_proto_tree(Potion *P, PN cl, PN self) {
  return PN_PROTO(self)->tree;
}

///\memberof PNProto
/// call method of PNProto. i.e. apply - bind args to a closure.
///\param args
PN potion_proto_call(Potion *P, PN cl, PN self, PN args) {
  return potion_vm(P, self, P->lobby, args, 0, NULL);
}

///\return string of the sig tuple. "arg1=o,arg2=o"
PN potion_sig_string(Potion *P, PN cl, PN sig) {
  PN out = potion_byte_str(P, "");
  if (PN_IS_TUPLE(sig)) {
    int nextdef = 0;
    struct PNTuple * volatile t = ((struct PNTuple *)potion_fwd(sig));
    if (t->len != 0) {
      PN_SIZE i, comma=0;
      for (i = 0; i < t->len; i++) {
	PN v = (PN)t->set[i];
	if (PN_IS_INT(v)) {
          // currently types are still encoded as NUM, TODO: support VTABLE also
	  int c = PN_INT(v); comma=0;
	  if (c == '.')      // is end
	    pn_printf(P, out, ".");
	  else if (c == '|') // is optional
	    pn_printf(P, out, "|");
	  else if (c == ':') { nextdef = 1;
	    pn_printf(P, out, ":"); // is default
	  }
	  else {
	    if (comma++) pn_printf(P, out, ",");
	    if (nextdef) { nextdef = 0;
	      pn_printf(P, out, "=");
	      potion_bytes_obj_string(P, out, v);
	    } else
	      pn_printf(P, out, "=%c", c);
	  }
	} else {
	  if (comma++) pn_printf(P, out, ",");
	  if (nextdef)
	    { nextdef = 0; pn_printf(P, out, "="); }
	  potion_bytes_obj_string(P, out, v);
	}}}}
  return PN_STR_B(out);
}

///\memberof PNProto
/// string method of PNProto. ascii dump of a function definition
PN potion_proto_string(Potion *P, PN cl, PN self) {
  vPN(Proto) t = (struct PNProto *)self;
  int x = 0;
  PN_SIZE num = 1;
  PN_SIZE numcols;
  PN out = potion_byte_str(P, "; function definition");
  #ifdef JIT_DEBUG
  pn_printf(P, out, ": %p; %u bytes\n", t, PN_FLEX_SIZE(t->asmb));
  #else
  pn_printf(P, out, ": %u bytes\n", PN_FLEX_SIZE(t->asmb));
  #endif
  if (t->name)
    pn_printf(P, out, "; %s(", PN_STR_PTR(t->name));
  else
    pn_printf(P, out, "; (");
  potion_bytes_obj_string(P, out, potion_sig_string(P, cl, t->sig));
  pn_printf(P, out, ") %ld registers\n", PN_INT(t->stack));
  PN_TUPLE_EACH(t->paths, i, v, {
    pn_printf(P, out, ".path /");
    v = PN_TUPLE_AT(t->values, PN_INT(v));
    potion_bytes_obj_string(P, out, v);
    pn_printf(P, out, " ; %u\n", i);
  });
  PN_TUPLE_EACH(t->locals, i, v, {
    pn_printf(P, out, ".local ");
    potion_bytes_obj_string(P, out, v);
    pn_printf(P, out, " ; %u\n", i);
  });
  PN_TUPLE_EACH(t->upvals, i, v, {
    pn_printf(P, out, ".upval ");
    potion_bytes_obj_string(P, out, v);
    pn_printf(P, out, " ; %u\n", i);
  });
  PN_TUPLE_EACH(t->values, i, v, {
    pn_printf(P, out, ".value ");
    potion_bytes_obj_string(P, out, v);
    pn_printf(P, out, " ; %u\n", i);
  });
  PN_TUPLE_EACH(t->protos, i, v, {
    potion_bytes_obj_string(P, out, v);
  });
  numcols = (int)ceil(log10(PN_FLEX_SIZE(t->asmb) / sizeof(PN_OP)));
  for (x = 0; x < PN_FLEX_SIZE(t->asmb) / sizeof(PN_OP); x++) {
    const int commentoffset = 20;
    int width = pn_printf(P, out, "[%*u] %-8s %d",
      numcols, num, potion_ops[PN_OP_AT(t->asmb, x).code].name, PN_OP_AT(t->asmb, x).a);

    if (potion_ops[PN_OP_AT(t->asmb, x).code].args > 1)
      width += pn_printf(P, out, " %d", PN_OP_AT(t->asmb, x).b);

    if (width < commentoffset)
      pn_printf(P, out, "%*s", commentoffset - width, "");
    else
      pn_printf(P, out, " ");

    // TODO: Byte code listing: instead of using tabs, pad with spaces to make everything line up
    switch (PN_OP_AT(t->asmb, x).code) {
      case OP_JMP:
        pn_printf(P, out, "; to %d", num + PN_OP_AT(t->asmb, x).a + 1);
        break;
      case OP_NOTJMP:
      case OP_TESTJMP:
        pn_printf(P, out, "; to %d", num + PN_OP_AT(t->asmb, x).b + 1);
        break;
      case OP_LOADPN:
        pn_printf(P, out, "; ");
        potion_bytes_obj_string(P, out, PN_OP_AT(t->asmb, x).b);
        break;
      case OP_LOADK:
        pn_printf(P, out, "; ");
        potion_bytes_obj_string(P, out, PN_TUPLE_AT(t->values, PN_OP_AT(t->asmb, x).b));
        break;
      case OP_SETLOCAL:
      case OP_GETLOCAL:
        pn_printf(P, out, "; ");
        potion_bytes_obj_string(P, out, PN_TUPLE_AT(t->locals, PN_OP_AT(t->asmb, x).b));
        break;
    }
    pn_printf(P, out, "\n");
    num++;
  }
  //pn_printf(P, out, "\n");
  pn_printf(P, out, "; function end\n");
  return PN_STR_B(out);
}

#define PN_REG(f, reg) \
  if (reg >= PN_INT(f->stack)) \
    f->stack = PN_NUM(reg + 1)
#define PN_ARG(n, reg) \
  if (PN_PART(t->a[n]) == AST_EXPR && PN_PART(PN_TUPLE_AT(PN_S(t->a[n],0), 0)) == AST_LIST) { \
    PN test = PN_S(PN_TUPLE_AT(PN_S(t->a[n],0), 0), 0);			\
    if (!PN_IS_NIL(test)) {						\
      DBG_c("expr (list %s) => %s\n", AS_STR(test), AS_STR(test));	\
      PN_TUPLE_EACH(test, i, v, {					\
	  potion_source_asmb(P, f, loop, 0, PN_SRC(v), reg); });	\
    }									\
  } else {								\
    potion_source_asmb(P, f, loop, 0, t->a[n], reg);			\
  }
#define PN_BLOCK(reg, blk, sig) ({ \
  PN block = potion_send(blk, PN_compile, (PN)f, sig); \
  PN_SIZE num = PN_PUT(f->protos, block); \
  DBG_c("protos => %u\n", num); \
  PN_ASM2(OP_PROTO, reg, num); \
  PN_TUPLE_EACH(((struct PNProto *)block)->upvals, i, v, { \
    PN_SIZE numup = PN_GET(f->upvals, v); \
    DBG_c("upvals %s <= %d\n", AS_STR(v), (int)numup); \
    if (numup != PN_NONE) PN_ASM2(OP_GETUPVAL, reg, numup); \
    else                  PN_ASM2(OP_GETLOCAL, reg, PN_GET(f->locals, v)); \
  }); \
})
#define PN_UPVAL(name) ({ \
  PN_SIZE numl = PN_GET(f->locals, name); \
  DBG_c("locals %s <= %d\n", AS_STR(name), (int)numl); \
  PN_SIZE numup = PN_NONE; \
  if (numl == PN_NONE) { \
    numup = PN_GET(f->upvals, name); \
    if (numup == PN_NONE) { \
      DBG_c("upvals %s <= -1\n", AS_STR(name));	\
      vPN(Proto) up = f; \
      int depth = 1; \
      while (PN_IS_PROTO(up->source)) { \
        up = (struct PNProto *)up->source; \
        if (PN_NONE != (numup = PN_GET(up->locals, name))) break; \
        depth++; \
      } \
      DBG_c("locals %s <= %d\n", AS_STR(name), (int)numup); \
      if (numup != PN_NONE) { \
        up = f; \
        while (depth--) { \
          up->upvals = PN_PUSH(up->upvals, name); \
	  DBG_c("upvals %s =>\n", AS_STR(name)); \
          up = (struct PNProto *)up->source; \
        } \
      } \
      numup = PN_GET(f->upvals, name); \
      DBG_c("upvals %s <= %d\n", AS_STR(name), (int)numup);	\
    } \
  } \
  numup; \
})
#define PN_ARG_TABLE(args, reg, inc) potion_arg_asmb(P, f, loop, args, &reg, inc)
#define SRC_TUPLE_AT(src,i)  PN_SRC(PN_TUPLE_AT(PN_S(src,0), i))
#define PN_ASM_DEBUG(REG, T) REG = potion_source_debug(P, f, T, REG)

/// insert DEBUG ops for every new line
u8 potion_source_debug(Potion *P, struct PNProto * volatile f, struct PNSource * volatile t, u8 reg) {
  static PNType lineno = 0;
  if ((P->flags & EXEC_DEBUG) && t && t->loc.lineno != lineno && t->loc.lineno > 0) {
    PN_SIZE num = PN_PUT(f->debugs, (PN)t);
    PN_ASM2(OP_DEBUG, reg, num);
    lineno = t->loc.lineno;
    DBG_c("debug %s :%d\n", PN_STR_PTR(potion_send(t, PN_name)), lineno);
  }
  return reg;
}

#define MAX_JUMPS 1024
/// jump table for loops, for the 2 args b and c
/// Note: only statically allocated (max 1024)
struct PNLoop {
  int bjmps[MAX_JUMPS];
  int cjmps[MAX_JUMPS];

  int bjmpc;  ///< count of b jumps (abc regs)
  int cjmpc;  ///< count of c jumps (abc regs)
};

void potion_source_asmb(Potion *, struct PNProto * volatile, struct PNLoop *, PN_SIZE, struct PNSource * volatile, u8);

void potion_arg_asmb(Potion *P, struct PNProto * volatile f, struct PNLoop *loop, PN args, u8 *reg, int inc) {
  if (args != PN_NIL) {
    if (PN_PART(args) == AST_LIST) {
      args = PN_S(args,0);
      if (!PN_IS_NIL(args)) {
        u8 freg = *reg, sreg = *reg + PN_TUPLE_LEN(args) + 1;
	DBG_c("ARGLIST %d %d\n", freg, sreg);
        PN_TUPLE_EACH(args, i, v, {
          if (inc) {
            (*reg)++;
            if (PN_PART(v) == AST_ASSIGN) {
              vPN(Source) lhs = PN_SRC(PN_S(v,0));
	      potion_source_asmb(P, f, loop, 0, PN_SRC(v)->a[1], sreg + 1);
              if (lhs->part == AST_EXPR && PN_TUPLE_LEN(PN_S(lhs,0)) == 1)
              {
                lhs = SRC_TUPLE_AT(lhs, 0);
		PN_ASM_DEBUG(sreg, lhs);
                if (lhs->part == AST_MSG || lhs->part == AST_VALUE) {
                  PN_OP op; op.a = PN_S(lhs,0); //12 bit!
                  if (!PN_IS_PTR(PN_S(lhs,0)) && PN_S(lhs,0) == op.a) {
                    PN_ASM2(OP_LOADPN, sreg, PN_S(lhs,0));
                  } else {
                    PN_SIZE num = PN_PUT(f->values, PN_S(lhs,0));
                    DBG_c("values %d %s => %d\n", sreg, AS_STR(lhs->a[0]), (int)num);
                    PN_ASM2(OP_LOADK, sreg, num);
                  }
                  lhs = NULL;
                }
              }

              if (lhs != NULL)
                potion_source_asmb(P, f, loop, 0, lhs, sreg);

              PN_ASM2(OP_NAMED, freg - 1, sreg + 1);
              PN_REG(f, sreg + 1);
            } else
              potion_source_asmb(P, f, loop, 0, PN_SRC(v), *reg);
          } else
            potion_source_asmb(P, f, loop, 0, PN_SRC(v), *reg);
        });
      }
    } else {
      if (inc) (*reg)++;
      potion_source_asmb(P, f, loop, 0, PN_SRC(args), *reg);
    }
  } else {
    if (inc) (*reg)++;
    PN_ASM_DEBUG(*reg, PN_SRC(args));
    PN_ASM2(OP_LOADPN, *reg, args);
  }
}

void potion_source_asmb(Potion *P, struct PNProto * volatile f, struct PNLoop *loop, PN_SIZE count,
                        struct PNSource * volatile t, u8 reg) {
  //PN fname = 0;
  PN_REG(f, reg);
  PN_ASM_DEBUG(reg, t);
  switch (t->part) {
    case AST_CODE:
    case AST_BLOCK:
      if (PN_S(t,0) != PN_NIL) {
        DBG_c("%s %u %s\n", t->part == AST_CODE?"code":"block", reg,
	      t->part == AST_CODE?"":AS_STR(PN_S(t,0)));
        PN_TUPLE_EACH(PN_S(t,0), i, v, {
          potion_source_asmb(P, f, loop, 0, PN_SRC(v), reg);
        });
      }
    break;

    case AST_EXPR:
      if (PN_S(t,0) != PN_NIL) {
	PN e = PN_S(t,0);
        DBG_c("expr %u %s\n", reg, AS_STR(e));
        PN_TUPLE_EACH(e, i, v, {
	  potion_source_asmb(P, f, loop, i, PN_SRC(v), reg);
        });
      }
    break;

    case AST_PROTO:
      PN_BLOCK(reg, PN_S(t,1), PN_S(t,0));
    break;

    case AST_VALUE: {
      vPN(Source) a = PN_S_(t,0);
      PN_OP op; op.a = PN_S(t,0); // but 12bit only
      if (!PN_IS_PTR(a) && (PN)a == (PN)op.a) {
        PN_ASM2(OP_LOADPN, reg, PN_S(t,0));
      } else if (a != PN_NIL && PN_IS_PTR(a)
                 && PN_PART(a) == AST_LICK && PN_PART(a->a[0]) == AST_MSG) {
        vPN(Source) msg = a->a[0];
        PN tpl = PN_S(msg, 0);
        // locals or upvals. locals alone broke nbody
        PN_SIZE num = PN_UPVAL(tpl);
        u8 opcode =  OP_GETUPVAL;
        PN key = PN_S(PN_TUPLE_AT(a->a[1]->a[0], 0), 0);
        if (num == PN_NONE) {
          num = PN_PUT(f->locals, tpl);
          DBG_c("locals %s => %d\n", AS_STR(tpl), (int)num);
          opcode = OP_GETLOCAL;
        }
        PN_ASM2(opcode, reg, num);
        if (PN_VTYPE(key) == PN_TSTRING) { // a[k] a variable
          num = PN_PUT(f->locals, key);
          DBG_c("locals %s => %d\n", PN_STR_PTR(key), (int)num);
          PN_ASM2(OP_GETLOCAL, reg+1, num);
          num = reg+1;
          DBG_c("gettuple %d %d %s[%s]\n", reg, num, PN_STR_PTR(tpl), PN_STR_PTR(key));
          PN_ASM2(OP_GETTUPLE, reg, num | ASM_TPL_IMM);
          PN_REG(f, reg + 1);
        } else { // a[0] constant, could to be optimized in jit
          assert(PN_VTYPE(key) == PN_TSOURCE && PN_PART(key) == AST_VALUE);
          PN k = PN_S(key, 0);
          if (PN_IS_INT(k)) {
            if (PN_INT(k) >= ASM_TPL_IMM || PN_INT(k) < 0) {
              num = PN_PUT(f->values, k);
              PN_ASM2(OP_LOADK, reg+1, num);
              DBG_c("values %ld => %d\n", PN_INT(k), (int)num);
              num = reg + 1;
              PN_REG(f, reg + 1);
            } else {
              num = PN_INT(k);
            }
            DBG_c("gettuple %d %d %s[%ld]\n", reg, num, PN_STR_PTR(tpl), PN_INT(k));
            PN_ASM2(OP_GETTUPLE, reg, num);
          } else {
            num = PN_PUT(f->values, k); // op.b has 12 bits
            PN_ASM2(OP_LOADK, reg+1, num);
            DBG_c("values \"%s\" => %d\n", PN_STR_PTR(k), (int)num);
            num = reg+1;
            DBG_c("gettable %d %d %s[\"%s\"]\n", reg, num, PN_STR_PTR(tpl), PN_STR_PTR(k));
            PN_ASM2(OP_GETTABLE, reg, num);
            PN_REG(f, reg + 1);
          }
        }
      } else {
        PN_SIZE num = PN_PUT(f->values, (PN)a);
	DBG_c("values %d %s => %d\n", reg, AS_STR(a), (int)num);
        PN_ASM2(OP_LOADK, reg, num);
      }
      if (PN_S(t,1) != PN_NIL && a->part != AST_LICK) {
        u8 breg = reg;
        PN_ASM1(OP_SELF, ++breg);
        PN_ARG_TABLE(PN_S(t,1), breg, 1);
        if (PN_S(t,2) != PN_NIL) {
          breg++;
          PN_BLOCK(breg, PN_S(t,2), PN_NIL);
        }
        DBG_c("; call %d %d VALUE\n", reg, breg);
        PN_ASM2(OP_CALL, reg, breg);
      }
    }
    break;

    case AST_ASSIGN: {
      vPN(Source) lhs = t->a[0];
      PN_SIZE num = PN_NONE, c = count;
      u8 opcode = OP_GETUPVAL, breg = reg;

      if (lhs->part == AST_EXPR) {
        unsigned long i = 0;
        c = PN_TUPLE_LEN(PN_S(lhs,0)) - 1;
        DBG_c("assign expr [%lu]\n", (_PN)c);
        for (i = 0; i < c; i++) {
          potion_source_asmb(P, f, loop, i, SRC_TUPLE_AT(lhs, i), reg);
        };
        lhs = SRC_TUPLE_AT(lhs, c);
	PN_ASM_DEBUG(reg, lhs);
      }

      if (lhs->part == AST_MSG || lhs->part == AST_QUERY) {
        char first_letter = PN_STR_PTR(PN_S(lhs,0))[0];
	DBG_c("assign %s '%s'\n", lhs->part == AST_MSG?"msg":"query",
	      AS_STR(PN_S(lhs,0)));
        if ((first_letter & 0x80) == 0 && isupper((unsigned char)first_letter)) {
          num = PN_PUT(f->values, PN_S(lhs,0));
	  DBG_c("values %d %s => %d\n", breg, AS_STR(lhs->a[0]), (int)num);
          PN_ASM2(OP_LOADK, breg, num);
          opcode = OP_GLOBAL;
          num = ++breg;
        } else if (c == 0) {
          num = PN_UPVAL(PN_S(lhs,0));
          if (num == PN_NONE) {
            num = PN_PUT(f->locals, PN_S(lhs,0));
	    DBG_c("locals %s => %d\n", AS_STR(lhs->a[0]), (int)num);
            opcode = OP_GETLOCAL;
#if 0 // store func names
	    if (lhs->part == AST_MSG) {
	      PN rhs = PN_TUPLE_AT(f->locals, num);
	      PN fname = PN_S(lhs,0);
	      DBG_c("getlocal %s %ld = %s\n",
	            PN_STR_PTR(fname), PN_INT(num), AS_STR(rhs));
            }
#endif
          }
        } else {
          num = PN_PUT(f->values, PN_S(lhs,0));
	  DBG_c("values %d %s => %d\n", breg+1, AS_STR(lhs->a[0]), (int)num);
          PN_ASM2(OP_LOADK, ++breg, num);
          opcode = OP_DEF;
          num = ++breg;
        }
      } else if (lhs->part == AST_PATH || lhs->part == AST_PATHQ) {
        DBG_c("assign %s\n", lhs->part == AST_PATH?"path":"pathq");
        num = PN_PUT(f->values, PN_S(lhs,0));
	DBG_c("values %d %s => %d\n", breg+1, AS_STR(lhs->a[0]), (int)num);
        if (c == 0) {
          PN_PUT(f->paths, PN_NUM(num));
	  DBG_c("paths %d\n", (int)num);
          PN_ASM1(OP_SELF, reg);
        }
        PN_ASM2(OP_LOADK, ++breg, num);
        opcode = OP_GETPATH;
        num = ++breg;
      }

      if (num == PN_NONE)
        potion_syntax_error(P, t, "Assignment to illegal variable");
      else if (lhs->a[1] != PN_NIL) {
        breg = reg;
        PN_ASM2(opcode, ++breg, num);
        DBG_c("; callset %d %d ASSIGN\n", reg, breg);
        PN_ASM2(OP_CALLSET, reg, breg);
        PN_ARG_TABLE(PN_S(lhs,1), breg, 1);
        // TODO: no block allowed here?
        potion_source_asmb(P, f, loop, 0, t->a[1], ++breg);
        DBG_c("; call %d %d ASSIGN\n", reg, breg);
        PN_ASM2(OP_CALL, reg, breg);
      } else {
        potion_source_asmb(P, f, loop, 0, t->a[1], breg);
        if (opcode == OP_GETUPVAL) {
          if (lhs->part == AST_QUERY) {
            PN_ASM2(OP_GETUPVAL, breg, num);
            PN_ASM2(OP_TESTJMP, breg, 1);
          }
          PN_ASM2(OP_SETUPVAL, reg, num);
        } else if (opcode == OP_GETLOCAL) {
          if (lhs->part == AST_QUERY) {
            PN_ASM2(OP_GETLOCAL, breg, num);
            PN_ASM2(OP_TESTJMP, breg, 1);
          } else {
            DBG_c("setlocal %d %d\n", reg, (int)num);
            PN_ASM2(OP_SETLOCAL, reg, num);
          }
        } else if (opcode == OP_GETPATH) {
          if (lhs->part == AST_PATHQ) {
            PN_ASM2(OP_GETPATH, reg, num);
            PN_ASM2(OP_TESTJMP, reg, 1);
          }
          PN_ASM2(OP_SETPATH, reg, num);
        } else {
          PN_ASM2(opcode, reg, num);
        }
      }
      PN_REG(f, breg);
    }
    break;

    case AST_INC: {
      u8 breg = reg;
      vPN(Source) lhs = t->a[0];
      PN_SIZE num = PN_UPVAL(PN_S(lhs,0));
      u8 opcode = OP_SETUPVAL;
      if (num == PN_NONE) {
        num = PN_PUT(f->locals, PN_S(lhs,0));
	DBG_c("locals %s => %d\n", AS_STR(lhs->a[0]), (int)num);
        opcode = OP_SETLOCAL;
      }

      if (opcode == OP_SETUPVAL)
        PN_ASM2(OP_GETUPVAL, reg, num);
       else if (opcode == OP_SETLOCAL)
        PN_ASM2(OP_GETLOCAL, reg, num);
      if (PN_IS_INT(PN_S(t,1))) {
        breg++;
        PN_ASM2(OP_MOVE, breg, reg);
      }
      PN_ASM2(OP_LOADPN, breg + 1, (PN_S(t,1) | PN_FINTEGER));
      PN_ASM2(OP_ADD, breg, breg + 1);
      PN_ASM2(opcode, breg, num);
      PN_REG(f, breg + 1);
    }
    break;

    case AST_CMP: case AST_EQ: case AST_NEQ:
    case AST_GT: case AST_GTE: case AST_LT: case AST_LTE:
    case AST_PLUS: case AST_MINUS: case AST_TIMES: case AST_DIV:
    case AST_REM:  case AST_POW:   case AST_BITL:  case AST_BITR: {
      PN_ARG(0, reg);
      PN_ARG(1, reg + 1);
      switch (t->part) {
        case AST_CMP:   PN_ASM2(OP_CMP, reg, reg + 1);  break;
        case AST_EQ:    PN_ASM2(OP_EQ,  reg, reg + 1);  break;
        case AST_NEQ:   PN_ASM2(OP_NEQ, reg, reg + 1);  break;
        case AST_GTE:   PN_ASM2(OP_GTE, reg, reg + 1);  break;
        case AST_GT:    PN_ASM2(OP_GT, reg, reg + 1);   break;
        case AST_LT:    PN_ASM2(OP_LT, reg, reg + 1);   break;
        case AST_LTE:   PN_ASM2(OP_LTE, reg, reg + 1);  break;
        case AST_PLUS:  PN_ASM2(OP_ADD, reg, reg + 1);  break;
        case AST_MINUS: PN_ASM2(OP_SUB, reg, reg + 1);  break;
        case AST_TIMES: PN_ASM2(OP_MULT, reg, reg + 1); break;
        case AST_DIV:   PN_ASM2(OP_DIV, reg, reg + 1);  break;
        case AST_REM:   PN_ASM2(OP_REM, reg, reg + 1);  break;
        case AST_POW:   PN_ASM2(OP_POW, reg, reg + 1);  break;
        case AST_BITL:  PN_ASM2(OP_BITL, reg, reg + 1); break;
        case AST_BITR:  PN_ASM2(OP_BITR, reg, reg + 1); break;
      }
    }
    break;

    case AST_NOT: case AST_WAVY:
      PN_ARG(0, reg);
      PN_ASM2(t->part == AST_WAVY ? OP_BITN : OP_NOT, reg, reg);
    break;

    case AST_AND: case AST_OR: {
      int jmp;
      PN_ARG(0, reg);
      jmp = PN_OP_LEN(f->asmb);
      PN_ASM2(t->part == AST_AND ? OP_NOTJMP : OP_TESTJMP, reg, 0);
      PN_ARG(1, reg);
      PN_OP_AT(f->asmb, jmp).b = (PN_OP_LEN(f->asmb) - jmp) - 1;
    }
    break;

    // TODO: this stuff is ugly and repetitive. replace by compiler macros for control structures
    case AST_MSG:
    case AST_QUERY: {
      u8 breg = reg;
      int arg = (PN_S(t,1) != PN_NIL);
      int call = (PN_S(t,2) != PN_NIL || arg);
      if (t->part == AST_MSG && PN_S(t,0) == PN_if) {
        int jmp; breg++;
        if (!t->a[1])
          return potion_syntax_error(P, t, "Missing if clause");
        //TODO: constant fold (cond as value) from p2
        PN_ARG_TABLE(PN_S(t,1), breg, 0);
        jmp = PN_OP_LEN(f->asmb);
        PN_ASM2(OP_NOTJMP, breg, 0);
        //TODO: allow this in p2 mode, rhs if expr
        if (!t->a[2])
          return potion_syntax_error(P, t, "Missing if body");
        potion_source_asmb(P, f, loop, 0, t->a[2], reg);
        PN_OP_AT(f->asmb, jmp).b = (PN_OP_LEN(f->asmb) - jmp) - 1;
      } else if (t->part == AST_MSG && PN_S(t,0) == PN_elsif) {
        int jmp1 = PN_OP_LEN(f->asmb), jmp2; breg++;
        PN_ASM2(OP_TESTJMP, breg, 0);
        if (!t->a[1])
          return potion_syntax_error(P, t, "Missing elsif clause");
        PN_ARG_TABLE(PN_S(t,1), breg, 0);
        jmp2 = PN_OP_LEN(f->asmb);
        PN_ASM2(OP_NOTJMP, breg, 0);
        if (!t->a[2])
          return potion_syntax_error(P, t, "Missing elsif body");
        potion_source_asmb(P, f, loop, 0, t->a[2], reg);
        PN_OP_AT(f->asmb, jmp1).b = (PN_OP_LEN(f->asmb) - jmp1) - 1;
        PN_OP_AT(f->asmb, jmp2).b = (PN_OP_LEN(f->asmb) - jmp2) - 1;
      } else if (t->part == AST_MSG && PN_S(t,0) == PN_else) {
        int jmp = PN_OP_LEN(f->asmb); breg++;
        PN_ASM2(OP_TESTJMP, breg, 0);
        if (!t->a[2])
          potion_syntax_error(P, t, "Missing else body");
        potion_source_asmb(P, f, loop, 0, t->a[2], reg);
        PN_OP_AT(f->asmb, jmp).b = (PN_OP_LEN(f->asmb) - jmp) - 1;
      } else if (t->part == AST_MSG && PN_S(t,0) == PN_class) {
        u8 breg = reg;
        if (count == 0)
          PN_ASM1(OP_SELF, reg);
        if (PN_S(t,2) != PN_NIL) {
	  vPN(Source) blk = PN_S_(t,2);
          // TODO: a hack to make sure constructors always return self
          if (PN_S(blk, 0) == PN_NIL)
            blk->a[0] = PN_SRC(PN_AST(CODE, PN_NIL, blk->loc.lineno, blk->line));
          PN ctor = PN_S(blk, 0);
          PN_PUSH(ctor, PN_AST(EXPR, PN_TUP(PN_AST(MSG,
	    PN_STRN("self", 4), blk->loc.lineno, 0)),
			        blk->loc.lineno, blk->line));
          breg++;
          PN_BLOCK(breg, (PN)blk, PN_S(t,1));
        }
        PN_ASM2(OP_CLASS, reg, breg);
#ifdef WITH_EXTERN
      } else if (t->part == AST_MSG && PN_S(t,0) == PN_extern) { // ffi
        //u8 breg = reg;
	int i, arity;
        PN msg = PN_S(t,1);
	char *name = PN_STR_PTR(PN_S(PN_TUPLE_AT(msg,0),0));
	//defer dlsym to run-time to see load, or require load at BEGIN time?
	//PN_ASM2(OP_LOADPN, breg++, name);
	PN_F sym = (PN_F)dlsym(RTLD_DEFAULT, name);
	DBG_c("extern %s => dlsym %p\n", name, sym);
	if (!sym) {
	  fprintf(stderr, "* extern %s not found. You may need to load a library first\n",
	          name);
          PN_ASM2(OP_LOADPN, reg, PN_NIL);
        } else {
          // TODO: create a ffi wrapper to translate the args and return value
          struct PNProto* cl = PN_CALLOC_N(PN_TPROTO, struct PNProto, 0);
          cl->source = PN_NIL;
          cl->stack = PN_NUM(1);
          cl->protos = cl->paths = cl->locals = cl->upvals = cl->values = PN_TUP0();
          cl->tree = (PN)t;
          cl->asmb = (PN)potion_asm_new(P);
          cl->name = PN_S(PN_TUPLE_AT(msg,0),0);
          cl->jit = (PN_F)sym;
	  PN sig = PN_TUPLE_LEN(msg) ? PN_TUPLE_AT(msg, 1) : PN_TUP0();
	  cl->sig = sig;
	  arity = cl->arity = potion_sig_arity(P, sig);
          for (i=0; i < arity; i++) {
	    if (PN_TUPLE_LEN(msg) > 1) {
	      // TODO set argtype translators
              PN arg = PN_TUPLE_AT(sig, i);
              if (arg == PN_STR("N") || arg == PN_STR("int") || arg == PN_STR("long")) {
                DBG_c("extern %s %d-th arg => PN_INT(N)\n", name, i);
                //insert PN_INT >>1 at runtime
                //PN_ASM2(OP_LOADPN, breg++, arg);
              } else if (arg == PN_STR("S") || arg == PN_STR("char*")) {
                DBG_c("extern %s %d-th arg => PN_STR_PTR(S)\n", name, i);
                // insert call to potion_str_ptr at runtime
                //PN_ASM2(OP_LOADPN, breg++, arg);
              } else {
                fprintf(stderr, "* unknown extern %s argument type qualifier\n",
                        PN_STR_PTR(arg));
              }
	    }
	    // maybe add code to check and fill in defaults?
	  }
          // TODO insert code to transform return values
	  cl->asmb = (PN)potion_asm_op(P, (PNAsm *)cl->asmb, OP_RETURN, reg, 0);
          PN_ASM2(OP_PROTO, reg, PN_PUT(f->protos, (PN)cl));
          //PN_ASM2(OP_EXTERN, reg, breg);
        }
#endif
      } else if (t->part == AST_MSG && (PN_S(t,0) == PN_while || PN_S(t,0) == PN_loop)) {
        int jmp1 = 0, jmp2 = PN_OP_LEN(f->asmb); breg++;
        struct PNLoop l; l.bjmpc = 0; l.cjmpc = 0;
        int i;
        if (PN_S(t,0) == PN_while) {
          PN_ARG_TABLE(PN_S(t,1), breg, 0);
          jmp1 = PN_OP_LEN(f->asmb);
          PN_ASM2(OP_NOTJMP, breg, 0);
        } else if (PN_S(t,1) || !t->a[2]) {
	  //warn or skip to allow a method named loop? for aio we want loop methods
	  //fprintf(stderr, "* loop takes no args, just a block");
	  goto loopfunc;
	}
        if (!t->a[2]) {
          if (PN_S(t,0) == PN_while)
            return potion_syntax_error(P, t, "Missing while body");
          else
            return potion_syntax_error(P, t, "Missing loop body");
        }
        potion_source_asmb(P, f, &l, 0, t->a[2], reg);
        PN_ASM1(OP_JMP, (jmp2 - PN_OP_LEN(f->asmb)) - 1);
        if (PN_S(t,0) == PN_while) {
          PN_OP_AT(f->asmb, jmp1).b = (PN_OP_LEN(f->asmb) - jmp1) - 1;
        }
        for (i = 0; i < l.bjmpc; i++) {
          PN_OP_AT(f->asmb, l.bjmps[i]).a = (PN_OP_LEN(f->asmb) - l.bjmps[i]) - 1;
        }
        for (i = 0; i < l.cjmpc; i++) {
          PN_OP_AT(f->asmb, l.cjmps[i]).a = (jmp2 - l.cjmps[i]) - 1;
        }
      } else if (t->part == AST_MSG && PN_S(t,0) == PN_return) {
        //if (!t->a[1])
        //    return potion_syntax_error(P, t, "Missing return value");
        PN_ARG_TABLE(PN_S(t,1), reg, 0);
        PN_ASM1(OP_RETURN, reg);
      } else if (t->part == AST_MSG && PN_S(t,0) == PN_break) {
        if (loop != NULL) {
          loop->bjmps[loop->bjmpc++] = PN_OP_LEN(f->asmb);
          PN_ASM1(OP_JMP, 0);
        } else {
          potion_syntax_error(P, t, "'break' outside of loop");
        }
      } else if (t->part == AST_MSG && PN_S(t,0) == PN_continue) {
        if (loop != NULL) {
          loop->cjmps[loop->cjmpc++] = PN_OP_LEN(f->asmb);
          PN_ASM1(OP_JMP, 0);
        } else {
          potion_syntax_error(P, t, "'continue' outside of loop");
        }
      } else if (t->part == AST_MSG && PN_S(t,0) == PN_self) {
        PN_ASM1(OP_SELF, reg);
      } else {
        // TODO lookup if macro
      loopfunc: ;
	u8 opcode = OP_GETUPVAL;
        PN_SIZE num = PN_NONE;
        PN v = PN_S(t,0);
        if (count == 0 && t->part == AST_MSG) {
          num = PN_UPVAL(v);
          if (num == PN_NONE) {
            num = PN_GET(f->locals, v);
	    DBG_c("locals %s <= %d\n", AS_STR(v), (int)num);
            opcode = OP_GETLOCAL;
          }
        }

        if (num == PN_NONE && PN_S(t,0) != PN_NIL) {
          u8 oreg = ++breg;
          int jmp = 0;
          num = PN_PUT(f->values, PN_S(t,0));
	  DBG_c("values %d %s => %d\n", reg, AS_STR(t->a[0]), (int)num);
          if (count == 0) {
            PN_ASM1(OP_SELF, oreg);
          } else {
            PN_ASM2(OP_MOVE, oreg, reg);
          }
          PN_ASM2(OP_LOADK, reg, num);
          if (PN_S(t,2) != PN_NIL && (PN_PART(PN_S(t,2)) == AST_MSG)) {
            vPN(Source) t2 = PN_S_(t,2); //typed message (MSG LIST|NIL TYPE)
            DBG_c("typed %s %s\n", AS_STR(t->a[0]), AS_STR(t2));
            //TODO type should already exist at compile-time. check native or user type
            num = PN_PUT(f->values, PN_S(t2,0));
            PN_ASM2(OP_LOADK, reg, num);
            PN_ASM2(OP_BIND, reg, breg);
          } else {
            PN_ASM2(((PN_S(t,1) != PN_NIL || PN_S(t,2) != PN_NIL) ? OP_MSG : OP_BIND), reg, breg);
          }
          if (t->part == AST_QUERY && PN_S(t,1) != PN_NIL) {
            jmp = PN_OP_LEN(f->asmb);
            PN_ASM2(OP_NOTJMP, reg, 0);
          }
#define LOAD_ARG() PN_ARG_TABLE(PN_S(t,1), breg, 1)
          if (arg) {
            u8 part1 = PN_PART(PN_S(t,1));
            if (part1 == AST_VALUE || (part1 == AST_LIST && PN_S(t,2) == PN_NIL)) {
              LOAD_ARG();
            }
          }
          if (PN_S(t,2) != PN_NIL && (PN_PART(PN_S(t,2)) == AST_PROTO)) {
            vPN(Source) t2 = PN_S_(t,2);
            breg++;
            PN_BLOCK(breg, PN_S(t2,1), PN_S(t2,0));
          } else {
            if (PN_S(t,1) == PN_NIL && PN_S(t,2) == PN_NIL)
              LOAD_ARG();
            if (PN_S(t,2) != PN_NIL && PN_PART(PN_S(t,2)) == AST_BLOCK) {
              breg++;
              PN_BLOCK(breg, PN_S(t,2), PN_S(t,1));
            }
          }
#undef LOAD_ARG
          if (t->part == AST_MSG) {
	    DBG_c("; call %d %d MSG\n", reg, breg);
            PN_ASM2(OP_CALL, reg, breg);
          } else {
            if (PN_S(t,1) != PN_NIL) {
	      DBG_c("; call %d %d !MSG\n", reg, breg);
              PN_ASM2(OP_CALL, reg, breg);
              PN_OP_AT(f->asmb, jmp).b = (PN_OP_LEN(f->asmb) - jmp) - 1;
            } else {
              PN_ASM2(OP_TEST, reg, breg);
            }
          }
        } else {
          if (num != PN_NONE)
            PN_ASM2(opcode, reg, num);
          if (call) {
            PN_ASM1(OP_SELF, ++breg);
            PN_ARG_TABLE(PN_S(t,1), breg, 1);
            if (PN_S(t,2) != PN_NIL) {
              breg++;
              PN_BLOCK(breg, PN_S(t,2), PN_NIL);
            }
            DBG_c("; call %d %d\n", reg, breg);
            PN_ASM2(OP_CALL, reg, breg);
          }
        }
      }
      PN_REG(f, breg);
    }
    break;

    case AST_PATH:
    case AST_PATHQ: {
      PN_SIZE num = PN_PUT(f->values, PN_S(t,0));
      DBG_c("values %d %s => %d\n", reg, AS_STR(t->a[0]), (int)num);
      if (count == 0) {
        PN_PUT(f->paths, PN_NUM(num));
	DBG_c("paths %d\n", (int)num);
        PN_ASM1(OP_SELF, reg);
      }
      PN_ASM2(OP_LOADK, reg + 1, num);
      PN_ASM2(OP_GETPATH, reg, reg + 1);
      if (t->part == AST_PATHQ)
        PN_ASM2(OP_TEST, reg, reg);
      PN_REG(f, reg + 1);
    }
    break;

    case AST_LICK: {
      u8 breg = reg;
      PN_SIZE num = PN_PUT(f->values, PN_S(t,0));
      DBG_c("values %d %s => %d\n", reg, AS_STR(t->a[0]), (int)num);
      PN_ASM2(OP_LOADK, reg, num);
      if (PN_S(t,1) != PN_NIL)
        potion_source_asmb(P, f, loop, 0, t->a[1], ++breg);
      else if (PN_S(t,2) != PN_NIL)
        PN_ASM2(OP_LOADPN, ++breg, PN_NIL);
      if (PN_S(t,2) != PN_NIL)
        potion_source_asmb(P, f, loop, 0, t->a[2], ++breg);
      PN_ASM2(OP_NEWLICK, reg, breg);
      PN_REG(f, breg);
    }
    break;

    case AST_LIST:
      PN_ASM1(OP_NEWTUPLE, reg);
      if (PN_S(t,0) != PN_NIL) {
        PN_TUPLE_EACH(PN_S(t,0), i, v, {
	  PN_ASM_DEBUG(reg, PN_SRC(v));
          if (PN_PART(v) == AST_ASSIGN) { //potion only: (k=v, ...)
	    vPN(Source) lhs = PN_SRC(PN_S(v,0));
            if (lhs->part == AST_EXPR && PN_TUPLE_LEN(PN_S(lhs,0)) == 1)
            {
              lhs = SRC_TUPLE_AT(lhs, 0);
              if (lhs->part == AST_MSG) {
		PN_ASM_DEBUG(reg, lhs);
                PN_SIZE num = PN_PUT(f->values, PN_S(lhs,0));
		DBG_c("values %d %s => %d\n", reg+1, AS_STR(lhs->a[0]), (int)num);
                PN_ASM2(OP_LOADK, reg + 1, num);
                lhs = NULL;
              }
            }

            if (lhs != NULL)
              potion_source_asmb(P, f, loop, 0, lhs, reg + 1);

            potion_source_asmb(P, f, loop, 0, PN_SRC(v)->a[1], reg + 2);
            PN_ASM2(OP_SETTABLE, reg, reg + 2);
            PN_REG(f, reg + 2);
          } else {
            potion_source_asmb(P, f, loop, 0, PN_SRC(v), reg + 1);
            PN_ASM2(OP_SETTUPLE, reg, reg + 1);
            PN_REG(f, reg + 1);
          }
        });
      }
    break;
  }
}

#define SIG_EXPR_MSG(name,expr)			\
  if (expr->part == AST_EXPR) {			\
    vPN(Source) t = SRC_TUPLE_AT(expr, 0);	\
    if (t->part == AST_MSG) {			\
      name = PN_S(t,0);				\
      PN_PUT(f->locals, name);			\
      DBG_c("locals %s\n", PN_STR_PTR(name));	\
      sig = PN_PUSH(sig, name);			\
    }						\
  } else { name = PN_NIL; }

/** Converts a pre-compiled potion expr to a signature tuple.
   Extract locals, apply type defaults and do type and param checks.
   Sigs are also ambigious, the parser usually compiles it down to
   expression trees, not to a sig tuple. Converts assign,pipe,value.

   Not used by P2, as args2 generates the correct sig tuple in the parser already.

   \todo this is overly complicated. create tuples as in P2 in the parser,
    do not reuse expr and assign.

   Name=Type, '|' optional '.' end ':='default
   type: 'o' (= PN object)
   Encode to 3-tuple AST_CODE: name type|modifier default
\param f    the PNProto closure to store locals
\param src  PNSource signature tree, parsed via yy_sig()
\return PNTuple of sigs

 (x=n,y:=1) =>
\code  (list (assign (expr (msg (x)) expr (msg (n ))),
        assign (expr (msg (y)) value (1 ))) \endcode */
PN potion_sig_compile(Potion *P, vPN(Proto) f, PN src) {
  PN sig = PN_TUP0();
  vPN(Source) t = PN_SRC(src);
  if (t->part == AST_LIST && PN_S(t,0) != PN_NIL) {
    //PN_TUPLE_EACH(PN_S(t,0), i, v, {
    ({ struct PNTuple * volatile __tv = ((struct PNTuple *)potion_fwd(PN_S(t,0)));
      if (__tv->len != 0) {
        DBG_c("--- sig compile ---\n");
	PN_SIZE i;
	for (i = 0; i < __tv->len; i++) {
	  struct PNSource *v = PN_SRC(__tv->set[i]);
	  {
      vPN(Source) expr = PN_SRC(v);
      PN name = 0;
      SIG_EXPR_MSG(name, expr)
      if (expr->part == AST_VALUE) {
        vPN(Source) lhs = expr->a[0];
	PN v = PN_S(lhs,0);
	if (PN_IS_STR(v)) {
	  DBG_c("sig: lhs value, computed name %s\n", AS_STR(v));
          PN_PUT(f->locals, v);
	  DBG_c("locals %s\n", PN_STR_PTR(v));
	  sig = PN_PUSH(sig, v);
	} else {
	  potion_syntax_error(P, t, "in signature: value %s as argument name", AS_STR(v));
	}
      } else if (expr->part == AST_PIPE) { //x|y => (pipe (expm x) (expm y))
	vPN(Source) lhs = expr->a[0];
	vPN(Source) rhs = expr->a[1];
	PN name2 = 0;
	SIG_EXPR_MSG(name, lhs);
	sig = PN_PUSH(sig, PN_NUM('|'));
	SIG_EXPR_MSG(name2, rhs);
	DBG_c("; (%s | %s)\n", AS_STR(name), AS_STR(name2));
      } else if (expr->part == AST_ASSIGN) { //x=o => (assign (expm x) (expm o))
        vPN(Source) lhs = expr->a[0];
	vPN(Source) rhs = expr->a[1];
        if (lhs->part == AST_EXPR && PN_TUPLE_LEN(PN_S(lhs,0)) == 1) {
	  SIG_EXPR_MSG(name, lhs);
          lhs = SRC_TUPLE_AT(lhs, 0);
	  DBG_c("; (%s ", AS_STR(name));
	}
        else if (lhs->part == AST_PIPE) {
	  //x|y=o => (assign (pipe (expm x) (expm y)) (expm o))
	  SIG_EXPR_MSG(name, lhs->a[0]);
	  sig = PN_PUSH(sig, PN_NUM('|'));
	  rhs = lhs->a[1];
	  DBG_c("; (%s | ", AS_STR(name));
	} else {
	  potion_syntax_error(P, t, "in signature: unexpected AST %s", AS_STR(lhs));
	}
	if (rhs->part == AST_EXPR && PN_TUPLE_LEN(rhs->a[0]) == 1) {
	  rhs = SRC_TUPLE_AT(rhs, 0);
	  if (rhs->part == AST_MSG) {
	    PN v = PN_NUM(PN_STR_PTR(PN_S(rhs,0))[0]); // = type. TODO: one-char => VTABLE
	    DBG_c("%s)\n", AS_STR(v));
	    sig = PN_PUSH(sig, v);
	  }
        } else if (rhs->part == AST_ASSIGN && rhs->a[0]->part == AST_PIPE) {
          //x=N|y=o => (assign (pipe (expm x) (expm y)) (expm o))
          lhs = PN_S_(PN_S_(rhs, 0), 0);
	  if (lhs->part == AST_EXPR && SRC_TUPLE_AT(lhs,0)->part == AST_MSG) {
	    PN v = PN_NUM(PN_STR_PTR((PN)SRC_TUPLE_AT(lhs,0)->a[0])[0]); // = type. TODO: one-char => VTABLE
	    DBG_c("%s)\n", AS_STR(v));
	    sig = PN_PUSH(sig, v);
	  }
	  sig = PN_PUSH(sig, PN_NUM('|'));
	  SIG_EXPR_MSG(name, rhs->a[0]->a[1]);
          rhs = rhs->a[1];
	  if (rhs->part == AST_EXPR && SRC_TUPLE_AT(rhs,0)->part == AST_MSG) {
            PN v = PN_S(SRC_TUPLE_AT(rhs,0),0);
	    v = PN_NUM(PN_STR_PTR(v)[0]); // = type. TODO: one-char => VTABLE
	    DBG_c("%s)\n", AS_STR(v));
	    sig = PN_PUSH(sig, v);
          }
	} else if (rhs->part == AST_VALUE) {    // :=default
	  PN v = PN_S(rhs,0);
	  DBG_c(": %s)\n", AS_STR(v));
	  sig = PN_PUSH(PN_PUSH(sig, PN_NUM(':')), v);
	} else {
	  potion_syntax_error(P, t, "in signature: unexpected AST %s", AS_STR(expr));
	}
      }
    }}}
    });
  }
  return sig;
}
#undef SRC_TUPLE_AT
#undef SIG_EXPR_MSG

///\memberof PNSource
/// "compile" method for PNSource, an AST fragment. Typically to add a function definition,
/// but also objects, blocks, ... (Almost everything is a PNClosure)
///\param source  PNProto or PN_NIL of the current closure definition stored in f->source of each block
///\param sig     PNSource signature tree or PN_NIL or PNTuple in P2, parsed via yy_sig()
///\return PNProto a closure
PN potion_source_compile(Potion *P, PN cl, PN self, PN source, PN sig) {
  vPN(Proto) f;
  vPN(Source) t = (struct PNSource *)self;

  switch (t->part) {
    case AST_CODE:
    case AST_BLOCK: break;
    default: return PN_NIL; // TODO: error
  }
  f = PN_ALLOC(PN_TPROTO, struct PNProto);
  f->source = source;
  f->stack = PN_NUM(1);
  f->protos = PN_TUP0();
  f->paths = PN_TUP0();
  f->locals = PN_TUP0();
  f->upvals = PN_TUP0();
  f->values = PN_TUP0();
  f->debugs = PN_TUP0();
  f->tree = self;
  DBG_c("-- compile --\n");
  f->sig = (sig == PN_NIL ? PN_TUP0() : potion_sig_compile(P, f, sig));
  f->asmb = (PN)potion_asm_new(P);
  f->name = PN_NIL;

  potion_source_asmb(P, f, NULL, 0, t, 0);
  PN_ASM1(OP_RETURN, 0);

  f->localsize = PN_TUPLE_LEN(f->locals);
  f->upvalsize = PN_TUPLE_LEN(f->upvals);
  f->pathsize = PN_TUPLE_LEN(f->paths);
  return (PN)f;
}

///\memberof PNProto
/// clone method of PNProto
/// shares name, tree. copies the rest
PN potion_proto_clone(Potion *P, PN cl, PN self) {
  vPN(Proto) f = (struct PNProto *)self;
  vPN(Proto) n = PN_ALLOC(PN_TPROTO, struct PNProto);
  PN PN_clone = PN_STR("clone");
  PNAsm * volatile asmb;
  PN len;

  //TODO extern protos
  n->name = f->name;
  n->source = f->source;
  n->stack = f->stack;
  n->protos = potion_send(PN_clone, f->protos);
  n->paths  = potion_send(PN_clone, f->paths);
  n->locals = potion_send(PN_clone, f->locals);
  n->upvals = potion_send(PN_clone, f->upvals);
  n->values = potion_send(PN_clone, f->values);
  n->debugs = potion_send(PN_clone, f->debugs);
  n->tree   = f->tree;
  n->localsize = f->localsize;
  n->upvalsize = f->upvalsize;
  n->pathsize = f->pathsize;
  n->sig = PN_IS_TUPLE(f->sig) ? potion_send(PN_clone, f->sig) : f->sig;

  len = ((PNAsm *)f->asmb)->len;
  PN_FLEX_NEW(asmb, PN_TBYTES, PNAsm, len);
  PN_MEMCPY_N(asmb->ptr, ((PNAsm *)f->asmb)->ptr, u8, len);
  asmb->len = len;
  n->asmb = (PN)asmb;

  return (PN)n;
}

#define READ_U8(ptr) ({u8 rpu = *ptr; ptr += sizeof(u8); rpu;})
#define READ_PN(pn, ptr) ({PN rpn = *(PN *)ptr; ptr += pn; rpn;})
#define READ_CONST(pn, ptr) ({ \
    PN val = READ_PN(pn, ptr); \
    if (PN_IS_PTR(val)) { \
      if (val & 2) { \
        size_t len = ((val ^ 2) >> 4) - 1; \
        val = potion_strtod(P, (char *)ptr, len); \
        ptr += len; \
      } else { \
        size_t len = (val >> 4) - 1; \
        val = PN_STRN((char *)ptr, len); \
        ptr += len; \
      } \
    } \
    val; \
  })

#define READ_TUPLE(ptr) \
  long i = 0, count = READ_U8(ptr); \
  PN tup = potion_tuple_with_size(P, (PN_SIZE)count); \
  for (; i < count; i++)
#define READ_VALUES(pn, ptr) ({ \
    READ_TUPLE(ptr) PN_TUPLE_AT(tup, i) = READ_CONST(pn, ptr); \
    tup; \
  })
#define READ_PROTOS(pn, ptr) ({ \
    READ_TUPLE(ptr) PN_TUPLE_AT(tup, i) = potion_proto_load(P, (PN)f, pn, &(ptr)); \
    tup; \
  })

// TODO: this byte string is volatile, need to avoid using ptr
PN potion_proto_load(Potion *P, PN up, u8 pn, u8 **ptr) {
  PN len = 0;
  PNAsm * volatile asmb = NULL;
  vPN(Proto) f = PN_ALLOC(PN_TPROTO, struct PNProto);
  // TODO extern protos
  f->name = READ_CONST(pn, *ptr);
  f->source = READ_CONST(pn, *ptr);
  if (f->source == PN_NIL) f->source = up;
  f->sig = READ_VALUES(pn, *ptr);
  f->stack = READ_CONST(pn, *ptr);
  f->values = READ_VALUES(pn, *ptr);
  f->paths = READ_VALUES(pn, *ptr);
  f->locals = READ_VALUES(pn, *ptr);
  f->upvals = READ_VALUES(pn, *ptr);
  f->protos = READ_PROTOS(pn, *ptr);
  f->debugs = PN_TUP0();

  len = READ_PN(pn, *ptr);
  PN_FLEX_NEW(asmb, PN_TBYTES, PNAsm, len);
  PN_MEMCPY_N(asmb->ptr, *ptr, u8, len);
  asmb->len = len;

  f->asmb = (PN)asmb;
  f->localsize = PN_TUPLE_LEN(f->locals);
  f->upvalsize = PN_TUPLE_LEN(f->upvals);
  f->pathsize  = PN_TUPLE_LEN(f->paths);
  *ptr += len;
  return (PN)f;
}

///\memberof PNSource
// TODO: "load" from a stream
PN potion_source_load(Potion *P, PN cl, PN buf) {
  u8 *ptr;
  vPN(BHeader) h = (struct PNBHeader *)PN_STR_PTR(buf);
  // check for compiled binary first
  if ((size_t)PN_STR_LEN(buf) <= sizeof(struct PNBHeader) ||
      strncmp((char *)h->sig, POTION_SIG, 4) != 0)
    return PN_NIL;

  ptr = h->proto;
  return potion_proto_load(P, PN_NIL, h->pn, &ptr);
}

// TODO: switch to dump methods
#define WRITE_U8(un, ptr) ({*ptr = (u8)un; ptr += sizeof(u8);})
#define WRITE_PN(pn, ptr) ({*(PN *)ptr = pn; ptr += sizeof(PN);})
#define WRITE_CONST(val, ptr) ({ \
    if (PN_IS_STR(val)) { \
      PN count = (PN_STR_LEN(val)+1) << 4; \
      WRITE_PN(count, ptr); \
      PN_MEMCPY_N(ptr, PN_STR_PTR(val), char, PN_STR_LEN(val)); \
      ptr += PN_STR_LEN(val); \
    } else if (PN_IS_DBL(val)) { \
      PN str = potion_num_string(P, PN_NIL, val); \
      PN count = ((PN_STR_LEN(str)+1) << 4) | 2; \
      WRITE_PN(count, ptr); \
      PN_MEMCPY_N(ptr, PN_STR_PTR(str), char, PN_STR_LEN(str)); \
      ptr += PN_STR_LEN(str); \
    } else { \
      PN cval = (PN_IS_PTR(val) ? PN_NIL : val); \
      WRITE_PN(cval, ptr); \
    } \
  })
#define WRITE_TUPLE(tup, ptr) \
  long i = 0, count = PN_TUPLE_LEN(tup); \
  WRITE_U8(count, ptr); \
  for (; i < count; i++)
#define WRITE_VALUES(tup, ptr) ({ \
    WRITE_TUPLE(tup, ptr) WRITE_CONST(PN_TUPLE_AT(tup, i), ptr); \
  })
#define WRITE_PROTOS(tup, ptr) ({ \
    WRITE_TUPLE(tup, ptr) ptr += potion_proto_dumpbc(P, PN_TUPLE_AT(tup, i), \
        out, (char *)ptr - PN_STR_PTR(out)); \
  })

///\memberof PNProto
/// compile to bytecode
///\param proto  PNProto
///\param out    PNBytes output buffer
///\param pos    long where to add at out
///\return char* ptr - start
long potion_proto_dumpbc(Potion *P, PN proto, PN out, long pos) {
  vPN(Proto) f = (struct PNProto *)proto;
  char *start = PN_STR_PTR(out) + pos;
  u8 *ptr = (u8 *)start;
  //TODO extern (by name)
  WRITE_CONST(f->name, ptr);
  WRITE_CONST(f->source, ptr);
  WRITE_VALUES(f->sig, ptr);
  WRITE_CONST(f->stack, ptr);
  WRITE_VALUES(f->values, ptr);
  WRITE_VALUES(f->paths, ptr);
  WRITE_VALUES(f->locals, ptr);
  WRITE_VALUES(f->upvals, ptr);
  WRITE_PROTOS(f->protos, ptr);
  WRITE_PN(PN_FLEX_SIZE(f->asmb), ptr);
  PN_MEMCPY_N(ptr, ((PNFlex *)f->asmb)->ptr, u8, PN_FLEX_SIZE(f->asmb));
  ptr += PN_FLEX_SIZE(f->asmb);
  return (char *)ptr - start;
}

///\memberof PNSource
/// dump to bytecode
// Low TODO: dump to a stream (if we have not enough memory)
PN potion_source_dumpbc(Potion *P, PN cl, PN proto, PN options) {
  PN pnb = potion_bytes(P, 8192);
  struct PNBHeader h;
  DBG_c("compile bc\n");
  PN_MEMCPY_N(h.sig, POTION_SIG, u8, 4);
  h.major = POTION_MAJOR;
  h.minor = POTION_MINOR;
  h.vmid = POTION_VMID;
  h.pn = (u8)sizeof(PN);

  PN_MEMCPY(PN_STR_PTR(pnb), &h, struct PNBHeader);
  PN_STR_LEN(pnb) = (long)sizeof(struct PNBHeader) +
    potion_proto_dumpbc(P, proto, pnb, sizeof(struct PNBHeader));
  return pnb;
}

///\memberof PNSource
/// dump (compiler) methods, default "bc". loads compile-"backend" extension.
///\param backend PNString - load a compile-"backend" module and call its dump"backend" method
///\param options optional PNString
/// TODO: serializable ascii, c, exe, jvm, .net
PN potion_source_dump(Potion *P, PN cl, PN self, PN backend, PN options) {
  if (backend == PN_STRN("bc", 2))
    return potion_source_dumpbc(P, cl, self, options);
#ifndef SANDBOX
  char *cb = PN_STR_PTR(backend);
  if (potion_load(P, P->lobby, self, potion_strcat(P, "compile/", cb))) {
    DBG_c("loaded compile/%s\n", cb);
    DBG_c("Source dump%s(%s)\n", cb, PN_IS_STR(options) ? PN_STR_PTR(options) : "");
    return potion_send(self, potion_strcat(P, "dump", cb), options);
  } else {
    fprintf(stderr, "** failed loading the compile/%s module\n", cb);
    return PN_NIL;
  }
#else
  potion_fatal("external compilers disabled with SANDBOX");
#endif
}

PN potion_run(Potion *P, PN code, int jit) {
#ifndef POTION_JIT_TARGET
  if (jit) {
    fprintf(stderr, "** potion not compiled with JIT\n");
    jit = 0;
  }
#endif
 if (jit) {
    PN cl = potion_closure_new(P, (PN_F)potion_jit_proto(P, code), PN_NIL, 1);
    PN_CLOSURE(cl)->data[0] = code;
    return PN_PROTO(code)->jit(P, cl, P->lobby);
  } else {
    return potion_vm(P, code, P->lobby, PN_NIL, 0, NULL);
  }
}

PN potion_eval(Potion *P, PN bytes) {
  PN code = (PN_TYPE(bytes) == PN_TSOURCE)
    ? bytes
    : potion_parse(P, bytes, "<eval>");
  if (PN_TYPE(code) != PN_TSOURCE) return code;
  code = potion_send(code, PN_compile, PN_NIL, PN_NIL);
  return potion_run(P, code, P->flags & EXEC_JIT);
}

void potion_compiler_init(Potion *P) {
  PN pro_vt = PN_VTABLE(PN_TPROTO);
  PN src_vt = PN_VTABLE(PN_TSOURCE);
  potion_method(pro_vt, "call", potion_proto_call, "args=u");
  potion_method(pro_vt, "tree", potion_proto_tree, 0);
  potion_method(pro_vt, "string", potion_proto_string, 0);
  potion_method(pro_vt, "clone", potion_proto_clone, 0);
  potion_method(src_vt, "compile", potion_source_compile, "source=a,sig=u");
  potion_method(src_vt, "dump", potion_source_dump, "backend=S|options=S");
  potion_method(src_vt, "dumpbc", potion_source_dumpbc, "|options=S");
}
