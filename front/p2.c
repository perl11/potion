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
      "  -d                 run script under the debugger\n"
#ifdef DEBUG
      "  -D[itvpPcGJ]       debugging flags, try -D?\n"
#endif
      "  -e code            execute code\n"
      "  -E code            execute code with extended features enabled\n"
      "  -V, --verbose      print bytecode and ast info\n"
      "  -h, --help         print this usage info and exit\n"
      "  -v, --version      print version, patchlevel, features and exit\n"
      "  --inspect          print the return value\n"
      "  --stats            print statistics and exit\n"
      "  --compile          compile the script to bytecode\n"
      "  --compile={c,exe}  load compiler backend and compile: C or exe\n" // or jvm, .net
  );
}

static void p2_cmd_stats(Potion *P) {
  printf("sizeof(PN=%d, PNObject=%d, PNTuple=%d, PNTable=%d)\n",
      (int)sizeof(PN), (int)sizeof(struct PNObject), (int)sizeof(struct PNTuple),
      (int)sizeof(struct PNTable));
  printf("GC (fixed=%ld, actual=%ld, reserved=%ld)\n",
      PN_INT(potion_gc_fixed(P, 0, 0)), PN_INT(potion_gc_actual(P, 0, 0)),
      PN_INT(potion_gc_reserved(P, 0, 0)));
}

static void p2_cmd_version(Potion *P) {
  printf(p2_banner, POTION_JIT);
}

#define DBG_Pv(p) \
    if (P->flags & DEBUG_VERBOSE) \
      potion_p(P, p)
#define DBG_Pvi(p) \
    if (P->flags & (DEBUG_INSPECT|DEBUG_VERBOSE)) \
      potion_p(P, p)

static PN p2_cmd_exec(Potion *P, PN buf, char *filename, char *compile) {
  PN code = p2_source_load(P, PN_NIL, buf);
  exec_mode_t exec = P->flags & ((1<<EXEC_BITS)-1);
  if (PN_IS_PROTO(code)) {
    DBG_v("\n-- loaded --\n");
  } else {
    code = p2_parse(P, buf, filename);
    if (!code || PN_TYPE(code) == PN_TERROR) {
      potion_p(P, code);
      return code;
    }
    DBG_v("\n-- parsed --\n");
    DBG_Pv(code);
    code = potion_send(code, PN_compile, potion_str(P, filename), PN_NIL);
    DBG_v("\n-- compiled --\n");
  }
  DBG_Pv(code);
  if (exec == EXEC_VM) {
    code = potion_vm(P, code, P->lobby, PN_NIL, 0, NULL);
    DBG_v("\n-- vm returned %p (fixed=%ld, actual=%ld, reserved=%ld) --\n", (void *)code,
	  PN_INT(potion_gc_fixed(P, 0, 0)), PN_INT(potion_gc_actual(P, 0, 0)),
	  PN_INT(potion_gc_reserved(P, 0, 0)));
    DBG_Pvi(code);
  } else if (exec == EXEC_JIT) {
#ifdef POTION_JIT_TARGET
    PN val;
    PN cl = potion_closure_new(P, (PN_F)potion_jit_proto(P, code), PN_NIL, 1);
    PN_CLOSURE(cl)->data[0] = code;
    val = PN_PROTO(code)->jit(P, cl, P->lobby);
    DBG_v("\n-- jit returned %p (fixed=%ld, actual=%ld, reserved=%ld) --\n", PN_PROTO(code)->jit,
	  PN_INT(potion_gc_fixed(P, 0, 0)), PN_INT(potion_gc_actual(P, 0, 0)),
	  PN_INT(potion_gc_reserved(P, 0, 0)));
    DBG_Pvi(val);
#else
    fprintf(stderr, "** p2 built without JIT support\n");
#endif
  }
  else if (exec == EXEC_CHECK) {
  }
  return code;
}

