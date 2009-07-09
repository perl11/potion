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

// STACK LAYOUT
//
// Like on other platforms, Potion attempts to match the conventions
// used by C compilers. In C, the stack layout looks something like:
//
//   sp | linkage (24 bytes) | parameters | locals | saved |
//
// Note that while PowerPC has an EA, registers and stack space are
// the same. This is actually pretty ideal for Potion, since I can
// use the parameters area (the general-purpose registers) as if
// they were Potion's registers, then copy everything to the
// saved registers area when it comes time to make a call. This
// cuts down the assembler in every operation except for OP_CALL.
//
// Now, if OP_CALL proves to be slow, I've considered an optimization.
// Since Potion already uses a contiguous set of registers to pass
// arguments, maybe I could just save the registers and the linkage,
// shift the stack pointer, then set it back once the call is done.
//
// Alternatively, maybe it would be nice to give Potion's VM its
// own set of parameter registers, as a hint to the JIT.

#define RBP(x) (0x18 + (x * sizeof(PN)))

// The EABI reserves GPR1 for a stack pointer, GPR3-GPR7 for function
// argument passing, and GPR3 for function return values. (In Potion,
// it's the same but GPR3 is also used as scratch space and GPR4-7 act
// as general registers when not being used for parameters.)
#define REG(x) (x == 0 ? 0 : (x == 1 ? 2 : x + 2))
// The scratch space, register 3, is referred to as rD in the notation.
#define REG_TMP 3

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
#define PPC_UNBOX() \
  PPC(31, REG(op.a), REG(op.a), 0x0e, 0x70); /* srawi rA,rA,1 */ \
  PPC(31, REG(op.b), REG(op.b), 0x0e, 0x70) /* srawi rB,rB,1 */
#define PPC_MATH(do) \
  PPC_UNBOX(); \
  do; /* add, sub, ... */ \
  PPC(21, REG(op.a), REG(op.a), 0x08, 0x3c); /* rlwin rA,rA,1,0,30 */ \
  PPC3(24, REG(op.a), REG(op.a), 1); /* ori rA,1 */ \
  PPC(21, REG(op.b), REG(op.b), 0x08, 0x3c); /* rlwin rB,rB,1,0,30 */ \
  PPC3(24, REG(op.b), REG(op.b), 1); /* ori rB,1 */
#define PPC_CMP(cmp) \
  PPC_UNBOX(); \
  PPC(31, 7 << 2, REG(op.a), REG(op.b) << 3, 0x40); /* cmplw cr7,rA,rB */ \
  ASMI(cmp | 12); /* bCMP +12 */ \
  PPC2(14, REG(op.a), PN_TRUE); /* li rA,TRUE */ \
  ASMI(0x48000008); /* b +8 */ \
  PPC2(14, REG(op.a), PN_FALSE); /* li rA,FALSE */
#define TAG_JMP(ins, jpos) \
  if (jpos >= pos) { \
    jmps[*jmpc].from = asmp[0]->len; \
    ASMI(ins); \
    jmps[*jmpc].to = jpos + 1; \
    *jmpc = *jmpc + 1; \
  } else if (jpos < pos) { \
    int off = (offs[jpos + 1] - (asmp[0]->len)); \
    if (ins == 0x48000000) \
      ASMI(ins | (off & 0x3FFFFFF)); \
    else \
      ASMI(ins | (off & 0xFFFF)); \
  } else { \
    ASMI(ins); \
  }

void potion_ppc_setup(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp) {
  PPC3(47, 30, 1, 0xFFF8); // stmw r30,-8(r1)
}

void potion_ppc_stack(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, long rsp) {
  rsp = -((rsp+31)&~(15));
  PPC3(37, 1, 1, rsp); // stwu r1,-X(r1)
  PPC_MOV(30, 1); // or r30,r1,r1
}

void potion_ppc_registers(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, long start) {
}

void potion_ppc_local(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, long reg, long arg) {
}

void potion_ppc_upvals(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, long lregs, long start, int upc) {
}

void potion_ppc_jmpedit(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, unsigned char *asmj, int dist) {
  if (asmj[0] == 0x48) {
    asmj[0] |= (dist >> 24) & 3;
    asmj[1] = dist >> 16;
  }
  asmj[2] = dist >> 8;
  asmj[3] = dist + 4;
}

void potion_ppc_move(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PPC_MOV(REG(op.a), REG(op.b)); // li rA,B
}

void potion_ppc_loadpn(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PPC2(14, REG(op.a), op.b); // li rA,B
}

void potion_ppc_loadk(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PN val = PN_TUPLE_AT(f->values, op.b);
  PPC2(15, REG(op.a), val >> 16); // lis rA,B
  PPC3(24, REG(op.a), REG(op.a), val); // ori rA,B
}

void potion_ppc_self(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_ppc_getlocal(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long regs) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PPC3(32, REG(op.a), 30, RBP(op.b)); // lwz rA,-B(rsp)
}

void potion_ppc_setlocal(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long regs) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PPC3(36, REG(op.a), 30, RBP(op.b)); // stw rA,-B(rsp)
}

