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
//    "  -c                 check script and exit\n"          // TODO:
//    "  -d                 run program under the debugger\n" // TODO: run p2d or do it internally?
//    "  -e script          execute string\n" // todo:
//    "  -E script          execute string with extended features enabled\n" // TODO: add a "use p2;"
      "  -V, --verbose      print bytecode and ast info\n"
      "  -h, --help         print this usage info and exit\n"
      "  -v, --version      print version, patchlevel and features and exit\n"
      "  --inspect          print the return value\n"
      "  --stats            print statistics and exit\n"
      "  --compile          compile the script to bytecode and exit\n"
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

static void p2_cmd_compile(Potion *P, char *filename, int exec, int verbose) {
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
    code = p2_source_load(P, PN_NIL, buf);
    if (PN_IS_PROTO(code)) {
      if (verbose > 1)
        printf("\n\n-- loaded --\n");
    } else {
      code = p2_parse(P, buf);
      if (!code || PN_TYPE(code) == PN_TERROR) {
        potion_p(P, code);
        goto done;
      }
      if (verbose > 1) {
        printf("\n-- parsed --\n");
        potion_p(P, code);
      }
      code = potion_send(code, PN_compile, potion_str(P, filename), PN_NIL);
      if (verbose > 1)
        printf("\n-- compiled --\n");
    }
    if (verbose > 1) potion_p(P, code);
    if (exec == 1) {
      code = potion_vm(P, code, P->lobby, PN_NIL, 0, NULL);
      if (verbose > 1)
        printf("\n-- vm returned %p (fixed=%ld, actual=%ld, reserved=%ld) --\n", (void *)code,
          PN_INT(potion_gc_fixed(P, 0, 0)), PN_INT(potion_gc_actual(P, 0, 0)),
          PN_INT(potion_gc_reserved(P, 0, 0)));
      if (verbose) potion_p(P, code);
    } else if (exec == 2) {
#ifdef POTION_JIT_TARGET
      PN val;
      PN cl = potion_closure_new(P, (PN_F)potion_jit_proto(P, code, verbose), PN_NIL, 1);
      PN_CLOSURE(cl)->data[0] = code;
      val = PN_PROTO(code)->jit(P, cl, P->lobby);
      if (verbose > 1)
        printf("\n-- jit returned %p (fixed=%ld, actual=%ld, reserved=%ld) --\n", PN_PROTO(code)->jit,
          PN_INT(potion_gc_fixed(P, 0, 0)), PN_INT(potion_gc_actual(P, 0, 0)),
          PN_INT(potion_gc_reserved(P, 0, 0)));
      if (verbose) potion_p(P, val);
#else
      fprintf(stderr, "** p2 built without JIT support\n");
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
        fprintf(stderr, "** could not write all bytecode.");
      }
    }

#if 0 && defined(DEBUG)
    // GC check
    void *scanptr = (void *)((char *)P->mem->old_lo + (sizeof(PN) * 2));
    while ((PN)scanptr < (PN)P->mem->old_cur) {
          printf("%p.vt = %lx (%u)\n",
            scanptr, ((struct PNObject *)scanptr)->vt,
            potion_type_size(P, scanptr));
      if (((struct PNFwd *)scanptr)->fwd != POTION_FWD && ((struct PNFwd *)scanptr)->fwd != POTION_COPIED) {
        if (((struct PNObject *)scanptr)->vt < 0 || ((struct PNObject *)scanptr)->vt > PN_TUSER) {
          printf("wrong type for allocated object: %p.vt = %lx\n",
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
  int i, verbose = 0, exec = 1 + POTION_JIT, interactive = 1;
  Potion *P = potion_create(sp);
  
  for (i = 1; i < argc; i++) {
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
      continue;
    }
    if (strcmp(argv[i], "-V") == 0 ||
        strcmp(argv[i], "--verbose") == 0) {
      verbose = 2;
      continue;
    }
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
      p2_cmd_stats(P);
      goto END;
    }
    if (strcmp(argv[i], "--compile") == 0) {
      exec = 0;
      continue;
    }
    if (strcmp(argv[i], "-B") == 0 ||
        strcmp(argv[i], "--bytecode") == 0) {
      exec = 1;
      continue;
    }
    if (strcmp(argv[i], "-J") == 0 ||
        strcmp(argv[i], "--jit") == 0) {
      exec = 2;
      continue;
    }
    if (i == argc - 1) {
      interactive = 0;
      continue;
    }
    fprintf(stderr, "** Unrecognized option: %s\n", argv[i]);
  }
  
  if (!interactive) {
    p2_cmd_compile(P, argv[argc-1], exec, verbose);
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
      "}"), exec - 1); // 1/2 => 0/1
  }
END:
  if (P != NULL)
    potion_destroy(P);
  return 0;
}
