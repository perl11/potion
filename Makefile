# posix (linux, bsd, osx, solaris) + mingw with gcc/clang only
.SUFFIXES: .g .c .i .i2 .o .opic .o2 .opic2 .textile .html

SRC = core/asm.c core/ast.c core/callcc.c core/compile.c core/contrib.c core/file.c core/gc.c core/internal.c core/lick.c core/load.c core/mt19937ar.c core/number.c core/objmodel.c core/primitive.c core/string.c core/table.c core/vm.c core/vm-ppc.c core/vm-x86.c
SRC_SYN = core/syntax.c
SRC_P2_SYN = core/syntax-p5.c
SRC_POTION = core/potion.c
SRC_P2 = core/p2.c
OBJ = ${SRC:.c=.o}
OBJ_SYN = ${SRC_SYN:.c=.o}
OBJ_POTION = ${SRC_POTION:.c=.o}
OBJ_P2 = ${SRC_P2:.c=.o}
OBJ_P2_SYN = ${SRC_P2_SYN:.c=.o}
PIC_OBJ = ${SRC:.c=.opic}
PIC_OBJ_SYN = ${SRC_SYN:.c=.opic}
PIC_OBJ_POTION = ${SRC_POTION:.c=.opic}
PIC_OBJ_P2 = ${SRC_P2:.c=.opic}
PIC_OBJ_P2_SYN = ${SRC_P2_SYN:.c=.opic}
OBJ_TEST = test/api/potion-test.o test/api/CuTest.o
OBJ_P2_TEST = test/api/p2-test.o test/api/CuTest.o
OBJ_GC_TEST = test/api/gc-test.o test/api/CuTest.o
OBJ_GC_BENCH = test/api/gc-bench.o
DOC = doc/start.textile
DOCHTML = ${DOC:.textile=.html}

CAT  = /bin/cat
ECHO = /bin/echo
SED  = sed
EXPR = expr
GREG = tools/greg${EXE}

INCS = -Icore
RUNPOTION = ./potion
RUNP2 = ./p2

# bootstrap config.inc with make -f config.mak
include config.inc

all: pn p2${EXE}
	+${MAKE} -s usage

pn: potion${EXE} lib/readline${LOADEXT}

rebuild: clean potion${EXE} test

usage:
	@${ECHO} " "
	@${ECHO} " ~ using potion ~"
	@${ECHO} " "
	@${ECHO} " Running a script."

	@${ECHO} " "
	@${ECHO} "   $$ ./potion example/fib.pn"
	@${ECHO} " "
	@${ECHO} " Dump the AST and bytecode inspection for a script. "
	@${ECHO} " "
	@${ECHO} "   $$ ./potion -V example/fib.pn"
	@${ECHO} " "
	@${ECHO} " Compiling to bytecode."
	@${ECHO} " "
	@${ECHO} "   $$ ./potion -c example/fib.pn"
	@${ECHO} "   $$ ./potion example/fib.pnb"
	@${ECHO} " "
	@${ECHO} " Potion builds its JIT compiler by default, but"
	@${ECHO} " you can use the bytecode VM by running scripts"
	@${ECHO} " with the -B flag."
	@${ECHO} " "
	@${ECHO} " If you built with JIT=0, then the bytecode VM"
	@${ECHO} " will run by default."
	@${ECHO} " "
	@${ECHO} " To verify your build,"
	@${ECHO} " "
	@${ECHO} "   $$ make test"
	@${ECHO} " "
	@${ECHO} " Written by _why <why@whytheluckystiff.net>"
	@${ECHO} " Maintained at https://github.com/perl11/potion, branch p2"

config:
	@${ECHO} MAKE -f config.mak $@
	@${MAKE} -s -f config.mak

# Force sync with config.inc
core/config.h: core/version.h tools/config.sh config.mak
	@${ECHO} MAKE -f config.mak $@
	@${MAKE} -s -f config.mak $@

core/version.h: .git/HEAD .git/$(shell git symbolic-ref HEAD)
	@${MAKE} -s -f config.mak $@

# bootstrap config.inc
config.inc: tools/config.sh config.mak
	@${ECHO} MAKE -f config.mak $@
	@${MAKE} -s -f config.mak $@

core/callcc.o core/callcc.o2: core/callcc.c
	@${ECHO} CC $< +frame-pointer
	@${CC} -c -fno-omit-frame-pointer ${INCS} -o $@ $<

