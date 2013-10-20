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
  printf("usage: potion [options] [filename] [arguments]\n"
      "  -e  code           execute code instead of filename\n"
      "  -B, --bytecode     run with bytecode VM (slower, but cross-platform)\n"
      "  -X, --x86          run with x86 JIT VM (faster, x86 and x86-64)\n"
      "  -I, --inspect      print only the return value\n"
      "  -V, --verbose      show bytecode and ast info\n"
      "  -Ldirectory        add library search path\n"
      "  -Mmodule[=args]    load module\n"
      "  -d[module[=args]]  debug script\n"
      "      --debug[=module[,args]]\n"
      "  -c, --compile      compile the script to bytecode\n"
      "      --compile={target[,args]} compile to target: c or exe\n" //TODO: js,jvm,.net
      "      --check        check the script syntax and exit\n"
      "  -h, --help         show this helpful stuff\n"
#ifdef DEBUG
      "  -D[itPpcvGJ?]       debugging flags, try -D?\n"
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

#define DBG_Pv(c) \
    if (P->flags & DEBUG_VERBOSE) \
      potion_p(P, c)
//v OR i!
#define DBG_Pvi(c) \
    if (P->flags & (DEBUG_INSPECT|DEBUG_VERBOSE)) \
      potion_p(P, c)

static PN potion_cmd_exec(Potion *P, PN buf, char *filename, char *compile, char *addcode) {
  exec_mode_t exec = (exec_mode_t)(P->flags & ((1<<EXEC_BITS)-1));
  int fd = -1;
  PN code = PN_NIL;

  if (!buf && filename) {
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
    long size = stats.st_size;
    char *bufptr;
    if (addcode) {
      int len = strlen(addcode);
      size += len;
      buf = potion_bytes(P, size);
      bufptr = PN_STR_PTR(buf);
      PN_MEMCPY_N(bufptr, addcode, char, len);
      bufptr += len;
    } else {
      buf = potion_bytes(P, size);
      bufptr = PN_STR_PTR(buf);
    }
    // TODO: mmap instead of read all
    if (read(fd, bufptr, stats.st_size) == stats.st_size) {
      PN_STR_PTR(buf)[size] = '\0';
    } else {
      fprintf(stderr, "** could not read entire file.");
      goto done;
    }
  }
  else if (!filename) filename = "-e";

  code = potion_source_load(P, PN_NIL, buf);
  if (PN_IS_PROTO(code)) {
    DBG_v("\n-- loaded --\n");
  } else {
    code = potion_parse(P, buf, filename);
    if (!code || PN_TYPE(code) == PN_TERROR) {
      potion_p(P, code);
      return code;
    }
    DBG_v("\n-- parsed --\n");
    DBG_Pv(code);
    code = potion_send(code, PN_compile, potion_str(P, filename),
		       compile ? potion_str(P, compile): PN_NIL);
    DBG_v("\n-- compiled --\n");
  }
  DBG_Pv(code);
  if (exec == EXEC_VM || exec == EXEC_DEBUG) {
    code = potion_vm(P, code, P->lobby, PN_NIL, 0, NULL);
    DBG_v("\n-- vm returned %p (fixed=%ld, actual=%ld, reserved=%ld, time=%0.6gms %dx/%dm/%di) --\n",
	  (void *)code,
	  PN_INT(potion_gc_fixed(P, 0, 0)), PN_INT(potion_gc_actual(P, 0, 0)),
	  PN_INT(potion_gc_reserved(P, 0, 0)), P->mem->time * 1000, P->mem->pass,
	  P->mem->majors, P->mem->minors);
    DBG_Pvi(code);
  } else if (exec == EXEC_JIT) {
#ifdef POTION_JIT_TARGET
    PN val;
    PN cl = potion_closure_new(P, (PN_F)potion_jit_proto(P, code), PN_NIL, 1);
    PN_CLOSURE(cl)->data[0] = code;
    val = PN_PROTO(code)->jit(P, cl, P->lobby);
    DBG_v("\n-- jit returned %p (fixed=%ld, actual=%ld, reserved=%ld, time=%0.6gms %dx/%dm/%di) --\n", PN_PROTO(code)->jit,
	  PN_INT(potion_gc_fixed(P, 0, 0)), PN_INT(potion_gc_actual(P, 0, 0)),
	  PN_INT(potion_gc_reserved(P, 0, 0)), P->mem->time * 1000, P->mem->pass,
	  P->mem->majors, P->mem->minors);
    DBG_Pvi(val);
#else
    fprintf(stderr, "** potion built without JIT support\n");
#endif
  }
  else if (exec == EXEC_CHECK) {
    DBG_v("\n-- check --\n");
  }

  if (!code || PN_TYPE(code) == PN_TERROR)
    goto done;

  if (exec >= MAX_EXEC)
    potion_fatal("fatal: stack corruption (exec > MAX_EXEC)\n");

  if (exec == EXEC_COMPILE) { // needs an inputfile. TODO: -e"" -ofile
    char outpath[255];
    FILE *pnb;
    PN opts;
    char *c_opts = NULL;
    PN_SIZE written = 0;
    if (compile) { // --compile=c[,OPTS]
      if ((c_opts = strchr(compile,','))) {
	c_opts[0] = '\0';
	c_opts++;
	//TODO check -o for outpath
      }
    }
    if (!compile || !strcmp(compile, "bc"))
      sprintf(outpath, "%sb", filename);  // .pnb
    else if (!strcmp(compile, "c"))
      sprintf(outpath, "%s.c", filename); // .pn.c
    else if (!strcmp(compile, "exe"))
      sprintf(outpath, "%s.out", filename); // TODO: strip ext
    opts = c_opts
      ? pn_printf(P, potion_bytes(P,0), "%s,-o%s", c_opts, outpath)
      : potion_strcat(P, "-o", outpath);

    pnb = fopen(outpath, "wb");
    if (!pnb) {
      fprintf(stderr, "** could not open %s for writing. check permissions.\n", outpath);
      goto done;
    }
    if (!compile)
      code = potion_source_dumpbc(P, 0, code, PN_NIL);
    else
      code = potion_source_dump(P, 0, code,
				potion_str(P, compile),
				opts ? opts : PN_NIL);
    if (code &&
	(written = fwrite(PN_STR_PTR(code), 1, PN_STR_LEN(code), pnb) == PN_STR_LEN(code))) {
      printf("** compiled code saved to %s\n", outpath);
      fclose(pnb);

      if (!compile || !strcmp(compile, "bc"))
	printf("** run it with: potion %s\n", outpath);
      // TODO: let the compilers write its own hints (,-ooutfile)
      else if (!strcmp(compile, "c"))
	printf("** compile it with: %s %s %s\n", POTION_CC, POTION_CFLAGS, outpath);
      else if (!strcmp(compile, "exe"))
	printf("** run it with: ./%s\n", outpath);
    } else {
      fprintf(stderr, "** could not write all %s compiled code (%u/%u) to %s\n",
	      compile?compile:"bytecode", written, code?PN_STR_LEN(code):0, outpath);
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

done:
  if (fd != -1)
    close(fd);

  return code;
}

char * addmodule(Potion *P, char *result, char *prefix, char *name) {
  char *args = strchr(name, '=');
  PN out = potion_bytes(P, 0);
  if (args) *args++ = '\0';
  if (prefix) {
    pn_printf(P, out, "load \"%s/%s\"\n", prefix, name);
  } else {
    pn_printf(P, out, "load \"%s\"\n", name);
  }
  if (args)
    pn_printf(P, out, "%s(%s)\n", name, args);
  else
    pn_printf(P, out, "%s()\n", name);
  return PN_STR_PTR(out);
}

int main(int argc, char *argv[]) {
  POTION_INIT_STACK(sp);
  int i;
  exec_mode_t exec = POTION_JIT ? EXEC_JIT : EXEC_VM;
  Potion *P = potion_create(sp);
  PN buf = PN_NIL;
  char *compile = NULL;
  char *fn = NULL;
  char *addmodules = NULL;

  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--")) { i++; break; }
    if (!strcmp(argv[i], "-I") || !strcmp(argv[i], "--inspect")) {
      P->flags |= DEBUG_INSPECT; continue; }
    if (!strcmp(argv[i], "-L")) {
#ifdef SANDBOX
    potion_fatal("-L disabled in SANDBOX");
#else
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
#endif /* SANDBOX */
    }
    if (!strcmp(argv[i], "-V") || !strcmp(argv[i], "--verbose")) {
      P->flags |= DEBUG_VERBOSE; continue; }
    if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--version")) {
      potion_cmd_version(P); goto END; }
    if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
      potion_cmd_usage(P); goto END; }
    if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--stats")) {
      potion_cmd_stats(P); goto END; }
    if (!strcmp(argv[i], "--check")) { exec = EXEC_CHECK; continue; }
    if (!strncmp(argv[i], "--compile=", 10)) {
      exec = EXEC_COMPILE; compile = &argv[i][10]; continue; }
    if (!strcmp(argv[i], "--compile") || !strcmp(argv[i], "-c")) {
      exec = EXEC_COMPILE; compile = NULL; continue; }
    if (!strcmp(argv[i], "-B") || !strcmp(argv[i], "--bytecode")) {
      exec = EXEC_VM; continue; }
    if (!strcmp(argv[i], "-X") || !strcmp(argv[i], "--x86")) {
      exec = EXEC_JIT; continue; }
    if (!strcmp(argv[i], "--debug") || !strcmp(argv[i], "-d")) {
      addmodules = addmodule(P, addmodules, NULL, "debug");
      exec = EXEC_DEBUG; continue; }
    if (!strcmp(argv[i], "--debug=")) {
      addmodules = addmodule(P, addmodules, "debug", &argv[i][9]);
      exec = EXEC_DEBUG; continue; }
    if (argv[i][0] == '-' && argv[i][1] == 'd') {
      if (argv[i][2] == '=')
	addmodules = addmodule(P, addmodules, NULL,
			       PN_STR_PTR(PN_STRCAT("debug=", &argv[i][3])));
      else
	addmodules = addmodule(P, addmodules, "debug", &argv[i][2]);
      exec = EXEC_DEBUG; continue; }
    if (argv[i][0] == '-' && argv[i][1] == 'M') {
      addmodules = addmodule(P, addmodules, NULL, &argv[i][2]);
      continue; }
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
	printf("  J  disassemble Jit code\n");
	printf("  G  GC (use w/ or wo/ -Dv\n");
	goto END;
      }
      if (strchr(&argv[i][2], 'i')) P->flags |= DEBUG_INSPECT;
      if (strchr(&argv[i][2], 'v')) P->flags |= DEBUG_VERBOSE;
      if (strchr(&argv[i][2], 't')) { P->flags |= DEBUG_TRACE;
	exec = exec==EXEC_JIT ? EXEC_VM : exec; }
      if (strchr(&argv[i][2], 'p')) P->flags |= DEBUG_PARSE;
      if (strchr(&argv[i][2], 'P')) P->flags |= DEBUG_PARSE_VERBOSE;
      if (strchr(&argv[i][2], 'c')) P->flags |= DEBUG_COMPILE;
      if (strchr(&argv[i][2], 'J')) P->flags |= DEBUG_JIT;
      if (strchr(&argv[i][2], 'G')) P->flags |= DEBUG_GC;
      continue;
    }
