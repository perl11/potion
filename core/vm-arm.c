/**\file vm-arm.c
the arm7 jit (32-bit only), unfinished!
\see core/vm.c and doc/INTERNALS.md

(c) 2008 why the lucky stiff, the freelance professor
(c) 2014 perl11.org */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "potion.h"
#include "internal.h"
#include "opcodes.h"
#include "asm.h"

/**\par STACK LAYOUT

 Like on other platforms, Potion attempts to match the conventions
 used by C compilers. In C, the stack layout looks something like:

   sp | linkage (24 bytes) | parameters | locals | saved |

 Note that while PowerPC has an EA, registers and stack space are
 the same. This is actually pretty ideal for Potion, since I can
 use the parameters area (the general-purpose registers) as if
 they were Potion's registers, then copy everything to the
 saved registers area when it comes time to make a call. This
 cuts down the assembler in every operation except for OP_CALL.

 Now, if OP_CALL proves to be slow, I've considered an optimization.
 Since Potion already uses a contiguous set of registers to pass
 arguments, maybe I could just save the registers and the linkage,
 shift the stack pointer, then set it back once the call is done.

 Alternatively, maybe it would be nice to give Potion's VM its
 own set of parameter registers, as a hint to the JIT.
*/

#define RBP(x) (0x18 + (x * sizeof(PN)))

/// The EABI reserves GPR1 for a stack pointer, GPR3-GPR7 for function
/// argument passing, and GPR3 for function return values. (In Potion,
/// it's the same but GPR3 is also used as scratch space and GPR4-7 act
/// as general registers when not being used for parameters.)
#define REG(x) (x == 0 ? 0 : (x == 1 ? 2 : x + 2))
/// The scratch space, register 3, is referred to as rD in the notation.
#define REG_TMP 3

#define ARM(ins, a, b, c, d) \
  ASM((ins << 2) | ((a >> 2) & 0x3)); \
  ASM((a << 5)   | (b & 0x1F)); \
  ASM(c); ASM(d)
#define ARM3(ins, a, b, c) \
  ARM(ins, a, b, c >> 8, c)
#define ARM2(ins, a, b) \
  ARM(ins, a, 0, b >> 8, b)
#define ARMN(ins, a) \
  ASM(ins << 2); \
  ASM(a >> 16); ASM(a >> 8); ASM(a)

#define ARM_MOV(a, b) \
  ARM(31, b, a, (b << 3) | 0x3, 0x78); // or rA,rB,rB
#define ARM_UNBOX() \
  ARM(31, REG(op.a), REG(op.a), 0x0e, 0x70); /* srawi rA,rA,1 */ \
  ARM(31, REG(op.b), REG(op.b), 0x0e, 0x70) /* srawi rB,rB,1 */
#define ARM_MATH(do) \
  ARM_UNBOX(); \
  do; /* add, sub, ... */ \
  ARM(21, REG(op.a), REG(op.a), 0x08, 0x3c); /* rlwin rA,rA,1,0,30 */ \
  ARM3(24, REG(op.a), REG(op.a), 1); /* ori rA,1 */ \
  ARM(21, REG(op.b), REG(op.b), 0x08, 0x3c); /* rlwin rB,rB,1,0,30 */ \
  ARM3(24, REG(op.b), REG(op.b), 1); /* ori rB,1 */
#define ARM_CMP(cmp) \
  ARM_UNBOX(); \
  ARM(31, 7 << 2, REG(op.a), REG(op.b) << 3, 0x40); /* cmplw cr7,rA,rB */ \
  ASMI(cmp | 12); /* bCMP +12 */ \
  ARM2(14, REG(op.a), PN_TRUE); /* li rA,TRUE */ \
  ASMI(0x48000008); /* b +8 */ \
  ARM2(14, REG(op.a), PN_FALSE); /* li rA,FALSE */
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

void potion_arm_setup(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp) {
  ARM3(47, 30, 1, 0xFFF8); // stmw r30,-8(r1)
}

void potion_arm_stack(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, long rsp) {
  rsp = -((rsp+31)&~(15));
  ARM3(37, 1, 1, rsp); // stwu r1,-X(r1)
  ARM_MOV(30, 1); // or r30,r1,r1
}

void potion_arm_registers(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, long start) {
}

void potion_arm_local(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, long reg, long arg) {
}

void potion_arm_upvals(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, long lregs, long start, int upc) {
}

void potion_arm_jmpedit(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, unsigned char *asmj, int dist) {
  if (asmj[0] == 0x48) {
    asmj[0] |= (dist >> 24) & 3;
    asmj[1] = dist >> 16;
  }
  asmj[2] = dist >> 8;
  asmj[3] = dist + 4;
}

void potion_arm_move(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  ARM_MOV(REG(op.a), REG(op.b)); // li rA,B
}

void potion_arm_loadpn(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  ARM2(14, REG(op.a), op.b); // li rA,B
}

void potion_arm_loadk(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  PN val = PN_TUPLE_AT(f->values, op.b);
  ARM2(15, REG(op.a), val >> 16); // lis rA,B
  ARM3(24, REG(op.a), REG(op.a), val); // ori rA,B
}

void potion_arm_self(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_arm_getlocal(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long regs) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  ARM3(32, REG(op.a), 30, RBP(op.b)); // lwz rA,-B(rsp)
}

void potion_arm_setlocal(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long regs) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  ARM3(36, REG(op.a), 30, RBP(op.b)); // stw rA,-B(rsp)
}

