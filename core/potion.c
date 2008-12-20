//
// potion.c
// the Potion!
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <string.h>
#include "potion.h"

const char potion_banner[] = "potion " POTION_VERSION
                             " (date=" POTION_DATE ", commit=" POTION_COMMIT ")";
const char potion_version[] = POTION_VERSION;

static void potion_cmd_usage() {
  printf("usage: potion [options] [script] [arguments]\n"
      "  -h, --help         show this helpful stuff\n"
      "  -v, --version      show version\n");
}

static void potion_cmd_version() {
  printf("%s\n", potion_banner);
}

int main(int argc, char *argv[]) {
  int i;

  if (argc > 0) {
    for (i = 0; i < argc; i++) {
      if (strcmp(argv[i], "-v") == 0 ||
          strcmp(argv[i], "--version") == 0)
      {
        potion_cmd_version();
        return 0;
      }

      if (strcmp(argv[i], "-h") == 0 ||
          strcmp(argv[i], "--help") == 0)
      {
        potion_cmd_usage();
        return 0;
      }
    }
  }

  potion_run();
  return 0;
}
