//
// asm.h
// some assembler macros
//
// (c) 2008 why the lucky stiff, the freelance professor
//

//
// PNAsm(vt = PN_TUSER, siz, ptr, len)
//   -> PNFlex(vt = PN_TUSER, siz, ...)
// overhead of 6 words on x86, but don't have to
// do constant forwarding tricks.
//
#ifndef POTION_ASM_H
#define POTION_ASM_H

#define ASM_UNIT 512

typedef struct {
  size_t from;
  PN_SIZE to;
} PNJumps;

#define MAKE_TARGET(arch) PNTarget potion_target_##arch = { \
    .setup = potion_##arch##_setup, \
    .stack = potion_##arch##_stack, \
    .registers = potion_##arch##_registers, \
    .local = potion_##arch##_local, \
    .upvals = potion_##arch##_upvals, \
    .jmpedit = potion_##arch##_jmpedit, \
    .op = { \
      (OP_F)NULL, \
      (OP_F)potion_##arch##_move, \
      (OP_F)potion_##arch##_loadk, \
      (OP_F)potion_##arch##_loadpn, \
      (OP_F)potion_##arch##_self, \
      (OP_F)potion_##arch##_newtuple, \
      (OP_F)potion_##arch##_settuple, \
      (OP_F)potion_##arch##_getlocal, \
      (OP_F)potion_##arch##_setlocal, \
      (OP_F)potion_##arch##_getupval, \
      (OP_F)potion_##arch##_setupval, \
      (OP_F)NULL, \
      (OP_F)potion_##arch##_settable, \
      (OP_F)potion_##arch##_newlick, \
      (OP_F)potion_##arch##_getpath, \
      (OP_F)potion_##arch##_setpath, \
      (OP_F)potion_##arch##_add, \
      (OP_F)potion_##arch##_sub, \
      (OP_F)potion_##arch##_mult, \
      (OP_F)potion_##arch##_div, \
      (OP_F)potion_##arch##_rem, \
      (OP_F)potion_##arch##_pow, \
      (OP_F)potion_##arch##_not, \
      (OP_F)potion_##arch##_cmp, \
      (OP_F)potion_##arch##_eq, \
      (OP_F)potion_##arch##_neq, \
      (OP_F)potion_##arch##_lt, \
      (OP_F)potion_##arch##_lte, \
      (OP_F)potion_##arch##_gt, \
      (OP_F)potion_##arch##_gte, \
      (OP_F)potion_##arch##_bitn, \
      (OP_F)potion_##arch##_bitl, \
      (OP_F)potion_##arch##_bitr, \
      (OP_F)potion_##arch##_def, \
      (OP_F)potion_##arch##_bind, \
      (OP_F)potion_##arch##_message, \
      (OP_F)potion_##arch##_jmp, \
      (OP_F)potion_##arch##_test, \
      (OP_F)potion_##arch##_testjmp, \
      (OP_F)potion_##arch##_notjmp, \
      (OP_F)potion_##arch##_named, \
      (OP_F)potion_##arch##_call, \
      (OP_F)potion_##arch##_callset, \
      (OP_F)NULL, \
      (OP_F)potion_##arch##_return, \
      (OP_F)potion_##arch##_method, \
      (OP_F)potion_##arch##_class \
    }, \
    .finish = potion_##arch##_finish, \
    .mcache = potion_##arch##_mcache, \
    .ivars = potion_##arch##_ivars \
  }

#define PN_HAS_UPVALS(v) \
  int v = 0; \
  if (PN_TUPLE_LEN(f->protos) > 0) { \
    PN_TUPLE_EACH(f->protos, i, proto2, { \
      if (PN_TUPLE_LEN(PN_PROTO(proto2)->upvals) > 0) { \
        v = 1; \
      } \
    }); \
  }
  
#define ASM(ins) *asmp = potion_asm_put(P, *asmp, (PN)(ins), sizeof(u8))
#define ASM2(pn) *asmp = potion_asm_put(P, *asmp, (PN)(pn), 2)
#define ASMI(pn) *asmp = potion_asm_put(P, *asmp, (PN)(pn), sizeof(int))
#define ASMN(pn) *asmp = potion_asm_put(P, *asmp, (PN)(pn), sizeof(PN))

PNAsm *potion_asm_new(Potion *);
PNAsm *potion_asm_clear(Potion *, PNAsm *);
PNAsm *potion_asm_put(Potion *, PNAsm *, PN, size_t);
PNAsm *potion_asm_op(Potion *, PNAsm *, u8, int, int);
PNAsm *potion_asm_write(Potion *, PNAsm *, char *, size_t);

#endif
