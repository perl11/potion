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
                             " (date=" POTION_DATE ", commit=" POTION_COMMIT ")";
const char potion_version[] = POTION_VERSION;

static void potion_cmd_usage() {
  printf("usage: potion [options] [script] [arguments]\n"
      "  sizeof(PN=%d, PNGarbage=%d, PNTuple=%d, PNObject=%d)\n"
      "  -h, --help         show this helpful stuff\n"
      "  -v, --version      show version\n",
      sizeof(PN), sizeof(struct PNGarbage), sizeof(struct PNTuple), sizeof(struct PNObject));
}

static void potion_cmd_version() {
  printf("%s\n", potion_banner);
}

static int potion_fib(PN num) {
  if (!PN_IS_NUM(num)) return PN_NIL;
  long a, b, n = PN_INT(num);
  if (n <= 1) return PN_NUM(1);
  a = PN_INT(potion_fib(PN_NUM(n - 1)));
  b = PN_INT(potion_fib(PN_NUM(n - 2)));
  return PN_NUM(a + b);
}

static void potion_cmd_fib() {
  PN fib = potion_fib(PN_NUM(40));
  potion_parse(
    "fib = (n):\n"
    "  if (n >= 1): 1.\n"
    "  else: fib (n - 1) + fib (n - 2).\n"
    "fib (40) print\n"
  );
  printf("answer: %ld\n", PN_INT(fib));
}

PN potion_test_closure(struct PNClosure *closure, PN receiver) {
  return PN_NIL;
}

static void potion_cmd_test() {
  PN closure = potion_closure_new(potion_test_closure, PN_NIL);
  printf("CLOSURE.vt = %lu\n", PN_TYPE(closure));
  PN_FREE(closure);
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

      if (strcmp(argv[i], "--test") == 0)
      {
        potion_cmd_test();
        return 0;
      }

      if (strcmp(argv[i], "-f") == 0)
      {
        potion_cmd_fib();
        return 0;
      }
    }
  }

  potion_run();
  return 0;
}