#endif
    if (argv[i][0] == '-') {
      if (argv[i][1] == 'e') {
        char *arg;
        if (strlen(argv[i]) == 2) {
          arg = argv[i+1];
        } else if (i <= argc) {
          arg = argv[i]+2;
        } else { // or go into interactive mode?
          potion_fatal("Missing argument for -e");
          goto END;
        }
	if (addmodules)
	  buf = potion_strcat(P, addmodules, arg);
	else
	  buf = potion_str(P, arg);
        continue;
      } else {
        fprintf(stderr, "** Unrecognized option: %s\n", argv[i]);
      }
    }
    else {
      break;
    }
  }
  P->flags = P->flags + exec;
  
  if (buf || i < argc) {
    PN args = PN_TUP0();
    if (buf == PN_NIL) { fn = argv[i++]; PN_PUSH(args, PN_STR(fn)); }
    else { PN_PUSH(args, PN_STR("-e")); }
    for (; i < argc; i++) PN_PUSH(args, PN_STR(argv[i]));
    potion_define_global(P, PN_STR("argv"), args);
    if (buf != PN_NIL) {
      potion_cmd_exec(P, buf, NULL, compile, PN_NIL);
    } else {
      potion_cmd_exec(P, buf, fn, compile, addmodules);
    }
  } else {
    if (!exec || P->flags & DEBUG_INSPECT) potion_fatal("no filename given");
    PN args = PN_TUP0();
    PN_PUSH(args, PN_STR(""));
    potion_define_global(P, PN_STR("argv"), args);
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
      "."));
  }
 END:
#if !defined(POTION_JIT_TARGET) || defined(DEBUG)
  if (P != NULL)
    potion_destroy(P);
#endif
  return 0;
}
