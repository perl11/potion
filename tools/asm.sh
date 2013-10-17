#!/bin/sh
# usage: tools/asm.sh xx.c
# creates annotated at&t x86 assembler listings for 32 and 64bit in xx.32.lst and xx.64.lst

# example xx.c:
<<EOF
#include "potion.h"
typedef unsigned char u8;
#include "opcodes.h"

main () {
  PN locals[10], reg[10];
  PN_OP op;

  //getlocal
  if (PN_IS_REF(locals[op.b])) {        // if ((potion_type((PN)(locals[op.b])) == (4+0x250000))) {
    reg[op.a] = PN_DEREF(locals[op.b]); //   reg[op.a] = ((struct PNWeakRef *)(locals[op.b]))->data;
  } else {
    reg[op.a] = locals[op.b];
  }
}                                 
EOF

test -f $1 || (echo "usage $0 cfile.c"; exit)
b=`echo $1|sed 's,\.c,,'`

gcc -m64 -Icore -S -fverbose-asm -O3 -finline $1 -o $b.64.s && as -64 -almhnd $b.64.s > $b.64.lst
gcc -m32 -Icore -S -fverbose-asm -O3 -finline $1 -o $b.32.s && as -32 -almhnd $b.32.s > $b.32.lst