void potion_arm_getupval(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long lregs) {
}

void potion_arm_setupval(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long lregs) {
}

void potion_arm_global(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_arm_newtuple(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_arm_gettuple(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_arm_settuple(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_arm_search(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_arm_gettable(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}
void potion_arm_settable(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_arm_newlick(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_arm_getpath(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_arm_setpath(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_arm_add(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  ARM_MATH({
    ARM(31, REG(op.a), REG(op.a), REG(op.b) << 3 | 0x2, 0x14); // add rA,rA,rB
  });
}

void potion_arm_sub(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  ARM_MATH({
    ARM(31, REG(op.a), REG(op.b), REG(op.a) << 3, 0x50); // subf rA,rA,rB
  });
}

void potion_arm_mult(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  ARM_MATH({
    ARM(31, REG(op.a), REG(op.a), REG(op.b) << 3 | 0x1, 0xD6); // mullw rA,rA,rB
  });
}

void potion_arm_div(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  ARM_MATH({
    ARM(31, REG(op.a), REG(op.a), REG(op.b) << 3 | 0x3, 0xD6); // divw rA,rA,rB
  });
}

void potion_arm_rem(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  ARM_MATH({
    ARM(31, REG_TMP, REG(op.a), REG(op.b) << 3 | 0x3, 0xD6); // divw rD,rA,rB
    ARM(31, REG_TMP, REG_TMP, REG(op.b) << 3 | 0x1, 0xD6); // mullw rD,rD,rB
    ARM(31, REG(op.a), REG_TMP, REG(op.a) << 3, 0x50); // subf rA,rD,rA
  });
}

// TODO: need to keep rB in the saved registers, it gets clobbered.
void potion_arm_pow(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  ARM_MATH({
    ARM_MOV(REG_TMP, REG(op.a)); // mov rD,rB
    ARM(14, REG(op.b), REG(op.b), 0xFF, 0xFF); // addi rD,rD,-1
    ARM(11, 7 << 2, REG(op.b), 0, 0); // cmpwi cr7,rD,0x0
    ASMI(0x409D000C); // ble cr7,-12
    ARM(31, REG(op.a), REG(op.a), REG_TMP << 3 | 0x1, 0xD6); // mullw rA,rA,rA
    ASMI(0x4BFFFFF0); // b -16
  });
}

void potion_arm_neq(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  ARM_CMP(0x419E0000); // beq
}

void potion_arm_eq(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  ARM_CMP(0x409E0000); // bne
}

void potion_arm_lt(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  ARM_CMP(0x409C0000); // bge
}

void potion_arm_lte(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  ARM_CMP(0x419D0000); // bgt
}

void potion_arm_gt(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  ARM_CMP(0x409D0000); // ble
}

void potion_arm_gte(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  ARM_CMP(0x419C0000); // blt
}

void potion_arm_bitn(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_arm_bitl(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  ARM_MATH({
    ARM(31, REG(op.a), REG(op.a), REG(op.b) << 3, 0x30); // slw rA,rA,rB
  });
}

void potion_arm_bitr(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  ARM_MATH({
    ARM(31, REG(op.a), REG(op.a), REG(op.b) << 3 | 0x6, 0x30); // sraw rA,rA,rB
  });
}

void potion_arm_def(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_arm_bind(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_arm_message(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_arm_jmp(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, PN_OP *start, PNJumps *jmps, size_t *offs, int *jmpc) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  TAG_JMP(0x48000000, pos + op.a); // b
}

void potion_arm_test_asm(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, int test) {
}

void potion_arm_test(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  potion_arm_test_asm(P, f, asmp, pos, 1);
}

void potion_arm_not(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  potion_arm_test_asm(P, f, asmp, pos, 0);
}

void potion_arm_cmp(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
}

void potion_arm_testjmp(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, PN_OP *start, PNJumps *jmps, size_t *offs, int *jmpc) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  ARM(11, 7 << 2, REG(op.a), 0, PN_FALSE); // cmpwi cr7,rA,0x0
  TAG_JMP(0x409E0000, pos + op.b); // bne
}

void potion_arm_notjmp(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, PN_OP *start, PNJumps *jmps, size_t *offs, int *jmpc) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  ARM(11, 7 << 2, REG(op.a), 0, PN_FALSE); // cmpwi cr7,rA,0x0
  TAG_JMP(0x419E0000, pos + op.b); // beq
}

void potion_arm_named(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_arm_call(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_arm_callset(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_arm_tailcall(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_arm_return(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos) {
  PN_OP op = PN_OP_AT(f->asmb, pos);
  ARM_MOV(3, REG(op.a)); // or r3,rA,rA
  ARM3(32, 1, 1, 0); // lwz r1,(r1)
  ARM3(46, 30, 1, 0xFFF8); // lmw r30,-8(r1)
  ASMI(0x4e800020); // blr
}

void potion_arm_method(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_OP **pos, long lregs, long start, long regs) {
}

void potion_arm_class(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp, PN_SIZE pos, long start) {
}

void potion_arm_finish(Potion *P, struct PNProto * volatile f, PNAsm * volatile *asmp) {
}

void potion_arm_mcache(Potion *P, vPN(Vtable) vt, PNAsm * volatile *asmp) {
}

void potion_arm_ivars(Potion *P, PN ivars, PNAsm * volatile *asmp) {
}

MAKE_TARGET(arm);
