//
// p2.c
// perl5 on potion
//
// (c) 2008 why the lucky stiff, the freelance professor
// (c) 2013 by perl11 org
//
#include <stdio.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "p2.h"
#include "internal.h"
#include "opcodes.h"
#include "khash.h"
#include "table.h"

#ifdef POTION_JIT_TARGET
#define P2_BANNER(jit) "p2 " P2_VERSION \
  " (date='" POTION_DATE "', commit='" POTION_COMMIT \
  "', platform='" POTION_PLATFORM \
  "', jit=%d/" jit \
  ")\n"
#else
#define P2_BANNER(nojit) "p2 " P2_VERSION \
  " (date='" POTION_DATE "', commit='" POTION_COMMIT \
  "', platform='" POTION_PLATFORM \
  "', jit=%d" \
  ")\n"
#endif

const char p2_banner[]  = P2_BANNER(_XSTR(POTION_JIT_NAME));
const char p2_version[] = P2_VERSION;

// POTION_JIT 1/0 if jit is default
// POTION_JIT_TARGET defined if jittable
static void p2_cmd_usage(Potion *P) {
  printf("usage: p2 [options] [script] [arguments]\n"
#ifdef POTION_JIT_TARGET
      "  -B, --bytecode     run with bytecode VM (slower, but cross-platform)"
#if !POTION_JIT
	 " (default)\n"
#else
	 "\n"
#endif
      "  -J, --jit          run with JIT VM (faster, only x86, x86-64, ppc)"
#if POTION_JIT
	 " (default)\n"
#else
	 "\n"
#endif
#endif
      "  -Idirectory        add library search path\n"
      "  -c                 check script and exit\n" // compile-time checks only
//    "  -d                 run program under the debugger\n" // TODO: we want to debug over sockets, instrument bytecode and use p2d frontend
#ifdef DEBUG
      "  -D[itpPvGJ]        debugging flags, try -D?\n"
#endif
      "  -e code            execute code\n"
      "  -E code            execute code with extended features enabled\n"
      "  -V, --verbose      print bytecode and ast info\n"
      "  -h, --help         print this usage info and exit\n"
      "  -v, --version      print version, patchlevel, features and exit\n"
      "  --inspect          print the return value\n"
      "  --stats            print statistics and exit\n"
// Traditionally compilers are "compile" methods of O packages (B::C, jvm, exec)
// But B pollutes our namespace, so prefer native methods to avoid B/DynaLoader deps being pulled in
      "  --compile          compile the script to bytecode and exit\n" // ie native B::ByteCode
//    "  --compile-c        compile the script to C and exit\n" // TODO: or use p2c?
#if POTION_JIT
//    "  --compile-exec     compile the script to native executable and exit\n" // TODO: 2st via C, then direct
#endif
  );
}

static void p2_cmd_stats(Potion *P) {
  printf("sizeof(PN=%d, PNObject=%d, PNTuple=%d, PNTuple+1=%d, PNTable=%d)\n",
      (int)sizeof(PN), (int)sizeof(struct PNObject), (int)sizeof(struct PNTuple),
      (int)(sizeof(PN) + sizeof(struct PNTuple)), (int)sizeof(struct PNTable));
  printf("GC (fixed=%ld, actual=%ld, reserved=%ld)\n",
      PN_INT(potion_gc_fixed(P, 0, 0)), PN_INT(potion_gc_actual(P, 0, 0)),
      PN_INT(potion_gc_reserved(P, 0, 0)));
}

static void p2_cmd_version(Potion *P) {
  printf(p2_banner, POTION_JIT);
}

