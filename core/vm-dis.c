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
      printf("0x%lx\t%s\n", (long)ud_insn_off(&ud_obj), ud_insn_asm(&ud_obj));
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
#endif
