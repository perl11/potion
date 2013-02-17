# posix (linux, bsd, osx, solaris) + mingw with gcc/clang only
.SUFFIXES: .g .c .i .i2 .o .opic .o2 .opic2 .textile .html

SRC = core/asm.c core/ast.c core/callcc.c core/compile.c core/contrib.c core/file.c core/gc.c core/internal.c core/lick.c core/load.c core/mt19937ar.c core/number.c core/objmodel.c core/primitive.c core/string.c core/table.c core/vm.c

# bootstrap config.inc with make -f config.mak
include config.inc

ifeq (${JIT_X86},1)
SRC += core/vm-x86.c
else
ifeq (${JIT_PPC},1)
SRC += core/vm-ppc.c
endif
ifeq (${JIT_ARM},1)
SRC += core/vm-arm.c # not yet ready
endif
endif

SRC_SYN    = syn/syntax.c
SRC_P2_SYN = syn/syntax-p5.c
SRC_POTION = front/potion.c
SRC_P2     = front/p2.c

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
DOC = doc/start.textile doc/p2-extensions.textile doc/glossary.textile doc/design-decisions.textile
DOCHTML = ${DOC:.textile=.html}

CAT  = /bin/cat
ECHO = /bin/echo
SED  = sed
EXPR = expr
GREG = syn/greg${EXE}

INCS = -Icore
RUNPOTION = ./potion
RUNP2 = ./p2

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

core/version.h: $(shell git show-ref HEAD | ${SED} "s,^.* ,.git/,g")
	@${MAKE} -s -f config.mak $@

# bootstrap config.inc
config.inc: tools/config.sh config.mak
	@${ECHO} MAKE -f config.mak $@
	@${MAKE} -s -f config.mak $@

DEFS = -Wall -fno-strict-aliasing -Wno-return-type -D_GNU_SOURCE
core/callcc.o core/callcc.o2: core/callcc.c
	@${ECHO} CC $< +frame-pointer
	@${CC} -c ${DEFS} -fno-omit-frame-pointer ${INCS} -o $@ $<

core/callcc.opic core/callcc.opic2: core/callcc.c
	@${ECHO} CC -fPIC $< +frame-pointer
	@${CC} -c ${DEFS} -fPIC -fno-omit-frame-pointer ${INCS} -o $@ $<

core/vm.o core/vm.opic: core/vm-dis.c

# no optimizations
#core/vm-x86.opic: core/vm-x86.c
#	@${ECHO} CC -fPIC $< +frame-pointer
#	@${CC} -c -g3 -fstack-protector -fno-omit-frame-pointer -Wall -fno-strict-aliasing -Wno-return-type# -D_GNU_SOURCE -fPIC ${INCS} -o $@ $<

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

# the installed version assumes bin/potion loading from ../lib/libpotion (relocatable)
# on darwin we generate a parallel p2/../lib to use @executable_path/../lib/libpotion
ifdef APPLE
LIBHACK = ../lib/libpotion.dylib ../lib/libp2.dylib
else
LIBHACK =
endif
../lib/libpotion.dylib:
	-mkdir ../lib
	-ln -s `pwd`/libpotion.dylib ../lib/
../lib/libp2.dylib:
	-mkdir ../lib
	-ln -s `pwd`/libp2.dylib ../lib/

potion${EXE}: ${OBJ_POTION} libpotion${DLL} ${LIBHACK}
	@${ECHO} LINK $@
	@${CC} ${CFLAGS} ${OBJ_POTION} -o $@ ${LDEXEFLAGS} -lpotion ${LIBS}
	@if [ "${DEBUG}" != "1" ]; then \
		${ECHO} STRIP $@; \
	  ${STRIP} $@; \
	fi

p2${EXE}: ${OBJ_P2} libp2${DLL} ${LIBHACK}
	@${ECHO} LINK $@
	@${CC} ${CFLAGS} ${OBJ_P2} -o $@ $(subst potion,p2,${LDEXEFLAGS}) -lp2 ${LIBS}
	@if [ "${DEBUG}" != "1" ]; then \
		${ECHO} STRIP $@; \
	  ${STRIP} $@; \
	fi

${GREG}: syn/greg.c syn/compile.c syn/tree.c
	@${ECHO} CC $@
	@${CC} -O3 -DNDEBUG -o $@ syn/greg.c syn/compile.c syn/tree.c -Isyn

libpotion.a: ${OBJ_SYN} ${OBJ}
	@${ECHO} AR $@
	@if [ -e $@ ]; then rm -f $@; fi
	@${AR} rcs $@ ${OBJ_SYN} ${OBJ} > /dev/null

libpotion${DLL}: ${PIC_OBJ_SYN} ${PIC_OBJ}
	@${ECHO} LD $@ -fpic
	@if [ -e $@ ]; then rm -f $@; fi
	@${CC} ${DEBUGFLAGS} -o $@ ${LDDLLFLAGS} \
	  ${PIC_OBJ_SYN} ${PIC_OBJ} ${LIBS} > /dev/null

libp2.a: ${OBJ_P2_SYN} $(subst .o,.o2,${OBJ})
	@${ECHO} AR $@
	@if [ -e $@ ]; then rm -f $@; fi
	@${AR} rcs $@ ${OBJ_P2_SYN} $(subst .o,.o2,${OBJ}) > /dev/null

libp2${DLL}: ${PIC_OBJ_P2_SYN} $(subst .opic,.opic2,${PIC_OBJ})
	@${ECHO} LD $@ -fpic
	@if [ -e $@ ]; then rm -f $@; fi
	@${CC} ${DEBUGFLAGS} -o $@ $(subst potion,p2,${LDDLLFLAGS}) \
	  ${PIC_OBJ_P2_SYN} $(subst .opic,.opic2,${PIC_OBJ}) ${LIBS} > /dev/null

