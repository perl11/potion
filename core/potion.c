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
      "  -B, --bytecode     run with bytecode VM (slower, but cross-platform)\n"
      "  -X, --x86          run with x86 JIT VM (faster, x86 and x86-64)\n"
      "  -Ldirectory        add library search path\n"
      "  -I, --inspect      print only the return value\n"
      "  -V, --verbose      show bytecode and ast info\n"
      "  -c, --compile      compile the script to bytecode\n"
      "      --check        check the script syntax and exit\n"
      "  -h, --help         show this helpful stuff\n"
#ifdef DEBUG
      "  -D[itpvGJ]         debugging flags\n"
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
    code = potion_source_load(P, PN_NIL, buf);
    if (PN_IS_PROTO(code)) {
      if (P->flags & DEBUG_VERBOSE)
        fprintf(stderr, "\n\n-- loaded --\n");
    } else {
      code = potion_parse(P, buf);
      if (!code || PN_TYPE(code) == PN_TERROR) {
	potion_p(P, code);
        goto done;
      }
      if (P->flags & DEBUG_VERBOSE) {
        fprintf(stderr, "\n-- parsed --\n");
	potion_p(P, code);
      }
      code = potion_send(code, PN_compile, potion_str(P, filename), PN_NIL);
      if (P->flags & DEBUG_VERBOSE)
        fprintf(stderr, "\n-- compiled --\n");
    }
    if (P->flags & DEBUG_VERBOSE) potion_p(P, code);
    if (exec == EXEC_VM) {
      code = potion_vm(P, code, P->lobby, PN_NIL, 0, NULL);
      if (P->flags & DEBUG_VERBOSE)
        fprintf(stderr, "\n-- vm returned %p (fixed=%ld, actual=%ld, reserved=%ld) --\n", (void *)code,
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
        fprintf(stderr, "\n-- jit returned %p (fixed=%ld, actual=%ld, reserved=%ld) --\n", PN_PROTO(code)->jit,
          PN_INT(potion_gc_fixed(P, 0, 0)), PN_INT(potion_gc_actual(P, 0, 0)),
          PN_INT(potion_gc_reserved(P, 0, 0)));
      if (P->flags & (DEBUG_INSPECT|DEBUG_VERBOSE))
	potion_p(P, val);
#else
      fprintf(stderr, "** potion built without JIT support\n");
#endif
    } else {
      char pnbpath[255];
      FILE *pnb;
      sprintf(pnbpath, "%sb", filename);
      pnb = fopen(pnbpath, "wb");
      if (!pnb) {
        fprintf(stderr, "** could not open %s for writing. check permissions.", pnbpath);
        goto done;
      }

      code = potion_source_dump(P, PN_NIL, code);
      if (fwrite(PN_STR_PTR(code), 1, PN_STR_LEN(code), pnb) == PN_STR_LEN(code)) {
        printf("** compiled code saved to %s\n", pnbpath);
        printf("** run it with: potion %s\n", pnbpath);
        fclose(pnb);
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
	  if ((signed long)(((struct PNObject *)scanptr)->vt) < 0
	      || ((struct PNObject *)scanptr)->vt > PN_TUSER) {
	    fprintf(stderr, "** wrong type for allocated object: %p.vt = %x\n",
		    scanptr, ((struct PNObject *)scanptr)->vt);
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
  int i, verbose = 0, exec = POTION_JIT ? EXEC_JIT : EXEC_VM, interactive = 1;
  Potion *P = potion_create(sp);
  
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-I") == 0 ||
        strcmp(argv[i], "--inspect") == 0) {
      verbose = 1;
      P->flags |= DEBUG_INSPECT;
      continue;
    }
    if (strcmp(argv[i], "-L") == 0) {
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

    if (strcmp(argv[i], "-V") == 0 ||
        strcmp(argv[i], "--verbose") == 0) {
      verbose = 2;
      P->flags |= DEBUG_VERBOSE;
      continue;
    }

    if (strcmp(argv[i], "-v") == 0 ||
        strcmp(argv[i], "--version") == 0) {
      potion_cmd_version(P);
      goto END;
    }

    if (strcmp(argv[i], "-h") == 0 ||
        strcmp(argv[i], "--help") == 0) {
      potion_cmd_usage(P);
      goto END;
    }
    if (strcmp(argv[i], "-s") == 0 ||
        strcmp(argv[i], "--stats") == 0) {
      potion_cmd_stats(P);
      goto END;
    }
    if (strcmp(argv[i], "--compile") == 0 ||
        strcmp(argv[i], "-c") == 0) {
      exec = EXEC_COMPILE;
      continue;
    }
    if (strcmp(argv[i], "--check") == 0) {
      exec = EXEC_CHECK;
      continue;
    }
    if (strcmp(argv[i], "-B") == 0 ||
        strcmp(argv[i], "--bytecode") == 0) {
      exec = EXEC_VM;
      continue;
    }
    if (strcmp(argv[i], "-X") == 0 ||
        strcmp(argv[i], "--x86") == 0) {
      exec = EXEC_JIT;
      continue;
    }
#ifdef DEBUG
    if (argv[i][0] == '-' && argv[i][1] == 'D') {
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
      if (strchr(&argv[i][2], 'J'))
	P->flags |= DEBUG_JIT;
      if (strchr(&argv[i][2], 'G'))
	P->flags |= DEBUG_GC;
      continue;
    }
#endif
    
    if (i == argc - 1) {
      interactive = 0;
      continue;
    }
    fprintf(stderr, "** Unrecognized option: %s\n", argv[i]);
  }
  
  if (!interactive) {
    potion_cmd_compile(P, argv[argc-1], exec);
  } else {
    if (!exec || verbose) potion_fatal("no filename given");
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
