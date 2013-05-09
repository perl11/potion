//
// potion.c
// the Potion!
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "potion.h"
#include "internal.h"
#include "opcodes.h"
#include "khash.h"
#include "table.h"

const char potion_banner[] = "potion " POTION_VERSION
                             " (date='" POTION_DATE "', commit='" POTION_COMMIT
                             "', platform='" POTION_PLATFORM "', jit=%d)\n";
const char potion_version[] = POTION_VERSION;

static void potion_cmd_usage(Potion *P) {
  printf("usage: potion [options] [script] [arguments]\n"
      "  -e  code           execute code\n"
      "  -B, --bytecode     run with bytecode VM (slower, but cross-platform)\n"
      "  -X, --x86          run with x86 JIT VM (faster, x86 and x86-64)\n"
      "  -Ldirectory        add library search path\n"
      "  -I, --inspect      print only the return value\n"
      "  -V, --verbose      show bytecode and ast info\n"
      "  -c, --compile      compile the script to bytecode\n"
      "      --check        check the script syntax and exit\n"
      "  -h, --help         show this helpful stuff\n"
#ifdef DEBUG
      "  -D[vitPpcGJ?]      debugging flags, try -D?\n"
#endif
      "  -v, --version      show version\n"
      "(default: %s)\n",
#if POTION_JIT == 1
      "-X"
#else
      "-B"
#endif
  );
}

static void potion_cmd_stats(Potion *P) {
  printf("sizeof(PN=%d, PNObject=%d, PNTuple=%d, PNTuple+1=%d, PNTable=%d)\n",
      (int)sizeof(PN), (int)sizeof(struct PNObject), (int)sizeof(struct PNTuple),
      (int)(sizeof(PN) + sizeof(struct PNTuple)), (int)sizeof(struct PNTable));
  printf("GC (fixed=%ld, actual=%ld, reserved=%ld)\n",
      PN_INT(potion_gc_fixed(P, 0, 0)), PN_INT(potion_gc_actual(P, 0, 0)),
      PN_INT(potion_gc_reserved(P, 0, 0)));
}

static void potion_cmd_version(Potion *P) {
  printf(potion_banner, POTION_JIT);
}

#define dbg_v(...) \
    if (P->flags & DEBUG_VERBOSE) \
      fprintf(stderr, __VA_ARGS__)
#define dbgP_v(c) \
    if (P->flags & DEBUG_VERBOSE) \
      potion_p(P, c)
#define dbgP_vi(c) \
    if (P->flags & (DEBUG_INSPECT|DEBUG_VERBOSE)) \
      potion_p(P, c)

static PN potion_cmd_exec(Potion *P, PN buf, char *filename, exec_mode_t exec) {
  PN code = potion_source_load(P, PN_NIL, buf);
  if (PN_IS_PROTO(code)) {
    dbg_v("\n-- loaded --\n");
  } else {
    code = potion_parse(P, buf, filename);
    if (!code || PN_TYPE(code) == PN_TERROR) {
      potion_p(P, code);
      return code;
    }
    dbg_v("\n-- parsed --\n");
    dbgP_v(code);
    code = potion_send(code, PN_compile, potion_str(P, filename), PN_NIL);
    dbg_v("\n-- compiled --\n");
  }
  dbgP_v(code);
  if (exec == EXEC_VM) {
    code = potion_vm(P, code, P->lobby, PN_NIL, 0, NULL);
    dbg_v("\n-- vm returned %p (fixed=%ld, actual=%ld, reserved=%ld) --\n", (void *)code,
	  PN_INT(potion_gc_fixed(P, 0, 0)), PN_INT(potion_gc_actual(P, 0, 0)),
	  PN_INT(potion_gc_reserved(P, 0, 0)));
    dbgP_vi(code);
  } else if (exec == EXEC_JIT) {
#ifdef POTION_JIT_TARGET
    PN val;
    if (code) {
      PN cl = potion_closure_new(P, (PN_F)potion_jit_proto(P, code), PN_NIL, 1);
      PN_CLOSURE(cl)->data[0] = code;
      val = PN_PROTO(code)->jit(P, cl, P->lobby);
      dbg_v("\n-- jit returned %p (fixed=%ld, actual=%ld, reserved=%ld) --\n", PN_PROTO(code)->jit,
            PN_INT(potion_gc_fixed(P, 0, 0)), PN_INT(potion_gc_actual(P, 0, 0)),
            PN_INT(potion_gc_reserved(P, 0, 0)));
      dbgP_vi(val);
    }
#else
    fprintf(stderr, "** potion built without JIT support\n");
#endif
  }
  else if (exec == EXEC_CHECK) {
  }
  return code;
}

