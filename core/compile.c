//
// compile.c
// ast to bytecode
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include "p2.h"
#include "internal.h"
#include "ast.h"
#include "opcodes.h"
#include "asm.h"

#define PN_ASM1(ins, _a)     f->asmb = (PN)potion_asm_op(P, (PNAsm *)f->asmb, (u8)ins, (int)_a, 0)
#define PN_ASM2(ins, _a, _b) f->asmb = (PN)potion_asm_op(P, (PNAsm *)f->asmb, (u8)ins, (int)_a, (int)_b)

const struct {
  const char *name;
  const u8 args;
} potion_ops[] = {
  {"noop", 0}, {"move", 2}, {"loadk", 2}, {"loadpn", 2}, {"self", 1},
  {"newtuple", 2}, {"settuple", 2}, {"getlocal", 2}, {"setlocal", 2},
  {"getupval", 2}, {"setupval", 2}, {"global", 2}, {"gettable", 2},
  {"settable", 2}, {"newlick", 2}, {"getpath", 2}, {"setpath", 2},
  {"add", 2}, {"sub", 2}, {"mult", 2}, {"div", 2}, {"mod", 2}, {"pow", 2},
  {"not", 1}, {"cmp", 2}, {"eq", 2}, {"neq", 2}, {"lt", 2}, {"lte", 2},
  {"gt", 2}, {"gte", 2}, {"bitn", 2}, {"bitl", 2}, {"bitr", 2}, {"def", 2},
  {"bind", 2}, {"message", 2}, {"jump", 1}, {"test", 2}, {"testjmp", 2},
  {"notjmp", 2}, {"named", 2}, {"call", 2}, {"callset", 2}, {"tailcall", 2},
  {"return", 1}, {"proto", 2}, {"class", 2}
};

PN potion_proto_tree(Potion *P, PN cl, PN self) {
  return PN_PROTO(self)->tree;
}

PN potion_proto_call(Potion *P, PN cl, PN self, PN args) {
  return potion_vm(P, self, P->lobby, args, 0, NULL);
}

PN potion_proto_string(Potion *P, PN cl, PN self) {
  vPN(Proto) t = (struct PNProto *)self;
  int x = 0;
  PN_SIZE num = 1;
  PN_SIZE numcols;
  PN out = potion_byte_str(P, "; function definition");
  pn_printf(P, out, ": %p ; %u bytes\n", t, PN_FLEX_SIZE(t->asmb));
  pn_printf(P, out, "; (");
  PN_TUPLE_EACH(t->sig, i, v, {
    if (PN_IS_NUM(v)) {
      if (v == '.')
        pn_printf(P, out, ". ");
      else if (v == '|')
        pn_printf(P, out, "| ");
      else
        pn_printf(P, out, "=%c, ", (int)PN_INT(v));
    } else
      potion_bytes_obj_string(P, out, v);
  });
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
  pn_printf(P, out, "; function end\n");
  return PN_STR_B(out);
}

#define PN_REG(f, reg) \
  if (reg >= PN_INT(f->stack)) \
    f->stack = PN_NUM(reg + 1)
#define PN_ARG(n, reg) \
  if (PN_PART(t->a[n]) == AST_EXPR && PN_PART(PN_TUPLE_AT(PN_S(t->a[n], 0), 0)) == AST_TABLE) { \
    PN test = PN_S(PN_TUPLE_AT(PN_S(t->a[n], 0), 0), 0); \
    if (!PN_IS_NIL(test)) { \
      PN_TUPLE_EACH(test, i, v, { \
        potion_source_asmb(P, f, loop, 0, (struct PNSource *)v, reg); }); \
    } \
  } else { \
    potion_source_asmb(P, f, loop, 0, (struct PNSource *)t->a[n], reg); \
  }