core/callcc.opic core/callcc.opic2: core/callcc.c
	@${ECHO} CC -fPIC $< +frame-pointer
	@${CC} -c -fPIC -fno-omit-frame-pointer ${INCS} -o $@ $<

%.i: %.c core/config.h
	@${ECHO} CPP $@
	@${CC} -c ${CFLAGS} ${INCS} -o $@ -E -c $<
%.i2: %.c core/config.h
	@${ECHO} CPP $@
	@${CC} -c -DP2 ${CFLAGS} ${INCS} -o $@ -E -c $<
%.o: %.c core/config.h
	@${ECHO} CC $<
	@${CC} -c ${CFLAGS} ${INCS} -o $@ $<
%.opic: %.c core/config.h
	@${ECHO} CC -fPIC $<
	@${CC} -c -fPIC ${CFLAGS} ${INCS} -o $@ $<
%.o2: %.c core/config.h
	@${ECHO} CC -DP2 $<
	@${CC} -c -DP2 ${CFLAGS} ${INCS} -o $@ $<
%.opic2: %.c core/config.h
	@${ECHO} CC -DP2 -fPIC $<
	@${CC} -c -DP2 -fPIC ${CFLAGS} ${INCS} -o $@ $<

.c.i: core/config.h
	@${ECHO} CPP $@
	@${CC} -c ${CFLAGS} ${INCS} -o $@ -E -c $<
.c.i2: core/config.h
	@${ECHO} CPP $@
	@${CC} -c -DP2 ${CFLAGS} ${INCS} -o $@ -E -c $<
.c.o: core/config.h
	@${ECHO} CC $<
	@${CC} -c ${CFLAGS} ${INCS} -o $@ $<
.c.o2: core/config.h
	@${ECHO} CC -DP2 $<
	@${CC} -c -DP2 ${CFLAGS} ${INCS} -o $@ $<
.c.opic: core/config.h
	@${ECHO} CC -fPIC $<
	@${CC} -c -fPIC ${CFLAGS} ${INCS} -o $@ $<
.c.opic2: core/config.h
	@${ECHO} CC -DP2 -fPIC $<
	@${CC} -c -DP2 -fPIC ${CFLAGS} ${INCS} -o $@ $<

%.c: %.g ${GREG}
	@${ECHO} GREG $<
	@${GREG} $< > $@
.g.c: ${GREG}
	@${ECHO} GREG $<
	@${GREG} $< > $@

${GREG}: tools/greg.c tools/compile.c tools/tree.c
	@${ECHO} CC $@
	@${CC} -O3 -DNDEBUG -o $@ tools/greg.c tools/compile.c tools/tree.c -Itools

potion${EXE}: ${OBJ_POTION} libpotion${DLL}
	@${ECHO} LINK $@
	@${CC} ${CFLAGS} ${OBJ_POTION} -o $@ ${LDEXEFLAGS} -lpotion ${LIBS}
	@if [ "${DEBUG}" != "1" ]; then \
		${ECHO} STRIP $@; \
	  ${STRIP} $@; \
	fi

p2${EXE}: ${OBJ_P2} libp2${DLL}
	@${ECHO} LINK $@
	@${CC} ${CFLAGS} ${OBJ_P2} -o $@ $(subst potion,p2,${LDEXEFLAGS}) -lp2 ${LIBS}
	@if [ "${DEBUG}" != "1" ]; then \
		${ECHO} STRIP $@; \
	  ${STRIP} $@; \
	fi

libpotion.a: ${OBJ_SYN} ${OBJ}
	@${ECHO} AR $@
	@if [ -e $@ ]; then rm -f $@; fi
	@${AR} rcs $@ ${OBJ_SYN} ${OBJ} > /dev/null

libpotion${DLL}: ${PIC_OBJ_SYN} ${PIC_OBJ}
	@${ECHO} LD $@ -fpic
	@if [ -e $@ ]; then rm -f $@; fi
	@${CC} ${DEBUGFLAGS} -o $@ ${LDDLLFLAGS} \
	  ${PIC_OBJ_SYN} ${PIC_OBJ} > /dev/null

libp2.a: ${OBJ_P2_SYN} $(subst .o,.o2,${OBJ})
	@${ECHO} AR $@
	@if [ -e $@ ]; then rm -f $@; fi
	@${AR} rcs $@ ${OBJ_P2_SYN} $(subst .o,.o2,${OBJ}) > /dev/null