lib/readline${LOADEXT}: config.inc lib/readline/Makefile \
  lib/readline/linenoise.c lib/readline/linenoise.h
	@${ECHO} MAKE $@ -fpic
	@${MAKE} -s -C lib/readline
	@cp lib/readline/readline${LOADEXT} $@

bench: potion${EXE} test/api/gc-bench${EXE}
	@${ECHO}; \
	  ${ECHO} running GC benchmark; \
	  time test/api/gc-bench

check: test.pn test.p2

test: test.pn test.p2

test.pn: potion${EXE} test/api/potion-test${EXE} test/api/gc-test${EXE}
	@${ECHO}; \
	${ECHO} running potion API tests; \
	LD_LIBRARY_PATH=`pwd`:$LD_LIBRARY_PATH \
	DYLD_LIBRARY_PATH=`pwd`:$DYLD_LIBRARY_PATH \
	export LD_LIBRARY_PATH; export DYLD_LIBRARY_PATH; \
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

test.p2: p2${EXE} test/api/p2-test${EXE} test/api/gc-test${EXE}
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
		look="P2"; \
		if [ $$t -eq 0 ]; then \
			for=`${RUNP2} --inspect -B $$f | ${SED} "s/\n$$//"`; \
		elif [ $$t -eq 1 ]; then \
			${RUNP2} --compile $$f > /dev/null; \
			fb="$$f"b; \
			for=`${RUNP2} --inspect -B $$fb | ${SED} "s/\n$$//"`; \
			rm -rf $$fb; \
		else \
			for=`${RUNP2} --inspect -J $$f | ${SED} "s/\n$$//"`; \
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

test/api/potion-test${EXE}: ${OBJ_TEST} libpotion.a
	@${ECHO} LINK potion-test
	@${CC} ${CFLAGS} ${OBJ_TEST} -o $@ libpotion.a ${LIBS}

test/api/gc-test${EXE}: ${OBJ_GC_TEST} libp2.a
	@${ECHO} LINK gc-test
	@${CC} ${CFLAGS} ${OBJ_GC_TEST} -o $@ libp2.a ${LIBS}

test/api/gc-bench${EXE}: ${OBJ_GC_BENCH} libp2.a
	@${ECHO} LINK gc-bench
	@${CC} ${CFLAGS} ${OBJ_GC_BENCH} -o $@ libp2.a ${LIBS}

test/api/p2-test${EXE}: ${OBJ_P2_TEST} libp2.a
	@${ECHO} LINK p2-test
	@${CC} ${CFLAGS} ${OBJ_P2_TEST} -o $@ libp2.a ${LIBS}

dist: core/config.h core/version.h ${SRC_SYN} ${SRC_P2_SYN} \
  potion${EXE} p2${EXE} \
  libpotion.a libpotion${DLL} libp2.a libp2${DLL} lib/readline${LOADEXT}
	+${MAKE} -f dist.mak $@ PREFIX=${PREFIX} EXE=${EXE} DLL=${DLL} LOADEXT=${LOADEXT}

install: dist
	+${MAKE} -f dist.mak $@ PREFIX=${PREFIX}

tarball:
	+${MAKE} -f dist.mak $@ PREFIX=${PREFIX}

release:
	+${MAKE} -f dist.mak $@ PREFIX=${PREFIX}

%.html: %.textile doc/logo
	@${ECHO} DOC $@
	@${ECHO} "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"DTD/xhtml1-transitional.dtd\">" > $@
	@${ECHO} "<html xmlns=\"http://www.w3.org/1999/xhtml\" lang=\"en\" xml:lang=\"en\">" >> $@
	@${ECHO} "<head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />" >> $@
	@${ECHO} "<style type=\"text/css\">@import 'doc.css';</style>" >> $@
	@${ECHO} "</head><body>" >> $@
	@${CAT} doc/logo >> $@
	@${ECHO} "<div id='central'>" >> $@
	@redcloth $< >> $@
	@${ECHO} "</div></body></html>" >> $@

doc: ${DOCHTML}

TAGS: ${SRC} core/*.h
	@rm -f TAGS
	/usr/bin/find  \( -name \*.c -o -name \*.h \) -exec etags -a --language=c \{\} \;

sloc: clean
	@cp syn/syntax.g syn/syntax-g.c
	@sloccount core syn front
	@rm -f syn/syntax-g.c

todo:
	@grep -rInso 'TODO: \(.\+\)' core syn front

clean:
	@${ECHO} cleaning
	@rm -f core/*.o test/api/*.o front/*.o syn/*.i syn/*.o syn/*.opic \
	       core/*.i core/*.opic core/*.opic2 core/*.o2
	@rm -f ${DOCHTML}
	@rm -f ${GREG} tools/*.o core/config.h core/version.h ${SRC_SYN}
	@rm -f tools/*~ doc/*~ example/*~
	@rm -f lib/readline${LOADEXT} lib/readline/readline${LOADEXT}
	@rm -f test/api/potion-test${EXE} test/api/gc-test${EXE} \
               test/api/gc-bench${EXE}
	@rm -f potion${EXE} libpotion.*
	@rm -f test/api/p2-test${EXE}
	@rm -f p2${EXE} libp2.* ${SRC_P2_SYN}

realclean: clean
	@rm -f config.inc

.PHONY: all config clean doc rebuild check test test.pn test.p2 bench tarball dist release install