static void p2_cmd_compile(Potion *P, char *filename, char *compile) {
  PN buf;
  int fd = -1;
  struct stat stats;
  exec_mode_t exec = P->flags & ((1<<EXEC_BITS)-1);

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

    code = p2_cmd_exec(P, buf, filename, compile);
    if (!code || PN_TYPE(code) == PN_TERROR)
      goto done;

    if (exec >= EXEC_COMPILE) { // needs an inputfile. TODO: -e"" -ofile
      char plcpath[255];
      FILE *plc;
      char *opts = NULL;
      if (compile) { // --compile=c[,OPTS]
	if ((opts = strchr(compile,','))) {
	  opts[0] = '\0';
	  opts++;
	}
      }
      if (exec == EXEC_COMPILE) {
        if (!compile || !strcmp(compile, "bc"))
	  sprintf(plcpath, "%sc", filename);  // .plc
        else if (!strcmp(compile, "c"))
	  sprintf(plcpath, "%s.c", filename); // .pl.c
        else if (!strcmp(compile, "exe"))
	  sprintf(plcpath, "%s.out", filename); // TODO: strip ext
      }

      plc = fopen(plcpath, "wb");
      if (!plc) {
	fprintf(stderr, "** could not open %s for writing. check permissions.", plcpath);
	goto done;
      }

      if (exec == EXEC_COMPILE) { // compile backend. default: bc
        if (!compile)
	  code = potion_source_dumpbc(P, PN_NIL, code, PN_NIL);
        else
	  code = potion_send(code, potion_str(P, "dump"),
			     potion_str(P, compile), opts ? potion_str(P, opts) : PN_NIL);
      }

      if (code && fwrite(PN_STR_PTR(code), 1, PN_STR_LEN(code), plc) == PN_STR_LEN(code)) {
	printf("** compiled code saved to %s\n", plcpath);
        fclose(plc);

	if (exec == EXEC_COMPILE) {
	  if (!compile || !strcmp(compile, "bc"))
	    printf("** run it with: p2 %s\n", plcpath);
	  else if (!strcmp(compile, "c"))
	    printf("** compile it with: %s %s %s\n", POTION_CC, POTION_CFLAGS, plcpath);
	  else if (!strcmp(compile, "exe"))
	    printf("** run it with: ./%s\n", plcpath);
	}
      } else {
        fprintf(stderr, "** could not write all %s compiled code.\n", compile ? compile : "bytecode");
      }
    }

#if defined(DEBUG)
    if (P->flags & DEBUG_GC) { // GC sanity check
      fprintf(stderr, "\n-- gc check --\n");
      void *scanptr = (void *)((char *)P->mem->old_lo + (sizeof(PN) * 2));
      while ((PN)scanptr < (PN)P->mem->old_cur) {
	fprintf(stderr, "%p.vt = %x (%u)\n",
		scanptr, ((struct PNObject *)scanptr)->vt,
		potion_type_size(P, scanptr));
	if (((struct PNFwd *)scanptr)->fwd != POTION_FWD
	    && ((struct PNFwd *)scanptr)->fwd != POTION_COPIED) {
	  PNType vt = ((struct PNObject *)scanptr)->vt;
	  if ((signed int)vt < 0 || vt > PN_TUSER) {
	    fprintf(stderr, "** wrong type for allocated object: %p.vt = %x\n",
		    scanptr, vt);
	    break;
	  }
	}
	scanptr = (void *)((char *)scanptr + potion_type_size(P, scanptr));
	if ((PN)scanptr > (PN)P->mem->old_cur) {
	  fprintf(stderr, "** allocated object goes beyond GC pointer\n");
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
  int i, exec = POTION_JIT ? EXEC_JIT : EXEC_VM, interactive = 1;
  Potion *P = potion_create(sp);
  PN buf = PN_NIL;
  char *compile = NULL;

  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--"))
      break;
    if (!strcmp(argv[i], "-I")) {
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
    if (!strcmp(argv[i], "--inspect")) {
      P->flags |= DEBUG_INSPECT;
      continue;
    }
    if (!strcmp(argv[i], "-V") || !strcmp(argv[i], "--verbose")) {
      P->flags |= DEBUG_VERBOSE;
      continue;
    }
#ifdef DEBUG
    if (argv[i][0] == '-' && argv[i][1] == 'D') {
      if (strchr(&argv[i][2], '?')) {
	printf("-D debugging flags:\n");
	printf("  i  inspect\n");
	printf("  v  verbose\n");
	printf("  t  trace\n");
	printf("  c  compile\n");
	printf("  p  parse\n");
	printf("  P  parse verbose\n");
	printf("  J  Jit\n");
	printf("  G  GC\n");
	goto END;
      }
      if (strchr(&argv[i][2], 'i')) P->flags |= DEBUG_INSPECT;
      if (strchr(&argv[i][2], 'v')) P->flags |= DEBUG_VERBOSE;
      if (strchr(&argv[i][2], 't')) { P->flags |= DEBUG_TRACE;
	exec = exec==EXEC_JIT ? EXEC_VM : exec; }
      if (strchr(&argv[i][2], 'p')) P->flags |= DEBUG_PARSE;
      if (strchr(&argv[i][2], 'P')) P->flags |= (DEBUG_PARSE | DEBUG_PARSE_VERBOSE);
      if (strchr(&argv[i][2], 'c')) P->flags |= DEBUG_COMPILE;
      if (strchr(&argv[i][2], 'J')) P->flags |= DEBUG_JIT;
      if (strchr(&argv[i][2], 'G')) P->flags |= DEBUG_GC;
      continue;
    }
#endif
    if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--version")) {
      p2_cmd_version(P);
      goto END;
    }
    if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
      p2_cmd_usage(P);
      goto END;
    }
    if (!strcmp(argv[i], "--stats")) {
      p2_cmd_stats(P); // TODO: afterwards
      goto END;
    }
    if (!strcmp(argv[i], "--compile")) {
      exec = EXEC_COMPILE;
      continue;
    }
    if (!strcmp(argv[i], "-c")) {
      exec = EXEC_CHECK;
      continue;
    }
    if (!strcmp(argv[i], "-B") || !strcmp(argv[i], "--bytecode")) {
      exec = EXEC_VM;
      continue;
    }
    if (!strcmp(argv[i], "-J") || !strcmp(argv[i], "--jit")) {
      exec = EXEC_JIT;
      continue;
    }
    if (argv[i][0] == '-' && (argv[i][1] == 'e' || argv[i][1] == 'E')) {
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
	buf = potion_str(P, arg);
	P->flags = (P->flags & 0xff00) + MODE_P2;
	//buf = potion_str(P, "use p2;\n");
	//buf = potion_bytes_append(P, 0, buf, potion_str(P, arg));
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
  P->flags += exec;

  potion_define_global(P, potion_str(P, "$P2::mode"), PN_NUM((P->flags & 0xff))); // first flags word: p5,p2,p6
  potion_define_global(P, potion_str(P, "$P2::execmode"), PN_NUM(exec)); // exec_jit, exec_vm
  potion_define_global(P, potion_str(P, "$P2::verbose"),  PN_NUM((P->flags & ~0xff) >> 8)); // second flags word
  potion_define_global(P, potion_str(P, "$P2::interactive"),
		       interactive ? PN_NUM(interactive) : PN_NIL);
  potion_define_global(P, potion_str(P, "$^X"), potion_str(P, argv[0]));

  if (!interactive) {
    if (buf != PN_NIL) {
      potion_define_global(P, potion_str(P, "$0"), potion_str(P, "-e"));
      p2_cmd_exec(P, buf, "-e", compile);
    }
    else {
      potion_define_global(P, potion_str(P, "$0"), potion_str(P, argv[argc-1]));
      p2_cmd_compile(P, argv[argc-1], compile);
    }
  } else {
    if (!exec || P->flags & DEBUG_INSPECT) potion_fatal("no filename given");
    potion_define_global(P, potion_str(P, "$0"), potion_str(P, "-i"));
    // TODO: p5 not yet parsed
    p2_eval(P, potion_byte_str(P,
      "load 'readline';\n" \
      "while ($code = readline('>> ')) {\n" \
      "  $obj = eval $code;\n" \
      "  if ($@) {\n" \
      "    say $@;\n" \
      "  } else {\n" \
      "    say '=> ', $obj;\n" \
      "  }\n"
      "}"));
  }
END:
#if !defined(POTION_JIT_TARGET) || defined(DEBUG)
  if (P != NULL)
    potion_destroy(P);
#endif
  return 0;
}