static PN p2_cmd_exec(Potion *P, PN buf, char *filename, exec_mode_t exec) {
  PN code = p2_source_load(P, PN_NIL, buf);
  if (PN_IS_PROTO(code)) {
    if (P->flags & DEBUG_VERBOSE)
      printf("\n\n-- loaded --\n");
  } else {
    code = p2_parse(P, buf);
    if (!code || PN_TYPE(code) == PN_TERROR) {
      potion_p(P, code);
      return code;
    }
    if (P->flags & DEBUG_VERBOSE) {
      printf("\n-- parsed --\n");
      potion_p(P, code);
    }
    code = potion_send(code, PN_compile, potion_str(P, filename), PN_NIL);
    if (P->flags & DEBUG_VERBOSE)
      printf("\n-- compiled --\n");
  }
  if (P->flags & DEBUG_VERBOSE)
    potion_p(P, code);
  if (exec == EXEC_VM) {
    code = potion_vm(P, code, P->lobby, PN_NIL, 0, NULL);
    if (P->flags & DEBUG_VERBOSE)
      printf("\n-- vm returned %p (fixed=%ld, actual=%ld, reserved=%ld) --\n", (void *)code,
	     PN_INT(potion_gc_fixed(P, 0, 0)), PN_INT(potion_gc_actual(P, 0, 0)),
	     PN_INT(potion_gc_reserved(P, 0, 0)));
    if (P->flags & (DEBUG_INSPECT|DEBUG_VERBOSE))
      potion_p(P, code);
  } else if (exec == EXEC_JIT) {
#ifdef POTION_JIT_TARGET
    PN val;
    PN cl = potion_closure_new(P, (PN_F)potion_jit_proto(P, code), PN_NIL, 1);
    PN_CLOSURE(cl)->data[0] = code;
    val = PN_PROTO(code)->jit(P, cl, P->lobby);
    if (P->flags & DEBUG_VERBOSE)
      printf("\n-- jit returned %p (fixed=%ld, actual=%ld, reserved=%ld) --\n", PN_PROTO(code)->jit,
	     PN_INT(potion_gc_fixed(P, 0, 0)), PN_INT(potion_gc_actual(P, 0, 0)),
	     PN_INT(potion_gc_reserved(P, 0, 0)));
    if (P->flags & (DEBUG_INSPECT|DEBUG_VERBOSE))
      potion_p(P, val);
#else
    fprintf(stderr, "** p2 built without JIT support\n");
#endif
  }
  else if (exec == EXEC_CHECK) {
  }
  return code;
}

