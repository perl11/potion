//
// vm-ppc.c
// the powerpc jit (32-bit only)
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "potion.h"
#include "internal.h"
#include "opcodes.h"
#include "asm.h"

#define RBP(x) (0x18 + (x * sizeof(PN)))
#define REG(x) (x == 0 ? 0 : (x == 1 ? 2 : x + 3))

#define PPC(ins, a, b, c, d) \
  ASM((ins << 2) | ((a >> 2) & 0x3)); \
  ASM((a << 5)   | (b & 0x1F)); \
  ASM(c); ASM(d)
#define PPC3(ins, a, b, c) \
  PPC(ins, a, b, c >> 8, c)
#define PPC2(ins, a, b) \
  PPC(ins, a, 0, b >> 8, b)
#define PPCN(ins, a) \
  ASM(ins << 2); \
  ASM(a >> 16); ASM(a >> 8); ASM(a)
#define PPC_MOV(a, b) \
  PPC(31, b, a, (b << 3) | 0x3, 0x78); // or rA,rB,rB
#define PPC_MATH(do) \
  PPC(31, REG(op->a), REG(op->a), 0x0e, 0x70); /* srawi rA,rA,1 */ \
  PPC(31, REG(op->b), REG(op->b), 0x0e, 0x70); /* srawi rB,rB,1 */ \
  do; /* add, sub, ... */ \
  PPC(21, REG(op->a), REG(op->a), 0x08, 0x3c); /* rlwin rA,rA,1,0,30 */ \
  PPC3(24, REG(op->a), REG(op->a), 1); /* ori rA,1 */ \
  PPC(21, REG(op->b), REG(op->b), 0x08, 0x3c); /* rlwin rB,rB,1,0,30 */ \
  PPC3(24, REG(op->b), REG(op->b), 1); /* ori rB,1 */

void potion_ppc_setup(PNAsm *asmb) {
  PPC3(47, 30, 1, 0xFFF8); // stmw r30,-8(r1)
}

void potion_ppc_stack(PNAsm *asmb, long rsp) {
  rsp = -((rsp+31)&~(15));
  PPC3(37, 1, 1, rsp); // stwu r1,-X(r1)
  PPC_MOV(30, 1); // or r30,r1,r1
}

void potion_ppc_registers(PNAsm *asmb, long start) {
}

void potion_ppc_local(PNAsm *asmb, long reg, long arg) {
}

void potion_ppc_upvals(PNAsm *asmb, long lregs, int upc) {
}

void potion_ppc_move(PNAsm *asmb, PN_OP *op) {
  PPC_MOV(REG(op->a), REG(op->b)); // li rA,B
}

void potion_ppc_loadpn(PNAsm *asmb, PN_OP *op) {
  PPC2(14, REG(op->a), op->b); // li rA,B
}

void potion_ppc_loadk(PNAsm *asmb, PN_OP *op, PN values) {
  PN val = PN_TUPLE_AT(values, op->b);
  PPC2(15, REG(op->a), val >> 16); // lis rA,B
  PPC3(24, REG(op->a), REG(op->a), val); // ori rA,B
}

void potion_ppc_self(PNAsm *asmb, PN_OP *op, long start) {
}

void potion_ppc_getlocal(PNAsm *asmb, PN_OP *op, long regs, PN_F *jit_protos) {
  PPC3(32, REG(op->a), 30, RBP(op->b));
}

void potion_ppc_setlocal(PNAsm *asmb, PN_OP *op, long regs, PN_F *jit_protos) {
  PPC3(36, REG(op->a), 30, RBP(op->b));
}

void potion_ppc_getupval(PNAsm *asmb, PN_OP *op, long lregs) {
}

void potion_ppc_setupval(PNAsm *asmb, PN_OP *op, long lregs) {
}

void potion_ppc_newtuple(PNAsm *asmb, PN_OP *op, long start) {
}

