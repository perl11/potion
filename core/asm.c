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

PNAsm *potion_asm_new() {
  PNAsm *asmb = PN_ALLOC(PNAsm);
  asmb->start = asmb->ptr = PN_ALLOC_N(u8, (asmb->capa = ASM_UNIT));
  return asmb;
}

void potion_asm_put(PNAsm *asmb, PN val, size_t len) {
  if (asmb->capa - (asmb->ptr - asmb->start) < len) {
    size_t dist = asmb->ptr - asmb->start;
    asmb->capa += ASM_UNIT;
    asmb->start = PN_REALLOC_N(asmb->start, u8, asmb->capa);
    asmb->ptr = asmb->start + dist;
  }

  if (len == sizeof(u8))
    *asmb->ptr = (u8)val;
  else if (len == sizeof(int))
    *((int *)asmb->ptr) = (int)val;
  else if (len == sizeof(PN))
    *((PN *)asmb->ptr) = val;
  asmb->ptr += len;
}

