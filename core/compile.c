//
// compile.c
// ast to bytecode
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "potion.h"
#include "internal.h"
#include "pn-ast.h"
#include "opcodes.h"

#define PN_ASM1(ins, _a) ({ \
    (*pos)->code = (u8)ins; \
    (*pos)->a    = (int)_a; \
    (*pos)++; \
  })

#define PN_ASM2(ins, _a, _b) ({ \
    (*pos)->code = (u8)ins; \
    (*pos)->a    = (int)_a; \
    (*pos)->b    = (int)_b; \
    (*pos)++; \
  })

const char *potion_op_names[] = {
  "noop", "move", "loadk", "loadpn",
  "newtuple", "settuple",
  "getlocal", "setlocal", "getupval", "setupval",
  "gettable", "settable", "getpath", "setpath",
  "add", "sub", "mult", "div", "mod", "pow",
  "not", "cmp", "eq", "neq",
  "lt", "lte", "gt", "gte", "bitl", "bitr",
  "bind", "jump", "test", "testjmp", "notjmp",
  "call", "tailcall", "return", "proto"
};

const u8 potion_op_args[] = {
  0, 2, 2, 2,
  2, 2,
  2, 2, 2, 2,
  2, 2, 2, 2,
  2, 2, 2, 2, 2, 2,
  1, 2, 2, 2,
  2, 2, 2, 2, 2, 2,
  2, 1, 2, 2, 2,
  2, 2, 1, 2
};

PN potion_proto_call(Potion *P, PN cl, PN self, PN args) {
  return potion_vm(P, self, args, 0, NULL);
}

PN potion_proto_string(Potion *P, PN cl, PN self) {
  struct PNProto *t = (struct PNProto *)self;
  int x = 0;
  PN_SIZE num = 1;
  PN_SIZE numcols;
  PN_OP *pos, *end;
  PN out = potion_byte_str(P, "; function definition");
  pn_printf(P, out, ": %p ; %u bytes\n", t, PN_STR_LEN(t->asmb));
  pn_printf(P, out, "; (");
  PN_TUPLE_EACH(t->sig, i, v, {
    if (PN_IS_NUM(v)) {
      if (i == x)
        x += PN_INT(v);
      else if (v != PN_ZERO)
        pn_printf(P, out, "=%c, ", (int)PN_INT(v));
      else
        pn_printf(P, out, ", ");
    } else
      potion_bytes_obj_string(P, out, v);
  });
  pn_printf(P, out, ") %ld registers\n", PN_INT(t->stack));
  PN_TUPLE_EACH(t->locals, i, v, {
    pn_printf(P, out, ".local \"");
    potion_bytes_obj_string(P, out, v);
    pn_printf(P, out, "\" ; %u\n", i);
  });
  PN_TUPLE_EACH(t->upvals, i, v, {
    pn_printf(P, out, ".upval \"");
    potion_bytes_obj_string(P, out, v);
    pn_printf(P, out, "\" ; %u\n", i);
  });
  PN_TUPLE_EACH(t->values, i, v, {
    pn_printf(P, out, ".value ");
    potion_bytes_obj_string(P, out, v);
    pn_printf(P, out, " ; %u\n", i);
  });
  PN_TUPLE_EACH(t->protos, i, v, {
    potion_bytes_obj_string(P, out, v);
  });
  pos = (PN_OP *)PN_STR_PTR(t->asmb);
  end = (PN_OP *)(PN_STR_PTR(t->asmb) + PN_STR_LEN(t->asmb));
  numcols = (int)ceil(log10(end - pos));
  while (pos < end) {
    const int commentoffset = 20;
    int width = pn_printf(P, out, "[%*u] %-8s %d", numcols, num, potion_op_names[pos->code], pos->a);
    if (potion_op_args[pos->code] > 1) {
      width += pn_printf(P, out, " %d", pos->b);
    }
    if (width < commentoffset) {
      pn_printf(P, out, "%*s", commentoffset - width, "");
    } else {
      pn_printf(P, out, " ");
    }
    // TODO: Byte code listing: instead of using tabs, pad with spaces to make everything line up
    switch (pos->code) {
      case OP_JMP:
        pn_printf(P, out, "; to %d", num + pos->a + 1);
        break;
      case OP_NOTJMP:
      case OP_TESTJMP:
        pn_printf(P, out, "; to %d", num + pos->b + 1);
        break;
      case OP_LOADPN:
        pn_printf(P, out, "; ");
        potion_bytes_obj_string(P, out, pos->b);
        break;
      case OP_LOADK:
        pn_printf(P, out, "; ");
        potion_bytes_obj_string(P, out, PN_TUPLE_AT(t->values, pos->b));
        break;
      case OP_SETLOCAL:
      case OP_GETLOCAL:
        pn_printf(P, out, "; ");
        potion_bytes_obj_string(P, out, PN_TUPLE_AT(t->locals, pos->b));
        break;
    }
    pn_printf(P, out, "\n");
    pos++; num++;
  }
  pn_printf(P, out, "; function end\n");
  return out;
}