libp2${DLL}: ${PIC_OBJ_P2_SYN} $(subst .opic,.opic2,${PIC_OBJ})
	@${ECHO} LD $@ -fpic
	@if [ -e $@ ]; then rm -f $@; fi
	@${CC} ${DEBUGFLAGS} -o $@ $(subst potion,p2,${LDDLLFLAGS}) \
	  ${PIC_OBJ_P2_SYN} $(subst .opic,.opic2,${PIC_OBJ}) > /dev/null

lib/readline${LOADEXT}: config.inc lib/readline/Makefile \
  lib/readline/linenoise.c lib/readline/linenoise.h
	@${ECHO} MAKE $@ -fpic
	@${MAKE} -s -C lib/readline
	@cp lib/readline/readline${LOADEXT} $@

bench: potion${EXE} test/api/gc-bench${EXE}
	@${ECHO}; \
	  ${ECHO} running GC benchmark; \
	  time test/api/gc-bench

test: test.pn test.p2

test.pn: potion${EXE} \
  test/api/potion-test${EXE} \
  test/api/gc-test${EXE}
	@${ECHO}; \
	${ECHO} running potion API tests; \
	test/api/potion-test; \
	${ECHO} running GC tests; \
	test/api/gc-test; \
	count=0; failed=0; pass=0; \
	while [ $$pass -lt 3 ]; do \
	  ${ECHO}; \
	  if [ $$pass -eq 0 ]; then \
		t=0; \
		${ECHO} running potion VM tests; \
	  elif [ $$pass -eq 1 ]; then \
                t=1; \
		${ECHO} running potion compiler tests; \
	  elif [ $$pass -eq 2 ]; then \
                t=2; \
		${ECHO} running potion JIT tests; \
		jit=`${RUNPOTION} -v | ${SED} "/jit=1/!d"`; \
		if [ "$$jit" = "" ]; then \
		    ${ECHO} skipping; \
		    break; \
		fi; \
	  fi; \
	  for f in test/**/*.pn; do \
		look=`${CAT} $$f | ${SED} "/\#=>/!d; s/.*\#=> //"`; \
		if [ $$t -eq 0 ]; then \
			for=`${RUNPOTION} -I -B $$f | ${SED} "s/\n$$//"`; \
		elif [ $$t -eq 1 ]; then \
			${RUNPOTION} -c $$f > /dev/null; \
			fb="$$f"b; \
			for=`${RUNPOTION} -I -B $$fb | ${SED} "s/\n$$//"`; \
			rm -rf $$fb; \
		else \
			for=`${RUNPOTION} -I -X $$f | ${SED} "s/\n$$//"`; \
		fi; \
		if [ "$$look" != "$$for" ]; then \
			${ECHO}; \
			${ECHO} "$$f: expected <$$look>, but got <$$for>"; \
			failed=`${EXPR} $$failed + 1`; \
		else \
		   ${ECHO} -n .; \
		fi; \
		count=`${EXPR} $$count + 1`; \
	  done; \
	  pass=`${EXPR} $$pass + 1`; \
	done; \
	${ECHO}; \
	if [ $$failed -gt 0 ]; then \
		${ECHO} "$$failed FAILS ($$count tests)"; \
		false; \
	else \
		${ECHO} "OK ($$count tests)"; \
	fi

test.p2: p2${EXE} \
  test/api/p2-test${EXE} \
  test/api/gc-test${EXE}
	@${ECHO}; \
	${ECHO} running p2 API tests; \
	test/api/p2-test; \
	${ECHO} running GC tests; \
	test/api/gc-test; \
	count=0; failed=0; pass=0; \
	while [ $$pass -lt 3 ]; do \
	  ${ECHO}; \
	  if [ $$pass -eq 0 ]; then \
		t=0; \
		${ECHO} running p2 VM tests; \
	  elif [ $$pass -eq 1 ]; then \
                t=1; \
		${ECHO} running p2 compiler tests; \
	  elif [ $$pass -eq 2 ]; then \
                t=2; \
		${ECHO} running p2 JIT tests; \
		jit=`${RUNP2} -v | ${SED} "/jit=1/!d"`; \
		if [ "$$jit" = "" ]; then \
		    ${ECHO} skipping; \
		    break; \
		fi; \
	  fi; \
	  for f in test/**/*.pl; do \
		look=`${CAT} $$f | ${SED} "/\#=>/!d; s/.*\#=> //"`; \
		if [ $$t -eq 0 ]; then \
			for=`${RUNP2} -I -B $$f | ${SED} "s/\n$$//"`; \
		elif [ $$t -eq 1 ]; then \
			${RUNP2} -c $$f > /dev/null; \
			fb="$$f"b; \
			for=`${RUNP2} -I -B $$fb | ${SED} "s/\n$$//"`; \
			rm -rf $$fb; \
		else \
			for=`${RUNP2} -I -X $$f | ${SED} "s/\n$$//"`; \
		fi; \
		if [ "$$look" != "$$for" ]; then \
			${ECHO}; \
			${ECHO} "$$f: expected <$$look>, but got <$$for>"; \
			failed=`${EXPR} $$failed + 1`; \
		else \
		   ${ECHO} -n .; \
		fi; \
		count=`${EXPR} $$count + 1`; \
	  done; \
	  pass=`${EXPR} $$pass + 1`; \
	done; \
	${ECHO}; \
	if [ $$failed -gt 0 ]; then \
		${ECHO} "$$failed FAILS ($$count tests)"; \
		false; \
	else \
		${ECHO} "OK ($$count tests)"; \
	fi

