//
// asm.c
// some assembler functions
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "potion.h"
#include "internal.h"
#include "opcodes.h"
#include "asm.h"

#define ASM_UNIT 4096

PNAsm *potion_asm_new(Potion *P) {
  PNAsm * volatile asmb = PN_ALLOC(PNAsm);
  PN_FLEX_NEW(*asmb, u8, ASM_UNIT);
  return asmb;
}

void potion_asm_put(Potion *P, PNAsm * volatile asmb, PN val, size_t len) {
  u8 *ptr = asmb->ptr + asmb->len;
  PN_FLEX_NEEDS(len, *asmb, u8, ASM_UNIT);

  if (len == sizeof(u8))
    *ptr = (u8)val;
  else if (len == sizeof(int))
    *((int *)ptr) = (int)val;
  else if (len == sizeof(PN))
    *((PN *)ptr) = val;
}