static void p2_cmd_compile(Potion *P, char *filename, exec_mode_t exec) {
  PN buf;
  int fd = -1;
  struct stat stats;

  if (stat(filename, &stats) == -1) {
    fprintf(stderr, "** %s does not exist.", filename);
    goto done;
  }
  fd = open(filename, O_RDONLY | O_BINARY);
  if (fd == -1) {
    fprintf(stderr, "** could not open %s. check permissions.", filename);
    goto done;
  }

  buf = potion_bytes(P, stats.st_size);
  // TODO: mmap instead of read all
  if (read(fd, PN_STR_PTR(buf), stats.st_size) == stats.st_size) {
    PN code;
    PN_STR_PTR(buf)[stats.st_size] = '\0';

    code = p2_cmd_exec(P, buf, filename, exec);
    if (!code || PN_TYPE(code) == PN_TERROR)
      goto done;

    if (exec >= EXEC_COMPILE) { // needs an inputfile. TODO: -e"" -ofile
      char plcpath[255];
      FILE *plc;
      if (exec == EXEC_COMPILE)
	sprintf(plcpath, "%sc", filename);  // .plc and .pmc
      else if (exec == EXEC_COMPILE_C)
	sprintf(plcpath, "%s.c", filename); // .pl.c and .pm.c
      else if (exec == EXEC_COMPILE_NATIVE) {
	sprintf(plcpath, "%s.out", filename);   // TODO: strip ext
      }
      plc = fopen(plcpath, "wb");
      if (!plc) {
	fprintf(stderr, "** could not open %s for writing. check permissions.", plcpath);
	goto done;
      }

      if (exec == EXEC_COMPILE)
	//TODO: use source dumpbc,dumpc,dumpbin,dump (ascii) methods
	//code = potion_send(P, code, "dumpbc");
	code = potion_source_dumpbc(P, PN_NIL, code);
      else if (exec == EXEC_COMPILE_C) {
	//code = potion_send(P, code, "dumpc");
	potion_fatal("--compile-c not yet implemented\n");
      }
      else if (exec == EXEC_COMPILE_NATIVE) {
	//code = potion_send(P, code, "dumpbin");
	potion_fatal("--compile-native not yet implemented\n");
      }
      if (fwrite(PN_STR_PTR(code), 1, PN_STR_LEN(code), plc) == PN_STR_LEN(code)) {
	printf("** compiled code saved to %s\n", plcpath);
        fclose(plc);
      } else {
        fprintf(stderr, "** could not write all compiled code.");
      }
      if (exec == EXEC_COMPILE)
        printf("** run it with: p2 %s\n", plcpath);
    }

#if defined(DEBUG)
    if (P->flags & DEBUG_GC) { // GC sanity check
      printf("\n-- gc check --\n");
      void *scanptr = (void *)((char *)P->mem->old_lo + (sizeof(PN) * 2));
      while ((PN)scanptr < (PN)P->mem->old_cur) {
	printf("%p.vt = %x (%u)\n",
	       scanptr, ((struct PNObject *)scanptr)->vt,
	       potion_type_size(P, scanptr));
	if (((struct PNFwd *)scanptr)->fwd != POTION_FWD
	    && ((struct PNFwd *)scanptr)->fwd != POTION_COPIED) {
	  if ((signed long)(((struct PNObject *)scanptr)->vt) < 0
	      || ((struct PNObject *)scanptr)->vt > PN_TUSER) {
	    printf("wrong type for allocated object: %p.vt = %x\n",
		   scanptr, ((struct PNObject *)scanptr)->vt);
	    break;
	  }
	}
	scanptr = (void *)((char *)scanptr + potion_type_size(P, scanptr));
	if ((PN)scanptr > (PN)P->mem->old_cur) {
	  printf("allocated object goes beyond GC pointer\n");
	  break;
	}
      }
    }
#endif

  } else {
    fprintf(stderr, "** could not read entire file.");
  }

done:
  if (fd != -1)
    close(fd);
}

int main(int argc, char *argv[]) {
  POTION_INIT_STACK(sp);
  int i, verbose = 0, exec = POTION_JIT ? EXEC_JIT : EXEC_VM, interactive = 1;
  Potion *P = potion_create(sp);
  PN buf = PN_NIL;

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--") == 0) {
      break;
    }
    if (strcmp(argv[i], "-I") == 0) {
      char *extra_path = &argv[i][2]; // todo: flexible
      if (*extra_path)
	potion_loader_add(P, potion_str(P, extra_path));
      else {
	if (i == argc) {
	    fprintf(stderr, "-I missing directory\n");
	    goto END;
	}
	potion_loader_add(P, potion_str(P, argv[i+1]));
	i++;
      }
      continue;
    }
    if (strcmp(argv[i], "--inspect") == 0) {
      verbose = 1;
      P->flags |= DEBUG_INSPECT;
      continue;
    }
    if (strcmp(argv[i], "-V") == 0 ||
        strcmp(argv[i], "--verbose") == 0) {
      P->flags |= DEBUG_VERBOSE;
      verbose = 2;
      continue;
    }
#ifdef DEBUG
    if (argv[i][0] == '-' && argv[i][1] == 'D') {
      if (strchr(&argv[i][2], '?')) {
	printf("-D debugging flags:\n");
	printf("  i  inspect\n");
	printf("  v  verbose\n");
	printf("  t  trace\n");
	printf("  p  parse\n");
	printf("  P  parse_verbose\n");
	printf("  J  Jit\n");
	printf("  G  GC\n");
	goto END;
      }
      if (strchr(&argv[i][2], 'i'))
	P->flags |= DEBUG_INSPECT;
      if (strchr(&argv[i][2], 'v'))
	P->flags |= DEBUG_VERBOSE;
      if (strchr(&argv[i][2], 't')) {
	P->flags |= DEBUG_TRACE;
	verbose = 2;
      }
      if (strchr(&argv[i][2], 'p'))
	P->flags |= DEBUG_PARSE;
      if (strchr(&argv[i][2], 'P'))
	P->flags |= DEBUG_PARSE_VERBOSE;
      if (strchr(&argv[i][2], 'J'))
	P->flags |= DEBUG_JIT;
      if (strchr(&argv[i][2], 'G'))
	P->flags |= DEBUG_GC;
      continue;
    }
