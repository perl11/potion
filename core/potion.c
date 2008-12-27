//
// potion.c
// the Potion!
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "internal.h"
#include "potion.h"

const char potion_banner[] = "potion " POTION_VERSION
                             " (date='" POTION_DATE "', commit='" POTION_COMMIT "')";
const char potion_version[] = POTION_VERSION;

static void potion_cmd_usage() {
  printf("usage: potion [options] [script] [arguments]\n"
      "  sizeof(PN=%lu, PNGarbage=%lu, PNTuple=%lu, PNObject=%lu, PNString=%lu)\n"
      "  -c, --compile      compile the script to bytecode\n"
      "  -h, --help         show this helpful stuff\n"
      "  -v, --version      show version\n",
      sizeof(PN), sizeof(struct PNGarbage), sizeof(struct PNTuple), sizeof(struct PNObject), sizeof(struct PNString));
}

static void potion_cmd_version() {
  printf("%s\n", potion_banner);
}

static void potion_cmd_compile(char *filename) {
  Potion *P = potion_create();
  potion_destroy(P);
}

static void potion_cmd_fib() {
  Potion *P = potion_create();
  potion_parse(P, potion_str(P,
    "fib = (n):\n"
    "  if (n >= 1, 1, fib (n - 1) + fib (n - 2)).\n"
    "fib (40) print"
  ));
  potion_destroy(P);
}

int main(int argc, char *argv[]) {
  int i;

  if (argc > 0) {
    for (i = 0; i < argc; i++) {
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

      if (strcmp(argv[i], "-c") == 0 ||
          strcmp(argv[i], "--compile") == 0) {
        if (i == argc - 1)
          fprintf(stderr, "** compiler requires a file name\n");
        else
          potion_cmd_compile(argv[i++]);
        return 0;
      }

      if (strcmp(argv[i], "-f") == 0) {
        potion_cmd_fib();
        return 0;
      }
    }
  }

  potion_run();
  return 0;
}
