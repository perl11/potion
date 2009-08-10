//
// asm.c
// some assembler functions
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

PNAsm *potion_asm_new(Potion *P) {
  int siz = ASM_UNIT - sizeof(PNAsm);
  PNAsm * volatile asmb = PN_FLEX_NEW(asmb, PN_TBYTES, PNAsm, siz);
  return asmb;
}

PNAsm *potion_asm_clear(Potion *P, PNAsm * volatile asmb) {
  asmb->len = 0;
  PN_MEMZERO_N(asmb->ptr, u8, asmb->siz);
  return asmb;
}

PNAsm *potion_asm_put(Potion *P, PNAsm * volatile asmb, PN val, size_t len) {
  u8 *ptr;
  PN_FLEX_NEEDS(len, asmb, PN_TBYTES, PNAsm, ASM_UNIT);
  ptr = asmb->ptr + asmb->len;

  if (len == sizeof(u8))
    *ptr = (u8)val;
  else if (len == sizeof(int))
    *((int *)ptr) = (int)val;
  else if (len == sizeof(PN))
    *((PN *)ptr) = val;

  asmb->len += len;
  return asmb;
}

PNAsm *potion_asm_op(Potion *P, PNAsm * volatile asmb, u8 ins, int _a, int _b) {
  PN_OP *pos;
  PN_FLEX_NEEDS(sizeof(PN_OP), asmb, PN_TBYTES, PNAsm, ASM_UNIT);
  pos = (PN_OP *)(asmb->ptr + asmb->len);

  pos->code = ins;
  pos->a    = _a;
  pos->b    = _b;

  asmb->len += sizeof(PN_OP);
  return asmb;
}

PNAsm *potion_asm_write(Potion *P, PNAsm * volatile asmb, char *str, size_t len) {
  char *ptr;
  PN_FLEX_NEEDS(len, asmb, PN_TBYTES, PNAsm, ASM_UNIT);
  ptr = (char *)asmb->ptr + asmb->len;
  PN_MEMCPY_N(ptr, str, char, len);
  asmb->len += len;
  return asmb;
}