#define PN_BLOCK(reg, blk, sig) ({ \
  PN block = potion_send(blk, PN_compile, (PN)f, sig); \
  PN_SIZE num = PN_PUT(f->protos, block); \
  PN_ASM2(OP_PROTO, reg, num); \
  PN_TUPLE_EACH(((struct PNProto *)block)->upvals, i, v, { \
    PN_SIZE numup = PN_GET(f->upvals, v); \
    if (numup != PN_NONE) PN_ASM2(OP_GETUPVAL, reg, numup); \
    else                  PN_ASM2(OP_GETLOCAL, reg, PN_GET(f->locals, v)); \
  }); \
})
#define PN_UPVAL(name) ({ \
  PN_SIZE numl = PN_GET(f->locals, name); \
  PN_SIZE numup = PN_NONE; \
  if (numl == PN_NONE) { \
    numup = PN_GET(f->upvals, name); \
    if (numup == PN_NONE) { \
      vPN(Proto) up = f; \
      int depth = 1; \
      while (PN_IS_PROTO(up->source)) { \
        up = (struct PNProto *)up->source; \
        if (PN_NONE != (numup = PN_GET(up->locals, name))) break; \
        depth++; \
      } \
      if (numup != PN_NONE) { \
        up = f; \
        while (depth--) { \
          up->upvals = PN_PUSH(up->upvals, name); \
          up = (struct PNProto *)up->source; \
        } \
      } \
      numup = PN_GET(f->upvals, name); \
    } \
  } \
  numup; \
})
#define PN_ARG_TABLE(args, reg, inc) potion_arg_asmb(P, f, loop, args, &reg, inc)

#define MAX_JUMPS 1024
struct PNLoop {
  int bjmps[MAX_JUMPS];
  int cjmps[MAX_JUMPS];

  int bjmpc;
  int cjmpc;
};

void potion_source_asmb(Potion *, vPN(Proto), struct PNLoop *, PN_SIZE, vPN(Source), u8);

void potion_arg_asmb(Potion *P, vPN(Proto) f, struct PNLoop *loop, PN args, u8 *reg, int inc)
{
  if (args != PN_NIL) {
    if (PN_PART(args) == AST_TABLE) {
      args = PN_S(args, 0);
      if (!PN_IS_NIL(args)) {
        u8 freg = *reg, sreg = *reg + PN_TUPLE_LEN(args) + 1;
        PN_TUPLE_EACH(args, i, v, {
          if (inc) {
            (*reg)++;
            if (PN_PART(v) == AST_ASSIGN) {
              vPN(Source) lhs = (struct PNSource *)PN_S(v, 0);
              potion_source_asmb(P, f, loop, 0, (struct PNSource *)PN_S(v, 1), sreg + 1);
              if (lhs->part == AST_EXPR && PN_TUPLE_LEN(lhs->a[0]) == 1)
              {
                lhs = (struct PNSource *)PN_TUPLE_AT(lhs->a[0], 0);
                if (lhs->part == AST_MESSAGE || lhs->part == AST_VALUE) {
                  PN_OP op; op.a = lhs->a[0];
                  if (!PN_IS_PTR(lhs->a[0]) && lhs->a[0] == (PN)op.a) {
                    PN_ASM2(OP_LOADPN, sreg, lhs->a[0]);
                  } else {
                    PN_SIZE num = PN_PUT(f->values, lhs->a[0]);
                    PN_ASM2(OP_LOADK, sreg, num);
                  }
                  lhs = NULL;
                }
              }

              if (lhs != NULL)
                potion_source_asmb(P, f, loop, 0, (struct PNSource *)lhs, sreg);

              PN_ASM2(OP_NAMED, freg - 1, sreg + 1);
              PN_REG(f, sreg + 1);
            } else
              potion_source_asmb(P, f, loop, 0, (struct PNSource *)v, *reg);
          } else
            potion_source_asmb(P, f, loop, 0, (struct PNSource *)v, *reg);
        });
      }
    } else {
      if (inc) (*reg)++;
      potion_source_asmb(P, f, loop, 0, (struct PNSource *)args, *reg);
    }
  } else {
    if (inc) (*reg)++;
    PN_ASM2(OP_LOADPN, *reg, args);
  }
}

