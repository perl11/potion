//
// potion.c
// the Potion!
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "potion.h"
#include "internal.h"
#include "opcodes.h"
#include "version.h"

const char potion_banner[] = "potion " POTION_VERSION
                             " (date='" POTION_DATE "', commit='" POTION_COMMIT
                             "', platform='" POTION_PLATFORM "', jit=%d)\n";
const char potion_version[] = POTION_VERSION;

static void potion_cmd_usage() {
  printf("usage: potion [options] [script] [arguments]\n"
      "  -B, --bytecode     run with bytecode VM (slower, but cross-platform)\n"
      "  -X, --x86          run with x86 JIT VM (faster, x86 and x86-64)\n"
      "  -I, --inspect      inspect the return value\n"
      "  -V, --verbose      show bytecode and ast info\n"
      "  -c, --compile      compile the script to bytecode\n"
      "  -h, --help         show this helpful stuff\n"
      "  -v, --version      show version\n"
      "(default: %s)\n",
#if POTION_JIT == 1
      "-X"
#else
      "-B"
#endif
  );
}

static void potion_cmd_stats() {
  printf("sizeof(PN): %d\nsizeof(PNGarbage): %d\nsizeof(PNObject): %d\n",
      (int)sizeof(PN), (int)sizeof(struct PNGarbage), (int)sizeof(struct PNObject));
  printf("sizeof(PNTuple): %d\nsizeof(PN + PNTuple): %d\n",
      (int)sizeof(struct PNTuple), (int)(sizeof(PN) + sizeof(struct PNTuple)));
}

static void potion_cmd_version() {
  printf(potion_banner, POTION_JIT);
}

static void potion_cmd_compile(char *filename, int exec, int verbose) {
  PN buf;
  FILE *fp;
  struct stat stats;
  Potion *P = potion_create();
  if (stat(filename, &stats) == -1) {
    fprintf(stderr, "** %s does not exist.", filename);
    goto done;
  }

  fp = fopen(filename, "rb");
  if (!fp) {
    fprintf(stderr, "** could not open %s. check permissions.", filename);
    goto done;
  }

  buf = potion_bytes(P, stats.st_size);
  if (fread(PN_STR_PTR(buf), 1, stats.st_size, fp) == stats.st_size) {
    PN code;
    code = potion_source_load(P, PN_NIL, buf);
    if (PN_IS_PROTO(code)) {
      if (verbose > 1)
        printf("\n\n-- loaded --\n");
    } else {
      code = potion_parse(P, buf);
      if (verbose > 1) {
        printf("\n-- parsed --\n");
        potion_send(potion_send(code, PN_inspect), PN_print);
        printf("\n");
      }
      code = potion_send(code, PN_compile, potion_str(P, filename), PN_NIL);
      if (verbose > 1)
        printf("\n-- compiled --\n");
    }
    if (verbose > 1) {
      potion_send(potion_send(code, PN_inspect), PN_print);
      printf("\n");
    }
    if (exec == 1) {
      code = potion_vm(P, code, PN_NIL, 0, NULL);
      if (verbose > 1)
        printf("\n-- returned %lu --\n", code);
      if (verbose) {
        potion_send(potion_send(code, PN_inspect), PN_print);
        printf("\n");
      }
    } else if (exec == 2) {
#ifdef X86_JIT
      PN val;
      PN_F func = potion_x86_proto(P, code);
      val = func(P, PN_NIL, PN_NIL);
      if (verbose > 1)
        printf("\n-- jit returned %p --\n", func);
      if (verbose) {
        potion_send(potion_send(val, PN_inspect), PN_print);
        printf("\n");
      }
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
        fprintf(stderr, "** could not write all bytecode.");
      }
    }
  } else {
    fprintf(stderr, "** could not read entire file.");
  }

  fclose(fp);

done:
  potion_destroy(P);
}

int main(int argc, char *argv[]) {
  int i, verbose = 0, exec = 1 + POTION_JIT;

#if defined(__MacOSX__)
  printf("", 0); // hack by quag to initialize stdout on Mac OS X.
                 // http://github.com/why/potion/commit/edf3ff8#comments
#endif

  if (argc > 1) {
    for (i = 0; i < argc; i++) {
      if (strcmp(argv[i], "-I") == 0 ||
          strcmp(argv[i], "--inspect") == 0) {
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
        potion_cmd_version();
        return 0;
      }

      if (strcmp(argv[i], "-h") == 0 ||
          strcmp(argv[i], "--help") == 0) {
        potion_cmd_usage();
        return 0;
      }

      if (strcmp(argv[i], "-s") == 0 ||
          strcmp(argv[i], "--stats") == 0) {
        potion_cmd_stats();
        return 0;
      }

      if (strcmp(argv[i], "-c") == 0 ||
          strcmp(argv[i], "--compile") == 0) {
        exec = 0;
      }

      if (strcmp(argv[i], "-B") == 0 ||
          strcmp(argv[i], "--bytecode") == 0) {
        exec = 1;
      }

      if (strcmp(argv[i], "-X") == 0 ||
          strcmp(argv[i], "--x86") == 0) {
        exec = 2;
      }
    }

    potion_cmd_compile(argv[argc-1], exec, verbose);
    return 0;
  }

  fprintf(stderr, "// TODO: read from stdin\n");
  potion_cmd_usage();
  return 0;
}
