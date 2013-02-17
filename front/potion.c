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

static void potion_cmd_usage() {
  printf("usage: potion [options] [script] [arguments]\n"
      "  -B, --bytecode     run with bytecode VM (slower, but cross-platform)\n"
      "  -X, --x86          run with x86 JIT VM (faster, x86 and x86-64)\n"
      "  -I, --inspect      print only the return value\n"
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

static void potion_cmd_stats(void *sp) {
  Potion *P = potion_create(sp);
  printf("sizeof(PN=%d, PNObject=%d, PNTuple=%d, PNTuple+1=%d, PNTable=%d)\n",
      (int)sizeof(PN), (int)sizeof(struct PNObject), (int)sizeof(struct PNTuple),
      (int)(sizeof(PN) + sizeof(struct PNTuple)), (int)sizeof(struct PNTable));
  printf("GC (fixed=%ld, actual=%ld, reserved=%ld)\n",
      PN_INT(potion_gc_fixed(P, 0, 0)), PN_INT(potion_gc_actual(P, 0, 0)),
      PN_INT(potion_gc_reserved(P, 0, 0)));
  potion_destroy(P);
}

static void potion_cmd_version() {
  printf(potion_banner, POTION_JIT);
}

static void potion_cmd_compile(char *filename, int exec, int verbose, void *sp) {
  PN buf;
  int fd = -1;
  struct stat stats;
  Potion *P = potion_create(sp);
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
      if (verbose > 1)
        printf("\n\n-- loaded --\n");
    } else {
      code = potion_parse(P, buf);
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
#if POTION_JIT == 1
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

#if 0
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
  if (P != NULL)
    potion_destroy(P);
}

int main(int argc, char *argv[]) {
  POTION_INIT_STACK(sp);
  int i, verbose = 0, exec = 1 + POTION_JIT, interactive = 1;
  
  for (i = 1; i < argc; i++) {
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
      potion_cmd_stats(sp);
      return 0;
    }

    if (strcmp(argv[i], "-c") == 0 ||
        strcmp(argv[i], "--compile") == 0) {
      exec = 0;
      continue;
    }

    if (strcmp(argv[i], "-B") == 0 ||
        strcmp(argv[i], "--bytecode") == 0) {
      exec = 1;
      continue;
    }

    if (strcmp(argv[i], "-X") == 0 ||
        strcmp(argv[i], "--x86") == 0) {
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
    potion_cmd_compile(argv[argc-1], exec, verbose, sp);
  } else {
    if (!exec || verbose) potion_fatal("no filename given");
    Potion *P = potion_create(sp);
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
      "."), exec - 1);
    potion_destroy(P);
  }
  return 0;
}