void potion_source_asmb(Potion *P, vPN(Proto) f, struct PNLoop *loop, PN_SIZE count,
                        vPN(Source) t, u8 reg) {
  PN_REG(f, reg);

  switch (t->part) {
    case AST_CODE:
    case AST_BLOCK:
      if (t->a[0] != PN_NIL) {
        PN_TUPLE_EACH(t->a[0], i, v, {
          potion_source_asmb(P, f, loop, 0, (struct PNSource *)v, reg);
        });
      }
    break;

    case AST_EXPR:
      if (t->a[0] != PN_NIL) {
        PN_TUPLE_EACH(t->a[0], i, v, {
          potion_source_asmb(P, f, loop, i, (struct PNSource *)v, reg);
        });
      }
    break;

    case AST_PROTO:
      PN_BLOCK(reg, t->a[1], t->a[0]);
    break;

    case AST_VALUE: {
      PN_OP op; op.a = t->a[0];
      if (!PN_IS_PTR(t->a[0]) && t->a[0] == (PN)op.a) {
        PN_ASM2(OP_LOADPN, reg, t->a[0]);
      } else {
        PN_SIZE num = PN_PUT(f->values, t->a[0]);
        PN_ASM2(OP_LOADK, reg, num);
      }
      if (t->a[1] != PN_NIL) {
        u8 breg = reg;
        PN_ASM1(OP_SELF, ++breg);
        PN_ARG_TABLE(t->a[1], breg, 1);
        if (t->a[2] != PN_NIL) {
          breg++;
          PN_BLOCK(breg, t->a[2], PN_NIL);
        }
        PN_ASM2(OP_CALL, reg, breg);
      }
    }
    break;

    case AST_ASSIGN: {
      vPN(Source) lhs = (struct PNSource *)t->a[0];
      PN_SIZE num = PN_NONE, c = count;
      u8 opcode = OP_GETUPVAL, breg = reg;

      if (lhs->part == AST_EXPR) {
        unsigned long i = 0;
        c = PN_TUPLE_LEN(lhs->a[0]) - 1;
        for (i = 0; i < c; i++) {
          potion_source_asmb(P, f, loop, i, (struct PNSource *)PN_TUPLE_AT(lhs->a[0], i), reg);
        };
        lhs = (struct PNSource *)PN_TUPLE_AT(lhs->a[0], c);
      }

      if (lhs->part == AST_MESSAGE || lhs->part == AST_QUERY) {
        char first_letter = PN_STR_PTR(lhs->a[0])[0];
        if ((first_letter & 0x80) == 0 && isupper((unsigned char)first_letter)) {
          num = PN_PUT(f->values, lhs->a[0]);
          PN_ASM2(OP_LOADK, breg, num);
          opcode = OP_GLOBAL;
          num = ++breg;
        } else if (c == 0) {
          num = PN_UPVAL(lhs->a[0]);
          if (num == PN_NONE) {
            num = PN_PUT(f->locals, lhs->a[0]);
            opcode = OP_GETLOCAL;
          }
        } else {
          num = PN_PUT(f->values, lhs->a[0]);
          PN_ASM2(OP_LOADK, ++breg, num);
          opcode = OP_DEF;
          num = ++breg;
        }
      } else if (lhs->part == AST_PATH || lhs->part == AST_PATHQ) {
        num = PN_PUT(f->values, lhs->a[0]);
        if (c == 0) {
          PN_PUT(f->paths, PN_NUM(num));
          PN_ASM1(OP_SELF, reg);
        }
        PN_ASM2(OP_LOADK, ++breg, num);
        opcode = OP_GETPATH;
        num = ++breg;
      }

      if (lhs->a[1] != PN_NIL) {
        breg = reg;
        PN_ASM2(opcode, ++breg, num);
        PN_ASM2(OP_CALLSET, reg, breg);
        PN_ARG_TABLE(lhs->a[1], breg, 1);
        // TODO: no block allowed here?
        potion_source_asmb(P, f, loop, 0, (struct PNSource *)t->a[1], ++breg);
        PN_ASM2(OP_CALL, reg, breg);
      } else {
        potion_source_asmb(P, f, loop, 0, (struct PNSource *)t->a[1], breg);
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
          }
          PN_ASM2(OP_SETLOCAL, reg, num);
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
      vPN(Source) lhs = (struct PNSource *)t->a[0];
      PN_SIZE num = PN_UPVAL(lhs->a[0]);
      u8 opcode = OP_SETUPVAL;
      if (num == PN_NONE) {
        num = PN_PUT(f->locals, lhs->a[0]);
        opcode = OP_SETLOCAL;
      }

      if (opcode == OP_SETUPVAL)
        PN_ASM2(OP_GETUPVAL, reg, num);
       else if (opcode == OP_SETLOCAL)
        PN_ASM2(OP_GETLOCAL, reg, num);
      if (PN_IS_NUM(t->a[1])) {
        breg++;
        PN_ASM2(OP_MOVE, breg, reg);
      }
      PN_ASM2(OP_LOADPN, breg + 1, (t->a[1] | PN_FNUMBER));
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

    // TODO: this stuff is ugly and repetitive
    case AST_MESSAGE:
    case AST_QUERY: {
      u8 breg = reg;
      int arg = (t->a[1] != PN_NIL);
      int call = (t->a[2] != PN_NIL || arg);
      if (t->part == AST_MESSAGE && t->a[0] == PN_if) {
        int jmp; breg++;
        PN_ARG_TABLE(t->a[1], breg, 0);
        jmp = PN_OP_LEN(f->asmb);
        PN_ASM2(OP_NOTJMP, breg, 0);
        potion_source_asmb(P, f, loop, 0, (struct PNSource *)t->a[2], reg);
        PN_OP_AT(f->asmb, jmp).b = (PN_OP_LEN(f->asmb) - jmp) - 1;
      } else if (t->part == AST_MESSAGE && t->a[0] == PN_elsif) {
        int jmp1 = PN_OP_LEN(f->asmb), jmp2; breg++;
        PN_ASM2(OP_TESTJMP, breg, 0);
        PN_ARG_TABLE(t->a[1], breg, 0);
        jmp2 = PN_OP_LEN(f->asmb);
        PN_ASM2(OP_NOTJMP, breg, 0);
        potion_source_asmb(P, f, loop, 0, (struct PNSource *)t->a[2], reg);
        PN_OP_AT(f->asmb, jmp1).b = (PN_OP_LEN(f->asmb) - jmp1) - 1;
        PN_OP_AT(f->asmb, jmp2).b = (PN_OP_LEN(f->asmb) - jmp2) - 1;
      } else if (t->part == AST_MESSAGE && t->a[0] == PN_else) {
        int jmp = PN_OP_LEN(f->asmb); breg++;
        PN_ASM2(OP_TESTJMP, breg, 0);
        potion_source_asmb(P, f, loop, 0, (struct PNSource *)t->a[2], reg);
        PN_OP_AT(f->asmb, jmp).b = (PN_OP_LEN(f->asmb) - jmp) - 1;
      } else if (t->part == AST_MESSAGE && t->a[0] == PN_class) {
        u8 breg = reg;
        if (count == 0)
          PN_ASM1(OP_SELF, reg);
        if (t->a[2] != PN_NIL) {
          // TODO: a hack to make sure constructors always return self
          if (PN_S(t->a[2], 0) == PN_NIL)
            PN_S(t->a[2], 0) = PN_AST(CODE, PN_NIL);
          PN ctor = PN_S(t->a[2], 0);
          PN_PUSH(ctor, PN_AST(EXPR, PN_TUP(PN_AST(MESSAGE, potion_str(P, "self")))));
          breg++;
          PN_BLOCK(breg, t->a[2], t->a[1]);
        }
        PN_ASM2(OP_CLASS, reg, breg);
      } else if (t->part == AST_MESSAGE && (t->a[0] == PN_while || t->a[0] == PN_loop)) {
        int jmp1 = 0, jmp2 = PN_OP_LEN(f->asmb); breg++;
        struct PNLoop l; l.bjmpc = 0; l.cjmpc = 0;
        int i;
        if (t->a[0] == PN_while) {
          // TODO: error if args to `loop`?
          PN_ARG_TABLE(t->a[1], breg, 0);
          jmp1 = PN_OP_LEN(f->asmb);
          PN_ASM2(OP_NOTJMP, breg, 0);
        }
        potion_source_asmb(P, f, &l, 0, (struct PNSource *)t->a[2], reg);
        PN_ASM1(OP_JMP, (jmp2 - PN_OP_LEN(f->asmb)) - 1);
        if (t->a[0] == PN_while) {
          PN_OP_AT(f->asmb, jmp1).b = (PN_OP_LEN(f->asmb) - jmp1) - 1;
        }
        for (i = 0; i < l.bjmpc; i++) {
          PN_OP_AT(f->asmb, l.bjmps[i]).a = (PN_OP_LEN(f->asmb) - l.bjmps[i]) - 1;
        }
        for (i = 0; i < l.cjmpc; i++) {
          PN_OP_AT(f->asmb, l.cjmps[i]).a = (jmp2 - l.cjmps[i]) - 1;
        }
      } else if (t->part == AST_MESSAGE && t->a[0] == PN_return) {
        PN_ARG_TABLE(t->a[1], reg, 0);
        PN_ASM1(OP_RETURN, reg);
      } else if (t->part == AST_MESSAGE && t->a[0] == PN_break) {
        if (loop != NULL) {
          loop->bjmps[loop->bjmpc++] = PN_OP_LEN(f->asmb);
          PN_ASM1(OP_JMP, 0);
        } else {
          // TODO: Report error: 'break' outside of loop.
        }
      } else if (t->part == AST_MESSAGE && t->a[0] == PN_continue) {
        if (loop != NULL) {
          loop->cjmps[loop->cjmpc++] = PN_OP_LEN(f->asmb);
          PN_ASM1(OP_JMP, 0);
        } else {
          // TODO: Report error: 'continue' outside of loop.
        }
      } else if (t->part == AST_MESSAGE && t->a[0] == PN_self) {
        PN_ASM1(OP_SELF, reg);
      } else {
        u8 opcode = OP_GETUPVAL;
        PN_SIZE num = PN_NONE;
        if (count == 0 && t->part == AST_MESSAGE) {
          num = PN_UPVAL(t->a[0]);
          if (num == PN_NONE) {
            num = PN_GET(f->locals, t->a[0]);
            opcode = OP_GETLOCAL;
          }
        }

        if (num == PN_NONE && t->a[0] != PN_NIL) {
          u8 oreg = ++breg;
          int jmp = 0;
          num = PN_PUT(f->values, t->a[0]);
          if (count == 0) {
            PN_ASM1(OP_SELF, oreg);
          } else {
            PN_ASM2(OP_MOVE, oreg, reg);
          }
          PN_ASM2(OP_LOADK, reg, num);
          PN_ASM2(t->a[1] != PN_NIL || t->a[2] != PN_NIL ? OP_MESSAGE : OP_BIND, reg, breg);
          if (t->part == AST_QUERY && t->a[1] != PN_NIL) {
            jmp = PN_OP_LEN(f->asmb);
            PN_ASM2(OP_NOTJMP, reg, 0);
          }
#define LOAD_ARG() PN_ARG_TABLE(t->a[1], breg, 1)
          if (arg) {
            u8 part1 = ((struct PNSource *)(t->a[1]))->part;
            if (part1 == AST_VALUE || (part1 == AST_TABLE && t->a[2] == PN_NIL)) {
              LOAD_ARG();
            }
          }
          if (t->a[2] != PN_NIL && ((struct PNSource *)(t->a[2]))->part == AST_PROTO) {
            vPN(Source) t2 = (struct PNSource *)(t->a[2]);
            breg++;
            PN_BLOCK(breg, t2->a[1], t2->a[0]);
          } else {
            if (t->a[1] == PN_NIL && t->a[2] == PN_NIL)
              LOAD_ARG();
            if (t->a[2] != PN_NIL && ((struct PNSource *)(t->a[2]))->part == AST_BLOCK) {
              breg++;
              PN_BLOCK(breg, t->a[2], t->a[1]);
            }
          }
#undef LOAD_ARG
          if (t->part == AST_MESSAGE) {
            PN_ASM2(OP_CALL, reg, breg);
          } else
            if (t->a[1] != PN_NIL) {
              PN_ASM2(OP_CALL, reg, breg);
              PN_OP_AT(f->asmb, jmp).b = (PN_OP_LEN(f->asmb) - jmp) - 1;
            } else {
              PN_ASM2(OP_TEST, reg, breg);
            }
        } else {
          if (num != PN_NONE)
            PN_ASM2(opcode, reg, num);
          if (call) {
            PN_ASM1(OP_SELF, ++breg);
            PN_ARG_TABLE(t->a[1], breg, 1);
            if (t->a[2] != PN_NIL) {
              breg++;
              PN_BLOCK(breg, t->a[2], PN_NIL);
            }
            PN_ASM2(OP_CALL, reg, breg);
          }
        }
      }
      PN_REG(f, breg);
    }
    break;

    case AST_PATH:
    case AST_PATHQ: {
      PN_SIZE num = PN_PUT(f->values, t->a[0]);
      if (count == 0) {
        PN_PUT(f->paths, PN_NUM(num));
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
      PN_SIZE num = PN_PUT(f->values, t->a[0]);
      PN_ASM2(OP_LOADK, reg, num);
      if (t->a[1] != PN_NIL)
        potion_source_asmb(P, f, loop, 0, (struct PNSource *)t->a[1], ++breg);
      else if (t->a[2] != PN_NIL)
        PN_ASM2(OP_LOADPN, ++breg, PN_NIL);
      if (t->a[2] != PN_NIL)
        potion_source_asmb(P, f, loop, 0, (struct PNSource *)t->a[2], ++breg);
      PN_ASM2(OP_NEWLICK, reg, breg);
      PN_REG(f, breg);
    }
    break;

    case AST_TABLE:
      PN_ASM1(OP_NEWTUPLE, reg);
      if (t->a[0] != PN_NIL) {
        PN_TUPLE_EACH(t->a[0], i, v, {
          if (PN_PART(v) == AST_ASSIGN) {
            vPN(Source) lhs = (struct PNSource *)PN_S(v, 0);
            if (lhs->part == AST_EXPR && PN_TUPLE_LEN(lhs->a[0]) == 1)
            {
              lhs = (struct PNSource *)PN_TUPLE_AT(lhs->a[0], 0);
              if (lhs->part == AST_MESSAGE) {
                PN_SIZE num = PN_PUT(f->values, lhs->a[0]);
                PN_ASM2(OP_LOADK, reg + 1, num);
                lhs = NULL;
              }
            }

            if (lhs != NULL)
              potion_source_asmb(P, f, loop, 0, (struct PNSource *)lhs, reg + 1);

            potion_source_asmb(P, f, loop, 0, (struct PNSource *)PN_S(v, 1), reg + 2);
            PN_ASM2(OP_SETTABLE, reg, reg + 2);
            PN_REG(f, reg + 2);
          } else {
            potion_source_asmb(P, f, loop, 0, (struct PNSource *)v, reg + 1);
            PN_ASM2(OP_SETTUPLE, reg, reg + 1);
            PN_REG(f, reg + 1);
          }
        });
      }
    break;
  }
}

PN potion_sig_compile(Potion *P, vPN(Proto) f, PN src) {
  PN sig = PN_TUP0();
  vPN(Source) t = (struct PNSource *)src;
  if (t->part == AST_TABLE && t->a[0] != PN_NIL) {
    PN_TUPLE_EACH(t->a[0], i, v, {
      vPN(Source) expr = (struct PNSource *)v;
      if (expr->part == AST_EXPR) {
        vPN(Source) name = (struct PNSource *)PN_TUPLE_AT(expr->a[0], 0);
        if (name->part == AST_MESSAGE)
        {
          PN_PUT(f->locals, name->a[0]);
          sig = PN_PUSH(PN_PUSH(sig, name->a[0]), PN_NUM('o'));
        }
      } else if (expr->part == AST_ASSIGN) {
        vPN(Source) lhs = (struct PNSource *)expr->a[0];
        if (lhs->part == AST_EXPR && PN_TUPLE_LEN(lhs->a[0]) == 1)
        {
          lhs = (struct PNSource *)PN_TUPLE_AT(lhs->a[0], 0);
          if (lhs->part == AST_MESSAGE) {
            PN_PUT(f->locals, lhs->a[0]);
            sig = PN_PUSH(PN_PUSH(sig, lhs->a[0]), PN_NUM('o'));
          }
        }
      }
    });
  }
  return sig;
}

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
  f->tree = self;
  f->sig = (sig == PN_NIL ? PN_TUP0() : potion_sig_compile(P, f, sig));
  f->asmb = (PN)potion_asm_new(P);

  potion_source_asmb(P, f, NULL, 0, t, 0);
  PN_ASM1(OP_RETURN, 0);

  f->localsize = PN_TUPLE_LEN(f->locals);
  f->upvalsize = PN_TUPLE_LEN(f->upvals);
  f->pathsize = PN_TUPLE_LEN(f->paths);
  return (PN)f;
}

#define READ_U8(ptr) ({u8 rpu = *ptr; ptr += sizeof(u8); rpu;})
#define READ_PN(pn, ptr) ({PN rpn = *(PN *)ptr; ptr += pn; rpn;})
#define READ_CONST(pn, ptr) ({ \
    PN val = READ_PN(pn, ptr); \
    if (PN_IS_PTR(val)) { \
      if (val & 2) { \
        size_t len = ((val ^ 2) >> 4) - 1; \
        val = potion_decimal(P, (char *)ptr, len); \
        ptr += len; \
      } else { \
        size_t len = (val >> 4) - 1; \
        val = potion_str2(P, (char *)ptr, len); \
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
  f->source = READ_CONST(pn, *ptr);
  if (f->source == PN_NIL) f->source = up; 
  f->sig = READ_VALUES(pn, *ptr);
  f->stack = READ_CONST(pn, *ptr);
  f->values = READ_VALUES(pn, *ptr);
  f->paths = READ_VALUES(pn, *ptr);
  f->locals = READ_VALUES(pn, *ptr);
  f->upvals = READ_VALUES(pn, *ptr);
  f->protos = READ_PROTOS(pn, *ptr);

  len = READ_PN(pn, *ptr);
  PN_FLEX_NEW(asmb, PN_TBYTES, PNAsm, len);
  PN_MEMCPY_N(asmb->ptr, *ptr, u8, len);
  asmb->len = len;

  f->asmb = (PN)asmb;
  f->localsize = PN_TUPLE_LEN(f->locals);
  f->upvalsize = PN_TUPLE_LEN(f->upvals);
  f->pathsize = PN_TUPLE_LEN(f->paths);
  *ptr += len;
  return (PN)f;
}

// TODO: load from a stream
#ifndef P2
PN potion_source_load(Potion *P, PN cl, PN buf) {
  u8 *ptr;
  vPN(BHeader) h = (struct PNBHeader *)PN_STR_PTR(buf);
  if ((size_t)PN_STR_LEN(buf) <= sizeof(struct PNBHeader) || 
      strncmp((char *)h->sig, POTION_SIG, 4) != 0)
    return PN_NIL;

  ptr = h->proto;
  return potion_proto_load(P, PN_NIL, h->pn, &ptr);
}
#else
PN p2_source_load(Potion *P, PN cl, PN buf) {
  u8 *ptr;
  vPN(BHeader) h = (struct PNBHeader *)PN_STR_PTR(buf);
  if ((size_t)PN_STR_LEN(buf) <= sizeof(struct PNBHeader) ||
      strncmp((char *)h->sig, P2_SIG, 4) != 0)
    return PN_NIL;

  ptr = h->proto;
  return potion_proto_load(P, PN_NIL, h->pn, &ptr);
}
#endif

#define WRITE_U8(un, ptr) ({*ptr = (u8)un; ptr += sizeof(u8);})
#define WRITE_PN(pn, ptr) ({*(PN *)ptr = pn; ptr += sizeof(PN);})
#define WRITE_CONST(val, ptr) ({ \
    if (PN_IS_STR(val)) { \
      PN count = (PN_STR_LEN(val)+1) << 4; \
      WRITE_PN(count, ptr); \
      PN_MEMCPY_N(ptr, PN_STR_PTR(val), char, PN_STR_LEN(val)); \
      ptr += PN_STR_LEN(val); \
    } else if (PN_IS_DECIMAL(val)) { \
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
    WRITE_TUPLE(tup, ptr) ptr += potion_proto_dump(P, PN_TUPLE_AT(tup, i), \
        out, (char *)ptr - PN_STR_PTR(out)); \
  })

long potion_proto_dump(Potion *P, PN proto, PN out, long pos) {
  vPN(Proto) f = (struct PNProto *)proto;
  char *start = PN_STR_PTR(out) + pos;
  u8 *ptr = (u8 *)start;
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

// TODO: dump to a stream
PN potion_source_dump(Potion *P, PN cl, PN proto) {
  PN pnb = potion_bytes(P, 8192);
  struct PNBHeader h;
  PN_MEMCPY_N(h.sig, POTION_SIG, u8, 4);
  h.major = POTION_MAJOR;
  h.minor = POTION_MINOR;
  h.vmid = POTION_VMID;
  h.pn = (u8)sizeof(PN);

  PN_MEMCPY(PN_STR_PTR(pnb), &h, struct PNBHeader);
  PN_STR_LEN(pnb) = (long)sizeof(struct PNBHeader) +
    potion_proto_dump(P, proto, pnb, sizeof(struct PNBHeader));
  return pnb;
}

PN potion_run(Potion *P, PN code, int jit) {
#ifndef POTION_JIT_TARGET
  if (jit) {
    fprintf(stderr, "** potion not compiled with JIT\n");
    jit = 0;
  }
#endif
  if (jit) {
    PN cl = potion_closure_new(P, (PN_F)potion_jit_proto(P, code, 0), PN_NIL, 1);
    PN_CLOSURE(cl)->data[0] = code;
    return PN_PROTO(code)->jit(P, cl, P->lobby);
  } else {
    return potion_vm(P, code, P->lobby, PN_NIL, 0, NULL);
  }
}

PN potion_eval(Potion *P, PN bytes, int jit) {
  PN code = potion_parse(P, bytes);
  if (PN_TYPE(code) != PN_TSOURCE) return code;
  code = potion_send(code, PN_compile, PN_NIL, PN_NIL);
  return potion_run(P, code, jit);
}

void potion_compiler_init(Potion *P) {
  PN pro_vt = PN_VTABLE(PN_TPROTO);
  potion_method(pro_vt, "call", potion_proto_call, 0);
  potion_method(pro_vt, "tree", potion_proto_tree, 0);
  potion_method(pro_vt, "string", potion_proto_string, 0);
}