#define PN_REG(f, reg) \
  if (reg >= PN_INT(f->stack)) \
    f->stack = PN_NUM(reg + 1)
#define PN_ARG(n, reg) \
  potion_source_asmb(P, f, loop, (struct PNSource *)t->a[n], reg, pos)
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
      struct PNProto *up = f; \
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

#define MAX_JUMPS 1024
struct PNLoop {
  PN_OP *bjmps[MAX_JUMPS];
  PN_OP *cjmps[MAX_JUMPS];

  int bjmpc;
  int cjmpc;
};

void potion_source_asmb(Potion *P, struct PNProto *f, struct PNLoop *loop, struct PNSource *t, u8 reg, PN_OP **pos) {
  PN_REG(f, reg);

  switch (t->part) {
    case AST_CODE:
    case AST_BLOCK:
    case AST_EXPR:
      PN_TUPLE_EACH(t->a[0], i, v, {
        potion_source_asmb(P, f, loop, (struct PNSource *)v, reg, pos);
      });
    break;

    case AST_PROTO:
      PN_BLOCK(reg, t->a[1], t->a[0]);
    break;

    case AST_VALUE:
      (*pos)->a = (int)t->a[0];
      if (!PN_IS_PTR(t->a[0]) && t->a[0] == (*pos)->a) {
        PN_ASM2(OP_LOADPN, reg, t->a[0]);
      } else {
        PN_SIZE num = PN_PUT(f->values, t->a[0]);
        PN_ASM2(OP_LOADK, reg, num);
      }
    break;

    case AST_ASSIGN: {
      struct PNSource *lhs = (struct PNSource *)t->a[0];
      PN_SIZE num = PN_NONE;
      u8 opcode = OP_SETUPVAL;

      if (lhs->part == AST_MESSAGE || lhs->part == AST_QUERY) {
        num = PN_UPVAL(lhs->a[0]);
        if (num == PN_NONE) {
          num = PN_PUT(f->locals, lhs->a[0]);
          opcode = OP_SETLOCAL;
        }
      } else if (lhs->part == AST_PATH || lhs->part == AST_PATHQ) {
        num = PN_PUT(f->values, lhs->a[0]);
        opcode = OP_SETPATH;
      }

      potion_source_asmb(P, f, loop, (struct PNSource *)t->a[1], reg, pos);

      if (opcode == OP_SETUPVAL) {
        if (lhs->part == AST_QUERY) {
          PN_ASM2(OP_GETUPVAL, reg, num);
          PN_ASM2(OP_TESTJMP, reg, 1);
        }
      } else if (opcode == OP_SETLOCAL) {
        if (lhs->part == AST_QUERY) {
          PN_ASM2(OP_GETLOCAL, reg, num);
          PN_ASM2(OP_TESTJMP, reg, 1);
        }
      } else if (opcode == OP_SETPATH) {
        if (lhs->part == AST_PATHQ) {
          PN_ASM2(OP_GETPATH, reg, num);
          PN_ASM2(OP_TESTJMP, num, 1);
        }
      }
      PN_ASM2(opcode, reg, num);
    }
    break;

    case AST_INC: {
      u8 breg = reg;
      struct PNSource *lhs = (struct PNSource *)t->a[0];
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
      PN_ASM2(OP_LOADPN, breg + 1, (t->a[1] | PN_TNUMBER));
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

    case AST_AND: case AST_OR: {
      PN_OP *jmp;
      PN_ARG(0, reg);
      jmp = *pos;
      if (t->part == AST_AND)
        PN_ASM2(OP_NOTJMP, reg, 0);
      else
        PN_ASM2(OP_TESTJMP, reg, 0);
      PN_ARG(1, reg);
      jmp->b = (*pos - jmp) - 1;
    }
    break;

#define PN_ARG_TABLE(args, reg1, reg2) \
  if (arg) { \
    PN test = args; \
    if (PN_PART(test) == AST_TABLE) { \
      test = PN_S(args, 0); \
      if (!PN_IS_NIL(test)) { \
        PN_TUPLE_EACH(test, i, v, { \
          potion_source_asmb(P, f, loop, (struct PNSource *)v, reg1, pos); }); \
      } \
    } else { \
      potion_source_asmb(P, f, loop, (struct PNSource *)test, reg2, pos); \
    } \
  } else \
    PN_ASM2(OP_LOADPN, reg1, args)

    // TODO: this stuff is ugly and repetitive
    case AST_MESSAGE:
    case AST_QUERY: {
      u8 breg = reg;
      int arg = (t->a[1] != PN_NIL);
      int call = (t->a[2] != PN_NIL || arg);
      if (t->a[0] == PN_if) {
        PN_OP *jmp; breg++;
        PN_ARG_TABLE(t->a[1], breg, breg);
        jmp = *pos;
        PN_ASM2(OP_NOTJMP, breg, 0);
        potion_source_asmb(P, f, loop, (struct PNSource *)t->a[2], reg, pos);
        jmp->b = (*pos - jmp) - 1;
      } else if (t->a[0] == PN_elsif) {
        PN_OP *jmp1 = *pos, *jmp2; breg++;
        PN_ASM2(OP_TESTJMP, breg, 0);
        PN_ARG_TABLE(t->a[1], breg, breg);
        jmp2 = *pos;
        PN_ASM2(OP_NOTJMP, breg, 0);
        potion_source_asmb(P, f, loop, (struct PNSource *)t->a[2], reg, pos);
        jmp1->b = (*pos - jmp1) - 1;
        jmp2->b = (*pos - jmp2) - 1;
      } else if (t->a[0] == PN_else) {
        PN_OP *jmp = *pos; breg++;
        PN_ASM2(OP_TESTJMP, breg, 0);
        potion_source_asmb(P, f, loop, (struct PNSource *)t->a[2], reg, pos);
        jmp->b = (*pos - jmp) - 1;
      } else if (t->a[0] == PN_loop) {
        PN_OP *jmp = *pos; breg++;
        PN_ARG_TABLE(t->a[1], breg, breg);
        potion_source_asmb(P, f, loop, (struct PNSource *)t->a[2], reg, pos);
        PN_ASM1(OP_JMP, (jmp - *pos) - 1);
      } else if (t->a[0] == PN_while) {
        PN_OP *jmp1, *jmp2 = *pos; breg++;
        struct PNLoop l; l.bjmpc = 0; l.cjmpc = 0;
        int i;
        PN_ARG_TABLE(t->a[1], breg, breg);
        jmp1 = *pos;
        PN_ASM2(OP_NOTJMP, breg, 0);
        potion_source_asmb(P, f, &l, (struct PNSource *)t->a[2], reg, pos);
        PN_ASM1(OP_JMP, (jmp2 - *pos) - 1);
        jmp1->b = (*pos - jmp1) - 1;
        for (i = 0; i < l.bjmpc; i++) {
          l.bjmps[i]->a = (*pos - l.bjmps[i]) - 1;
        }
        for (i = 0; i < l.cjmpc; i++) {
          l.cjmps[i]->a = (jmp2 - l.cjmps[i]) - 1;
        }
      } else if (t->a[0] == PN_return) {
        PN_ARG_TABLE(t->a[1], reg, reg);
        PN_ASM1(OP_RETURN, reg);
      } else if (t->a[0] == PN_break) {
        if (loop != NULL) {
          loop->bjmps[loop->bjmpc++] = *pos;
          PN_ASM1(OP_JMP, 0);
        } else {
          // TODO: Report error: 'break' outside of loop.
        }
      } else if (t->a[0] == PN_continue) {
        if (loop != NULL) {
          loop->cjmps[loop->cjmpc++] = *pos;
          PN_ASM1(OP_JMP, 0);
        } else {
          // TODO: Report error: 'continue' outside of loop.
        }
      } else {
        u8 opcode = OP_GETUPVAL;
        PN_SIZE num = PN_UPVAL(t->a[0]);
        if (num == PN_NONE) {
          num = PN_GET(f->locals, t->a[0]);
          opcode = OP_GETLOCAL;
        }

        if (num == PN_NONE) {
          PN_ARG_TABLE(t->a[1], ++breg, breg);
          if (t->a[2] != PN_NIL)
            PN_BLOCK(++breg, t->a[2], PN_NIL);
          num = PN_PUT(f->values, t->a[0]);
          PN_ASM2(OP_LOADK, ++breg, num);
          PN_ASM2(OP_BIND, breg, reg);
          if (t->part == AST_MESSAGE) {
            PN_ASM2(OP_CALL, reg, breg);
          } else
            PN_ASM2(OP_TEST, reg, breg);
        } else {
          if (t->part == AST_QUERY) {
            PN_ASM2(opcode, reg, num);
            PN_ASM2(OP_TEST, reg, reg);
          } else if (call) {
            PN_ARG_TABLE(t->a[1], ++breg, breg);
            if (t->a[2] != PN_NIL)
              PN_BLOCK(++breg, t->a[2], PN_NIL);
            PN_ASM2(opcode, ++breg, num);
            PN_ASM2(OP_CALL, reg, breg);
          } else
            PN_ASM2(opcode, reg, num);
        }
      }
      PN_REG(f, breg);
    }
    break;

    case AST_PATH:
    case AST_PATHQ: {
      PN_SIZE num = PN_PUT(f->values, t->a[0]);
      u8 breg = reg;
      PN_ASM2(OP_LOADK, ++breg, num);
      PN_ASM2(OP_GETPATH, breg, reg);
      if (t->part == AST_PATHQ)
        PN_ASM2(OP_TEST, reg, breg);
      else
        PN_ASM2(OP_MOVE, reg, breg);
    }
    break;

    case AST_TABLE:
      PN_ASM1(OP_NEWTUPLE, reg);
      PN_TUPLE_EACH(t->a[0], i, v, {
        if (PN_PART(v) == AST_ASSIGN) {
          struct PNSource *lhs = (struct PNSource *)PN_S(v, 0);
          potion_source_asmb(P, f, loop, (struct PNSource *)PN_S(v, 1), reg + 1, pos);
          if (lhs->part == AST_MESSAGE) {
            PN_SIZE num = PN_PUT(f->values, lhs->a[0]);
            PN_ASM2(OP_SETTABLE, reg, num);
          }
        } else {
          potion_source_asmb(P, f, loop, (struct PNSource *)v, reg + 1, pos);
          PN_ASM2(OP_SETTUPLE, reg, reg + 1);
        }
      });
    break;
  }
}

PN potion_sig_compile(Potion *P, struct PNProto *f, PN src) {
  PN sig = PN_TUP0();
  struct PNSource *t = (struct PNSource *)src;
  if (t->part == AST_TABLE && t->a[0] != PN_NIL) {
    sig = PN_PUSH(sig, PN_NUM(0));
    PN_TUPLE_EACH(t->a[0], i, v, {
      struct PNSource *expr = (struct PNSource *)v;
      if (expr->part == AST_EXPR) {
        struct PNSource *name = (struct PNSource *)PN_TUPLE_AT(expr->a[0], 0);
        if (name->part == AST_MESSAGE)
        {
          PN_PUT(f->locals, name->a[0]);
          sig = PN_PUSH(sig, name->a[0]);
        }
      } else if (expr->part == AST_ASSIGN) {
        struct PNSource *name = (struct PNSource *)expr->a[0];
        if (name->part == AST_MESSAGE)
        {
          PN_PUT(f->locals, name->a[0]);
          sig = PN_PUSH(sig, name->a[0]);
        }
      }
      sig = PN_PUSH(sig, PN_NUM(0));
    });
    PN_TUPLE_AT(sig, 0) = PN_NUM(PN_TUPLE_LEN(sig) - 1);
  }
  return sig;
}

PN potion_source_compile(Potion *P, PN cl, PN self, PN source, PN sig) {
  struct PNProto *f;
  struct PNSource *t = (struct PNSource *)self;
  char *start;
  PN_OP **pos, *ptr;
  switch (t->part) {
    case AST_CODE:
    case AST_BLOCK: break;
    default: return PN_NIL; // TODO: error
  }

  f = PN_OBJ_ALLOC(struct PNProto, PN_TPROTO, 0);
  f->source = source;
  f->stack = PN_NUM(1);
  f->protos = PN_TUP0();
  f->locals = PN_TUP0();
  f->upvals = PN_TUP0();
  f->values = PN_TUP0();
  f->sig = (sig == PN_NIL ? PN_TUP0() : potion_sig_compile(P, f, sig));
  f->asmb = potion_bytes(P, 8192);

  ptr = (PN_OP *)(start = PN_STR_PTR(f->asmb));
  pos = &ptr;
  potion_source_asmb(P, f, NULL, t, 0, pos);
  PN_ASM1(OP_RETURN, 0);
  // TODO: byte strings should be more flexible than this
  PN_STR_LEN(f->asmb) = (char *)ptr - start;
  f->localsize = PN_TUPLE_LEN(f->locals);
  f->upvalsize = PN_TUPLE_LEN(f->upvals);
  return (PN)f;
}

#define READ_U8(ptr) ({u8 rpu = *ptr; ptr += sizeof(u8); rpu;})
#define READ_PN(pn, ptr) ({PN rpn = *(PN *)ptr; ptr += pn; rpn;})
#define READ_CONST(pn, ptr) ({ \
    PN val = READ_PN(pn, ptr); \
    if (PN_IS_PTR(val)) { \
      if (val & 2) { \
        size_t len = ((val ^ 2) >> 4) - 1; \
        struct PNDecimal *n = PN_OBJ_ALLOC(struct PNDecimal, PN_TNUMBER, sizeof(PN) * len); \
        n->len = len; n->sign = READ_U8(ptr); \
        PN_MEMCPY_N(n->digits, ptr, PN, len); \
        val = (PN)n; \
        ptr += sizeof(PN) * len; \
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

PN potion_proto_load(Potion *P, PN up, u8 pn, u8 **ptr) {
  PN len = 0;
  struct PNProto *f = PN_OBJ_ALLOC(struct PNProto, PN_TPROTO, 0);
  f->source = READ_CONST(pn, *ptr);
  if (f->source == PN_NIL) f->source = up; 
  f->sig = READ_VALUES(pn, *ptr);
  f->stack = READ_CONST(pn, *ptr);
  f->values = READ_VALUES(pn, *ptr);
  f->locals = READ_VALUES(pn, *ptr);
  f->upvals = READ_VALUES(pn, *ptr);
  f->protos = READ_PROTOS(pn, *ptr);
  len = READ_PN(pn, *ptr);
  f->asmb = potion_bytes(P, len);
  PN_MEMCPY_N(PN_STR_PTR(f->asmb), *ptr, u8, len);
  f->localsize = PN_TUPLE_LEN(f->locals);
  f->upvalsize = PN_TUPLE_LEN(f->upvals);
  *ptr += len;
  return (PN)f;
}

// TODO: load from a stream
PN potion_source_load(Potion *P, PN cl, PN buf) {
  u8 *ptr;
  struct PNBHeader *h = (struct PNBHeader *)PN_STR_PTR(buf);
  if ((size_t)PN_STR_LEN(buf) <= sizeof(struct PNBHeader) || 
      strncmp((char *)h->sig, POTION_SIG, 4) != 0)
    return PN_NIL;

  ptr = h->proto;
  return potion_proto_load(P, PN_NIL, h->pn, &ptr);
}

#define WRITE_U8(un, ptr) ({*ptr = (u8)un; ptr += sizeof(u8);})
#define WRITE_PN(pn, ptr) ({*(PN *)ptr = pn; ptr += sizeof(PN);})
#define WRITE_CONST(val, ptr) ({ \
    if (PN_IS_STR(val)) { \
      PN count = (PN_STR_LEN(val)+1) << 4; \
      WRITE_PN(count, ptr); \
      PN_MEMCPY_N(ptr, PN_STR_PTR(val), char, PN_STR_LEN(val)); \
      ptr += PN_STR_LEN(val); \
    } else if (PN_IS_DECIMAL(val)) { \
      struct PNDecimal *n = (struct PNDecimal *)val; \
      PN count = ((n->len+1) << 4) | 2; \
      WRITE_PN(count, ptr); \
      WRITE_U8(n->sign, ptr); \
      PN_MEMCPY_N(ptr, n->digits, PN, n->len); \
      ptr += sizeof(PN) * n->len; \
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
  struct PNProto *f = (struct PNProto *)proto;
  char *start = PN_STR_PTR(out) + pos;
  u8 *ptr = (u8 *)start;
  WRITE_CONST(f->source, ptr);
  WRITE_VALUES(f->sig, ptr);
  WRITE_CONST(f->stack, ptr);
  WRITE_VALUES(f->values, ptr);
  WRITE_VALUES(f->locals, ptr);
  WRITE_VALUES(f->upvals, ptr);
  WRITE_PROTOS(f->protos, ptr);
  WRITE_PN(PN_STR_LEN(f->asmb), ptr);
  PN_MEMCPY_N(ptr, PN_STR_PTR(f->asmb), u8, PN_STR_LEN(f->asmb));
  ptr += PN_STR_LEN(f->asmb);
  return (char *)ptr - start;
}

PN potion_proto__link(Potion *P, PN cl, PN proto, PN link) {
  struct PNProto *f = (struct PNProto *)proto;
  PN_LINK(f->source);
  PN_LINK(f->sig);
  PN_LINK(f->stack);
  PN_LINK(f->locals);
  PN_LINK(f->upvals);
  PN_LINK(f->values);
  PN_LINK(f->protos);
  PN_LINK(f->asmb);
  return link;
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

PN potion_run(Potion *P, PN code) {
#if POTION_JIT == 1
  PN_F func = potion_x86_proto(P, code);
  return func(P, PN_NIL, PN_NIL);
#else
  return potion_vm(P, code, PN_NIL, 0, NULL);
#endif
}

PN potion_eval(Potion *P, const char *str) {
  PN bytes = potion_byte_str(P, str);
  PN code = potion_parse(P, bytes);
  code = potion_send(code, PN_compile, PN_NIL, PN_NIL);
  return potion_run(P, code);
}

void potion_compiler_init(Potion *P) {
  PN pro_vt = PN_VTABLE(PN_TPROTO);
  potion_method(pro_vt, "call", potion_proto_call, 0);
  potion_method(pro_vt, "string", potion_proto_string, 0);
  potion_method(pro_vt, "~link", potion_proto__link, 0);
}