void potion_ppc_getupval(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long lregs) {
}

void potion_ppc_setupval(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long lregs) {
}

void potion_ppc_newtuple(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_ppc_settuple(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_ppc_search(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_ppc_settable(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_ppc_newlick(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_ppc_getpath(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_ppc_setpath(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_ppc_add(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PPC_MATH({
    PPC(31, REG(op.a), REG(op.a), REG(op.b) << 3 | 0x2, 0x14); // add rA,rA,rB
  });
}

void potion_ppc_sub(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PPC_MATH({
    PPC(31, REG(op.a), REG(op.b), REG(op.a) << 3, 0x50); // subf rA,rA,rB
  });
}

void potion_ppc_mult(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PPC_MATH({
    PPC(31, REG(op.a), REG(op.a), REG(op.b) << 3 | 0x1, 0xD6); // mullw rA,rA,rB
  });
}

void potion_ppc_div(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PPC_MATH({
    PPC(31, REG(op.a), REG(op.a), REG(op.b) << 3 | 0x3, 0xD6); // divw rA,rA,rB
  });
}

void potion_ppc_rem(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PPC_MATH({
    PPC(31, REG_TMP, REG(op.a), REG(op.b) << 3 | 0x3, 0xD6); // divw rD,rA,rB
    PPC(31, REG_TMP, REG_TMP, REG(op.b) << 3 | 0x1, 0xD6); // mullw rD,rD,rB
    PPC(31, REG(op.a), REG_TMP, REG(op.a) << 3, 0x50); // subf rA,rD,rA
  });
}

// TODO: need to keep rB in the saved registers, it gets clobbered.
void potion_ppc_pow(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PPC_MATH({
    PPC_MOV(REG_TMP, REG(op.a)); // mov rD,rB
    PPC(14, REG(op.b), REG(op.b), 0xFF, 0xFF); // addi rD,rD,-1
    PPC(11, 7 << 2, REG(op.b), 0, 0); // cmpwi cr7,rD,0x0
    ASMI(0x409D000C); // ble cr7,-12
    PPC(31, REG(op.a), REG(op.a), REG_TMP << 3 | 0x1, 0xD6); // mullw rA,rA,rA
    ASMI(0x4BFFFFF0); // b -16
  });
}

void potion_ppc_neq(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PPC_CMP(0x419E0000); // beq
}

void potion_ppc_eq(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PPC_CMP(0x409E0000); // bne
}

void potion_ppc_lt(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PPC_CMP(0x409C0000); // bge
}

void potion_ppc_lte(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PPC_CMP(0x419D0000); // bgt
}

void potion_ppc_gt(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PPC_CMP(0x409D0000); // ble
}

void potion_ppc_gte(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PPC_CMP(0x419C0000); // blt
}

void potion_ppc_bitn(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_ppc_bitl(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PPC_MATH({
    PPC(31, REG(op.a), REG(op.a), REG(op.b) << 3, 0x30); // slw rA,rA,rB
  });
}

void potion_ppc_bitr(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PPC_MATH({
    PPC(31, REG(op.a), REG(op.a), REG(op.b) << 3 | 0x6, 0x30); // sraw rA,rA,rB
  });
}

void potion_ppc_def(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_ppc_bind(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_ppc_message(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_ppc_jmp(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, PN_OP *start, PNJumps *jmps, size_t *offs, int *jmpc) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  TAG_JMP(0x48000000, pos + op.a); // b
}

void potion_ppc_test_asm(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, int test) {
}

void potion_ppc_test(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  potion_ppc_test_asm(P, f, asmp, pos, 1);
}

void potion_ppc_not(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  potion_ppc_test_asm(P, f, asmp, pos, 0);
}

void potion_ppc_cmp(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
}

void potion_ppc_testjmp(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, PN_OP *start, PNJumps *jmps, size_t *offs, int *jmpc) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PPC(11, 7 << 2, REG(op.a), 0, PN_FALSE); // cmpwi cr7,rA,0x0
  TAG_JMP(0x409E0000, pos + op.b); // bne
}

void potion_ppc_notjmp(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, PN_OP *start, PNJumps *jmps, size_t *offs, int *jmpc) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PPC(11, 7 << 2, REG(op.a), 0, PN_FALSE); // cmpwi cr7,rA,0x0
  TAG_JMP(0x419E0000, pos + op.b); // beq
}

void potion_ppc_named(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_ppc_call(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_ppc_callset(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_ppc_return(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PPC_MOV(3, REG(op.a)); // or r3,rA,rA
  PPC3(32, 1, 1, 0); // lwz r1,(r1)
  PPC3(46, 30, 1, 0xFFF8); // lmw r30,-8(r1)
  ASMI(0x4e800020); // blr
}

void potion_ppc_method(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_OP **pos, long lregs, long start, long regs) {
}

void potion_ppc_class(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_ppc_finish(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp) {
}

void potion_ppc_mcache(Potion *P, vPN(Vtable) vt, PNAsm * volatile *asmp) {
}

void potion_ppc_ivars(Potion *P, PN ivars, PNAsm * volatile *asmp) {
}

MAKE_TARGET(ppc);
