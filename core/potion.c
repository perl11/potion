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
                             " (date='" POTION_DATE "', commit='" POTION_COMMIT "')";
const char potion_version[] = POTION_VERSION;

static void potion_cmd_usage() {
  printf("usage: potion [options] [script] [arguments]\n"
      "  -V, --verbose      show bytecode and ast info\n"
      "  -c, --compile      compile the script to bytecode\n"
      "  -h, --help         show this helpful stuff\n"
      "  -v, --version      show version\n");
}

static void potion_cmd_stats() {
  printf("sizeof(PN): %d\nsizeof(PNGarbage): %d\nsizeof(PNObject): %d\n",
      (int)sizeof(PN), (int)sizeof(struct PNGarbage), (int)sizeof(struct PNObject));
  printf("sizeof(PNTuple): %d\nsizeof(PN + PNTuple + PN_NUM): %d\n",
      (int)sizeof(struct PNTuple), (int)((2 * sizeof(PN)) + sizeof(struct PNTuple)));
}

static void potion_cmd_version() {
  printf("%s\n", potion_banner);
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
      if (verbose)
        printf("\n\n-- loaded --\n");
    } else {
      code = potion_parse(P, buf);
      if (verbose) {
        printf("\n-- parsed --\n");
        potion_send(code, PN_inspect);
        printf("\n");
      }
      code = potion_send(code, PN_compile, potion_str(P, filename), PN_NIL);
      if (verbose)
        printf("\n-- compiled --\n");
    }
    if (verbose) {
      potion_send(code, PN_inspect);
      printf("\n");
    }
    if (exec == 1) {
      code = potion_vm(P, code, PN_EMPTY, PN_EMPTY);
      if (verbose) {
        printf("\n-- returned --\n");
        potion_send(code, PN_inspect);
        printf("\n");
      }
    } else if (exec == 2) {
#ifdef X86_JIT
      jit_t func = potion_x86_proto(P, code);
      printf("JIT: %ld\n", PN_INT(func()));
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

static void potion_cmd_fib() {
  Potion *P = potion_create();
  PN fib = potion_parse(P, potion_str(P,
    "fib = (n):\n"
    "  if (n >= 1, 1, fib (n - 1) + fib (n - 2)).\n"
    "fib (40) print"
  ));
  fib = potion_send(fib, PN_compile);
  potion_send(fib, PN_inspect);
  potion_destroy(P);
}

int main(int argc, char *argv[]) {
  int i, verbose = 0;

  if (argc > 0) {
    for (i = 0; i < argc; i++) {
      if (strcmp(argv[i], "-V") == 0 ||
          strcmp(argv[i], "--verbose") == 0) {
        verbose = 1;
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
        if (i == argc - 1)
          fprintf(stderr, "** compiler requires a file name\n");
        else
          potion_cmd_compile(argv[++i], 0, verbose);
        return 0;
      }

      if (strcmp(argv[i], "-X") == 0 ||
          strcmp(argv[i], "--X86") == 0) {
        if (i == argc - 1)
          fprintf(stderr, "** compiler requires a file name\n");
        else
          potion_cmd_compile(argv[++i], 2, verbose);
        return 0;
      }

      if (strcmp(argv[i], "-f") == 0) {
        potion_cmd_fib();
        return 0;
      }
    }

    potion_cmd_compile(argv[argc-1], 1, verbose);
    return 0;
  }

  potion_run();
  return 0;
}