static void potion_cmd_compile(Potion *P, char *filename, exec_mode_t exec) {
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

    //code = potion_cmd_exec(P, buf, filename, exec);

    // horrible code duplication hack.
    // fix #18 segv with test/closures/long.pn and wrong result in test/closures/upvals.pn
    // haven't found the wrong stack ptr yet
  code = potion_source_load(P, PN_NIL, buf);
  if (PN_IS_PROTO(code)) {
    dbg_v("\n-- loaded --\n");
  } else {
    code = potion_parse(P, buf, filename);
    if (!code || PN_TYPE(code) == PN_TERROR) {
      potion_p(P, code);
      goto done;
    }
    dbg_v("\n-- parsed --\n");
    dbgP_v(code);
    code = potion_send(code, PN_compile, potion_str(P, filename), PN_NIL);
    dbg_v("\n-- compiled --\n");
  }
  dbgP_v(code);
  if (exec == EXEC_VM) {
    code = potion_vm(P, code, P->lobby, PN_NIL, 0, NULL);
    dbg_v("\n-- vm returned %p (fixed=%ld, actual=%ld, reserved=%ld) --\n", (void *)code,
	  PN_INT(potion_gc_fixed(P, 0, 0)), PN_INT(potion_gc_actual(P, 0, 0)),
	  PN_INT(potion_gc_reserved(P, 0, 0)));
    dbgP_vi(code);
  } else if (exec == EXEC_JIT) {
#ifdef POTION_JIT_TARGET
    if (code) {
      PN val;
      PN cl = potion_closure_new(P, (PN_F)potion_jit_proto(P, code), PN_NIL, 1);
      PN_CLOSURE(cl)->data[0] = code;
      val = PN_PROTO(code)->jit(P, cl, P->lobby);
      dbg_v("\n-- jit returned %p (fixed=%ld, actual=%ld, reserved=%ld) --\n", PN_PROTO(code)->jit,
            PN_INT(potion_gc_fixed(P, 0, 0)), PN_INT(potion_gc_actual(P, 0, 0)),
            PN_INT(potion_gc_reserved(P, 0, 0)));
      dbgP_vi(val);
    }
#else
    fprintf(stderr, "** potion built without JIT support\n");
#endif
  }
  else if (exec == EXEC_CHECK) {
    dbg_v("\n-- check --\n");
  }

    if (!code || PN_TYPE(code) == PN_TERROR)
      goto done;

    if (exec >= MAX_EXEC)
      potion_fatal("fatal: stack overwrite (exec > MAX_EXEC)\n");
    if (exec >= EXEC_COMPILE) { // needs an inputfile. TODO: -e"" -ofile
      char pnbpath[255];
      FILE *pnb;
      if (exec == EXEC_COMPILE)
	sprintf(pnbpath, "%sb", filename);  // .pnb
      else if (exec == EXEC_COMPILE_C)
	sprintf(pnbpath, "%s.c", filename); // .pn.c
      else if (exec == EXEC_COMPILE_NATIVE) {
	sprintf(pnbpath, "%s.out", filename); // TODO: strip ext
      }

      pnb = fopen(pnbpath, "wb");
      if (!pnb) {
        fprintf(stderr, "** could not open %s for writing. check permissions.", pnbpath);
        goto done;
      }

      // TODO: rename dump to compile-
      if (exec == EXEC_COMPILE) { // compile to bytecode
	code = potion_source_dumpbc(P, PN_NIL, code);
      }
      else if (exec == EXEC_COMPILE_C) {
	//code = potion_send(code, "dumpc");
	potion_fatal("--compile-c not yet implemented\n");
      }
      else if (exec == EXEC_COMPILE_NATIVE) {
	//code = potion_send(code, "dumpexec");
	potion_fatal("--compile-native not yet implemented\n");
      }

      if (code && (fwrite(PN_STR_PTR(code), 1, PN_STR_LEN(code), pnb) == PN_STR_LEN(code))) {
        printf("** compiled code saved to %s\n", pnbpath);
        fclose(pnb);

	if (exec == EXEC_COMPILE)
	  printf("** run it with: potion %s\n", pnbpath);
	else if (exec == EXEC_COMPILE_C)
	  printf("** compile it with: %s %s %s\n", POTION_CC, POTION_CFLAGS, pnbpath);
	else if (exec == EXEC_COMPILE_NATIVE)
	  printf("** run it with: ./%s\n", pnbpath);
      } else {
        fprintf(stderr, "** could not write all compiled code.");
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
  int i;
  int interactive = 1;
  exec_mode_t exec = POTION_JIT ? EXEC_JIT : EXEC_VM;
  Potion *P = potion_create(sp);
  PN buf = PN_NIL;

  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--"))
      break;
    if (!strcmp(argv[i], "-I") || !strcmp(argv[i], "--inspect")) {
      P->flags |= DEBUG_INSPECT;
      continue;
    }
    if (!strcmp(argv[i], "-L")) {
      char *extra_path = &argv[i][2]; // todo: flexible
      if (*extra_path)
	potion_loader_add(P, potion_str(P, extra_path));
      else {
	if (i == argc) {
	    fprintf(stderr, "-L missing directory\n");
	    goto END;
	}
	potion_loader_add(P, potion_str(P, argv[i+1]));
	i++;
      }
      continue;
    }
    if (!strcmp(argv[i], "-V") || !strcmp(argv[i], "--verbose")) {
      P->flags |= DEBUG_VERBOSE;
      continue;
    }
    if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--version")) {
      potion_cmd_version(P);
      goto END;
    }
    if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
      potion_cmd_usage(P);
      goto END;
    }
    if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--stats")) {
      potion_cmd_stats(P);
      goto END;
    }
    if (!strcmp(argv[i], "--compile") || !strcmp(argv[i], "-c")) {
      exec = EXEC_COMPILE;
      continue;
    }
    if (!strcmp(argv[i], "--check")) {
      exec = EXEC_CHECK;
      continue;
    }
    if (!strcmp(argv[i], "-B") || !strcmp(argv[i], "--bytecode")) {
      exec = EXEC_VM;
      continue;
    }
    if (!strcmp(argv[i], "-X") || !strcmp(argv[i], "--x86")) {
      exec = EXEC_JIT;
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
	printf("  P  parse verbose\n");
	printf("  c  compile\n");
	printf("  J  Jit\n");
	printf("  G  GC\n");
	goto END;
      }
      if (strchr(&argv[i][2], 'i')) P->flags |= DEBUG_INSPECT;
      if (strchr(&argv[i][2], 'v')) P->flags |= DEBUG_VERBOSE;
      if (strchr(&argv[i][2], 't')) { P->flags |= DEBUG_TRACE; exec = EXEC_VM; }
      if (strchr(&argv[i][2], 'p')) P->flags |= DEBUG_PARSE;
      if (strchr(&argv[i][2], 'P')) P->flags |= (DEBUG_PARSE | DEBUG_PARSE_VERBOSE);
      if (strchr(&argv[i][2], 'c')) P->flags |= DEBUG_COMPILE;
      if (strchr(&argv[i][2], 'J')) P->flags |= DEBUG_JIT;
      if (strchr(&argv[i][2], 'G')) P->flags |= DEBUG_GC;
      continue;
    }