void potion_ppc_settuple(PNAsm *asmb, PN_OP *op, long start) {
}

void potion_ppc_search(PNAsm *asmb, PN_OP *op, long start) {
}

void potion_ppc_settable(PNAsm *asmb, PN_OP *op, long start, PN values) {
}

void potion_ppc_add(PNAsm *asmb, PN_OP *op) {
  PPC_MATH({
    PPC(31, REG(op->a), REG(op->a), REG(op->b) << 3 | 0x2, 0x14); // add rA,rA,rB
  });
}

void potion_ppc_sub(PNAsm *asmb, PN_OP *op) {
  PPC_MATH({
    PPC(31, REG(op->a), REG(op->a), REG(op->b) << 3, 0x50); // subf rA,rA,rB
  });
}

void potion_ppc_mult(PNAsm *asmb, PN_OP *op) {
  PPC_MATH({
    PPC(31, REG(op->a), REG(op->a), REG(op->b) << 3 | 0x1, 0xD6); // mullw rA,rA,rB
  });
}

void potion_ppc_div(PNAsm *asmb, PN_OP *op) {
  PPC_MATH({
    PPC(31, REG(op->a), REG(op->a), REG(op->b) << 3 | 0x3, 0xD6); // divw rA,rA,rB
  });
}

void potion_ppc_rem(PNAsm *asmb, PN_OP *op) {
}

void potion_ppc_pow(PNAsm *asmb, PN_OP *op, long start) {
}

void potion_ppc_neq(PNAsm *asmb, PN_OP *op) {
}

void potion_ppc_eq(PNAsm *asmb, PN_OP *op) {
}

void potion_ppc_lt(PNAsm *asmb, PN_OP *op) {
}

void potion_ppc_lte(PNAsm *asmb, PN_OP *op) {
}

void potion_ppc_gt(PNAsm *asmb, PN_OP *op) {
}

void potion_ppc_gte(PNAsm *asmb, PN_OP *op) {
}

void potion_ppc_bitl(PNAsm *asmb, PN_OP *op) {
}

void potion_ppc_bitr(PNAsm *asmb, PN_OP *op) {
}

void potion_ppc_bind(PNAsm *asmb, PN_OP *op, long start) {
}

void potion_ppc_jmp(PNAsm *asmb, PN_OP *op, PN_OP *start, PNJumps *jmps, size_t *offs, int *jmpc) {
}

void potion_ppc_test_asm(PNAsm *asmb, PN_OP *op, int test) {
}

void potion_ppc_test(PNAsm *asmb, PN_OP *op) {
  potion_ppc_test_asm(asmb, op, 1);
}

void potion_ppc_not(PNAsm *asmb, PN_OP *op) {
  potion_ppc_test_asm(asmb, op, 0);
}

void potion_ppc_cmp(PNAsm *asmb, PN_OP *op) {
}

void potion_ppc_testjmp(PNAsm *asmb, PN_OP *op, PN_OP *start, PNJumps *jmps, size_t *offs, int *jmpc) {
}

void potion_ppc_notjmp(PNAsm *asmb, PN_OP *op, PN_OP *start, PNJumps *jmps, size_t *offs, int *jmpc) {
}

void potion_ppc_call(PNAsm *asmb, PN_OP *op, long start) {
}

void potion_ppc_return(PNAsm *asmb, PN_OP *op) {
  PPC_MOV(3, REG(op->a)); // or r3,r0,r0
  PPC3(32, 1, 1, 0); // lwz r1,(r1)
  PPC3(46, 30, 1, 0xFFF8); // lmw r30,-8(r1)
  ASMI(0x4e800020); // blr
}

void potion_ppc_method(PNAsm *asmb, Potion *P, PN_OP **pos, PN_F *jit_protos, PN protos, long lregs, long start, long regs) {
}

void potion_ppc_finish(PNAsm *asmb) {
}

MAKE_TARGET(ppc);