#endif
    if (strcmp(argv[i], "-v") == 0 ||
        strcmp(argv[i], "--version") == 0) {
      p2_cmd_version(P);
      goto END;
    }
    if (strcmp(argv[i], "-h") == 0 ||
        strcmp(argv[i], "--help") == 0) {
      p2_cmd_usage(P);
      goto END;
    }
    if (strcmp(argv[i], "--stats") == 0) {
      p2_cmd_stats(P); // TODO: afterwards
      goto END;
    }
    if (strcmp(argv[i], "--compile") == 0) {
      exec = EXEC_COMPILE;
      continue;
    }
    if (strcmp(argv[i], "-c") == 0) {
      exec = EXEC_CHECK;
      continue;
    }
    if (strcmp(argv[i], "-B") == 0 ||
        strcmp(argv[i], "--bytecode") == 0) {
      exec = EXEC_VM;
      continue;
    }
    if (strcmp(argv[i], "-J") == 0 ||
        strcmp(argv[i], "--jit") == 0) {
      exec = EXEC_JIT;
      continue;
    }
    if (argv[i][0] == '-' &&
	(argv[i][1] == 'e' || argv[i][1] == 'E')) {
      char *arg;
      interactive = 0;
      if (strlen(argv[i]) == 2) {
	arg = argv[i+1];
      } else if (i <= argc) {
	arg = argv[i]+2;
      } else { // or go into interactive mode as -de0?
	potion_fatal("** Missing argument for -e");
	goto END;
      }
      if (argv[i][1] == 'E') {
	potion_define_global(P, potion_str(P, "$0"), potion_str(P, "-E"));
	buf = potion_str(P, "use p2;\n");
	buf = potion_bytes_append(P, 0, buf, potion_str(P, arg));
      } else {
	potion_define_global(P, potion_str(P, "$0"), potion_str(P, "-e"));
	buf = potion_str(P, arg);
      }
      continue;
    }
    if (i == argc - 1) {
      interactive = 0;
      continue;
    }
    fprintf(stderr, "** Unrecognized option: %s\n", argv[i]);
  }

  potion_define_global(P, potion_str(P, "$P2::execmode"), PN_INT(exec));
  potion_define_global(P, potion_str(P, "$P2::verbose"),
		       verbose ? PN_INT(verbose) : PN_NIL);
  potion_define_global(P, potion_str(P, "$P2::interactive"),
		       interactive ? PN_INT(interactive) : PN_NIL);
  potion_define_global(P, potion_str(P, "$^X"), potion_str(P, argv[0]));

  if (!interactive) {
    if (buf != PN_NIL) {
      PN code = p2_cmd_exec(P, buf, "-e", exec);
      if (!code || PN_TYPE(code) == PN_TERROR)
	goto END;
    }
    else {
      potion_define_global(P, potion_str(P, "$0"), potion_str(P, argv[argc-1]));
      p2_cmd_compile(P, argv[argc-1], exec);
    }
  } else {
    if (!exec || verbose) potion_fatal("no filename given");
    // TODO: p5 not yet parsed
    p2_eval(P, potion_byte_str(P,
      "p2::load 'readline';" \
      "while ($code = readline('>> ')) {" \
      "  $obj = eval $code;" \
      "  if ($@) {" \
      "    say $@;" \
      "  } else {" \
      "    say '=> ', $obj;" \
      "  }"
      "}"), exec == EXEC_JIT ? 1 : 0);
  }
END:
#if !defined(POTION_JIT_TARGET) || defined(DEBUG)
  if (P != NULL)
    potion_destroy(P);
#endif
  return 0;
}
