/**\file vm-dis.c
  interface to various jit disassembler libs. currently x86 only. usage: -DJ

 - http://udis86.sourceforge.net/ x86 16,32,64 bit
   \code port install udis86 \endcode
 - http://ragestorm.net/distorm/ x86 16,32,64 bit with all intel/amd extensions
   \code apt-get install libdistorm64-dev \endcode
 - http://bastard.sourceforge.net/libdisasm.html 386 32bit only
   \code apt-get install libdisasm-dev \endcode

  written by Reini Urban
*/

#if defined(JIT_DEBUG)
  printf("-- jit --\n");
  printf("; function definition: %p ; %u bytes\n", asmb->ptr, asmb->len);
#  if defined(HAVE_LIBUDIS86) && (POTION_JIT_TARGET == POTION_X86)
  {
    ud_t ud_obj;

    ud_init(&ud_obj);
    ud_set_input_buffer(&ud_obj, asmb->ptr, asmb->len);
    ud_set_mode(&ud_obj, __WORDSIZE == 64 ? 64 : 32);
    ud_set_syntax(&ud_obj, UD_SYN_ATT);

    while (ud_disassemble(&ud_obj)) {
      printf("0x%012lx 0x%lx %-24s \t%s\n", ud_insn_off(&ud_obj)+(unsigned long)asmb->ptr,
	     (long)ud_insn_off(&ud_obj), ud_insn_hex(&ud_obj), ud_insn_asm(&ud_obj));
    }
  }
#  else
#  if defined(HAVE_LIBDISTORM64) && (POTION_JIT_TARGET == POTION_X86)
  {
    #define MAX_INSTRUCTIONS 2048
    #define MAX_TEXT_SIZE (60)
    typedef enum {Decode16Bits = 0, Decode32Bits = 1, Decode64Bits = 2} _DecodeType;
    typedef enum {DECRES_NONE, DECRES_SUCCESS, DECRES_MEMORYERR, DECRES_INPUTERR} _DecodeResult;
    typedef long _OffsetType;
    typedef struct {
      unsigned int pos;
      int8_t p[MAX_TEXT_SIZE];
    } _WString;
    typedef struct {
      _WString mnemonic;
      _WString operands;
      _WString instructionHex;
      unsigned int size;
      _OffsetType offset;
    } _DecodedInst;
    _DecodeResult distorm_decode64(_OffsetType,
			 const unsigned char*,
			 long,
			 int,
			 _DecodedInst*,
			 int,
			 unsigned int*);

    _DecodedInst disassembled[MAX_INSTRUCTIONS];
    unsigned int decodedInstructionsCount = 0;
    _OffsetType offset = 0;
    int i;

    distorm_decode64(offset,
      (const unsigned char*)asmb->ptr,
      asmb->len,
      __WORDSIZE == 64 ? 2 : 1,
      disassembled,
      MAX_INSTRUCTIONS,
      &decodedInstructionsCount);
    for (i = 0; i < decodedInstructionsCount; i++) {
      printf("0x%012lx 0x%04x (%02d) %-24s %s%s%s\r\n",
	     disassembled[i].offset + (unsigned long)asmb->ptr,
	     (unsigned int)disassembled[i].offset,
	     disassembled[i].size,
	     (char*)disassembled[i].instructionHex.p,
	     (char*)disassembled[i].mnemonic.p,
	     disassembled[i].operands.pos != 0 ? " " : "",
	     (char*)disassembled[i].operands.p);
    }
  }
#  else
#  if defined(HAVE_LIBDISASM) && (POTION_JIT_TARGET == POTION_X86)
#    define LINE_SIZE 255
  {
    char line[LINE_SIZE];
    int pos = 0;
    int size = asmb->len;
    int insnsize;            /* size of instruction */
    x86_insn_t insn;         /* one instruction */

    // only stable for 32bit
    x86_init(opt_none, NULL, NULL);
    while ( pos < size ) {
      insnsize = x86_disasm(asmb->ptr, size, 0, pos, &insn);
      if ( insnsize ) {
        int i;
        x86_format_insn(&insn, line, LINE_SIZE, att_syntax);
        printf("0x%x\t", pos);
	for ( i = 0; i < 10; i++ ) {
          if ( i < insn.size ) printf("%02x", insn.bytes[i]);
	  else printf("  ");
        }
        printf("%s\n", line);
        pos += insnsize;
      } else {
        printf("Invalid instruction at 0x%x. size=0x%x\n", pos, size);
        pos++;
      }
    }
    x86_cleanup();
  }
#else
  long ai = 0;
  for (ai = 0; ai < asmb->len; ai++) {
    printf("%x ", asmb->ptr[ai]);
  }
  printf("\n");
#  endif
#  endif
#  endif
#endif