test/api/potion-test${EXE}: ${OBJ_TEST} libpotion${DLL}
	@${ECHO} LINK potion-test
	@${CC} ${CFLAGS} ${OBJ_TEST} ${LIBS} -o $@ -L. -lpotion

test/api/p2-test${EXE}: ${OBJ_P2_TEST} libp2${DLL}
	@${ECHO} LINK p2-test
	@${CC} ${CFLAGS} ${OBJ_P2_TEST} ${LIBS} -o $@ -L. -lp2

test/api/gc-test${EXE}: ${OBJ_GC_TEST} libp2${DLL}
	@${ECHO} LINK gc-test
	@${CC} ${CFLAGS} ${OBJ_GC_TEST} ${LIBS} -o $@ -L. -lp2

test/api/gc-bench${EXE}: ${OBJ_GC_BENCH} libp2${DLL}
	@${ECHO} LINK gc-bench
	@${CC} ${CFLAGS} ${OBJ_GC_BENCH} ${LIBS} -o $@ -L. -lp2

dist: core/config.h core/version.h core/syntax.c core/syntax-p5.c \
  potion${EXE} p2${EXE} \
  libpotion.a libpotion${DLL} libp2.a libp2${DLL} lib/readline${LOADEXT}
	+${MAKE} -f dist.mak $@ PREFIX=${PREFIX} EXE=${EXE} DLL=${DLL} LOADEXT=${LOADEXT}

install: dist
	+${MAKE} -f dist.mak $@ PREFIX=${PREFIX}

tarball:
	+${MAKE} -f dist.mak $@ PREFIX=${PREFIX}

%.html: %.textile
	@${ECHO} DOC $<
	@${ECHO} "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"DTD/xhtml1-transitional.dtd\">" > $@
	@${ECHO} "<html xmlns=\"http://www.w3.org/1999/xhtml\" lang=\"en\" xml:lang=\"en\">" >> $@
	@${ECHO} "<head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />" >> $@
	@${ECHO} "<style type=\"text/css\">@import 'doc.css';</style>" >> $@
	@${ECHO} "<div id='potion'><img src='potion-1.png' /></div>" >> $@
	@${ECHO} "</head><body><div id='central'>" >> $@
	@redcloth $< >> $@
	@${ECHO} "</div></body></html>" >> $@

doc: ${DOCHTML}

TAGS: ${SRC} core/*.h
	@rm -f TAGS
	/usr/bin/find  \( -name \*.c -o -name \*.h \) -exec etags -a --language=c \{\} \;

sloc: clean
	@cp core/syntax.g core/syntax-g.c
	@sloccount core
	@rm -f core/syntax-g.c

todo:
	@grep -rInso 'TODO: \(.\+\)' core

clean:
	@${ECHO} cleaning
	@rm -f core/*.o test/api/*.o ${DOCHTML} \
	       core/*.i core/*.opic core/*.opic2 core/*.o2
	@rm -f ${GREG} tools/*.o core/config.h core/version.h core/syntax.c
	@rm -f lib/readline${LOADEXT} lib/readline/readline${LOADEXT}
	@rm -f test/api/potion-test${EXE} test/api/gc-test${EXE} \
               test/api/gc-bench${EXE}
	@rm -f potion${EXE} libpotion.*
	@rm -f p2${EXE} libp2.* core/syntax-p5.c

.PHONY: all config clean doc rebuild test test.pn test.p2 bench tarball dist install
