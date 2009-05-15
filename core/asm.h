//
// asm.h
// some assembler macros
//
// (c) 2008 why the lucky stiff, the freelance professor
//

typedef struct PNJitAsm {
  u8 *ptr;
  PN_SIZE len, capa;
} PNAsm;

typedef struct {
  size_t from;
  PN_OP *to;
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
      (OP_F)NULL, \
      (OP_F)NULL, \
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
      (OP_F)potion_##arch##_bitl, \
      (OP_F)potion_##arch##_bitr, \
      (OP_F)potion_##arch##_bind, \
      (OP_F)potion_##arch##_jmp, \
      (OP_F)potion_##arch##_test, \
      (OP_F)potion_##arch##_testjmp, \
      (OP_F)potion_##arch##_notjmp, \
      (OP_F)potion_##arch##_call, \
      (OP_F)NULL, \
      (OP_F)potion_##arch##_return, \
      (OP_F)potion_##arch##_method, \
    }, \
    .finish = potion_##arch##_finish \
  }

#define ASM(ins) potion_asm_put(asmb, (PN)ins, sizeof(u8))
#define ASM2(pn) potion_asm_put(asmb, (PN)(pn), 2)
#define ASMI(pn) potion_asm_put(asmb, (PN)(pn), sizeof(int))
#define ASMN(pn) potion_asm_put(asmb, (PN)pn, sizeof(PN))

PNAsm *potion_asm_new();
void potion_asm_put(PNAsm *, PN, size_t);