#endif
    if (argv[i][0] == '-' && argv[i][1] == 'e') {
      char *arg;
      interactive = 0;
      if (strlen(argv[i]) == 2) {
	arg = argv[i+1];
      } else if (i <= argc) {
	arg = argv[i]+2;
      } else { // or go into interactive mode?
	potion_fatal("Missing argument for -e");
	goto END;
      }
      buf = potion_str(P, arg);
      continue;
    }
    if (i == argc - 1) {
      interactive = 0;
      continue;
    }
    fprintf(stderr, "** Unrecognized option: %s\n", argv[i]);
  }
  
  if (!interactive) {
    if (buf != PN_NIL) {
      potion_cmd_exec(P, buf, "-e", exec);
    } else {
      potion_cmd_compile(P, argv[argc-1], exec);
    }
  } else {
    if (!exec || P->flags & DEBUG_INSPECT) potion_fatal("no filename given");
    potion_eval(P, potion_byte_str(P,
      "load 'readline'\n" \
      "loop:\n" \
      "  code = readline('>> ')\n" \
      "  if (not code): \"\\n\" print, break.\n" \
      "  if (code != ''):\n" \
      "    obj = code eval\n" \
      "    if (obj kind == Error):\n" \
      "      obj string print." \
      "    else: ('=> ', obj, \"\\n\") join print.\n" \
      "  .\n"
      "."), exec == EXEC_JIT ? 1 : 0);
  }
 END:
#if !defined(POTION_JIT_TARGET) || defined(DEBUG)
  if (P != NULL)
    potion_destroy(P);
#endif
  return 0;
}
