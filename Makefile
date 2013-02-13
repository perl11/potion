# posix (linux, bsd, osx, solaris) + mingw with gcc/clang only
.SUFFIXES: .g .c .i .o .opic .textile .html

SRC = core/asm.c core/ast.c core/callcc.c core/compile.c core/contrib.c core/file.c core/gc.c core/internal.c core/lick.c core/load.c core/mt19937ar.c core/number.c core/objmodel.c core/primitive.c core/string.c core/syntax.c core/table.c core/vm.c

# bootstrap config.inc with make -f config.mak
include config.inc

ifeq (${JIT},1)
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
endif
OBJ = ${SRC:.c=.o}
PIC_OBJ = ${SRC:.c=.opic}
OBJ_POTION = core/potion.o
OBJ_TEST = test/api/potion-test.o test/api/CuTest.o
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

all: pn
	+${MAKE} -s usage

pn: potion${EXE} libpotion.a lib/readline${LOADEXT}

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
	@${ECHO} " Maintained at https://github.com/fogus/potion"

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

core/callcc.o: core/callcc.c
	@${ECHO} CC $< +frame-pointer
	@${CC} -c -fno-omit-frame-pointer ${INCS} -o $@ $<

core/callcc.opic: core/callcc.c
	@${ECHO} CC -fPIC $< +frame-pointer
	@${CC} -c -fPIC -fno-omit-frame-pointer ${INCS} -o $@ $<

core/vm.o core/vm.opic: core/vm-dis.c

%.i: %.c core/config.h
	@${ECHO} CPP $@
	@${CC} -c ${CFLAGS} ${INCS} -o $@ -E -c $<
%.o: %.c core/config.h
	@${ECHO} CC $<
	@${CC} -c ${CFLAGS} ${INCS} -o $@ $<
%.opic: %.c core/config.h
	@${ECHO} CC -fPIC $<
	@${CC} -c -fPIC ${CFLAGS} ${INCS} -o $@ $<

.c.i: core/config.h
	@${ECHO} CPP $@
	@${CC} -c ${CFLAGS} ${INCS} -o $@ -E -c $<
.c.o: core/config.h
	@${ECHO} CC $<
	@${CC} -c ${CFLAGS} ${INCS} -o $@ $<
.c.opic: core/config.h
	@${ECHO} CC -fPIC $<
	@${CC} -c -fPIC ${CFLAGS} ${INCS} -o $@ $<

%.c: %.g ${GREG}
	@${ECHO} GREG $<
	@${GREG} $< > $@
.g.c: ${GREG}
	@${ECHO} GREG $<
	@${GREG} $< > $@

${GREG}: tools/greg.c tools/compile.c tools/tree.c
	@${ECHO} CC $@
	@${CC} -O3 -DNDEBUG -o $@ tools/greg.c tools/compile.c tools/tree.c -Itools

ifdef APPLE
LIBHACK = ../lib/libpotion.dylib
else
LIBHACK =
endif
../lib/libpotion.dylib:
	-mkdir ../lib
	-ln -s `pwd`/libpotion.dylib ../lib/

potion${EXE}: ${OBJ_POTION} libpotion${DLL} ${LIBHACK}
	@${ECHO} LINK $@
	@${CC} ${CFLAGS} ${OBJ_POTION} -o $@ ${LDEXEFLAGS} -lpotion ${LIBS}
	@if [ "${DEBUG}" != "1" ]; then \
		${ECHO} STRIP $@; \
	  ${STRIP} $@; \
	fi

libpotion.a: ${OBJ}
	@${ECHO} AR $@
	@if [ -e $@ ]; then rm -f $@; fi
	@${AR} rcs $@ core/*.o > /dev/null

libpotion${DLL}: ${PIC_OBJ}
	@${ECHO} LD $@ -fpic
	@if [ -e $@ ]; then rm -f $@; fi
	@${CC} ${DEBUGFLAGS} -o $@ ${LDDLLFLAGS} \
	  ${PIC_OBJ} > /dev/null

lib/readline${LOADEXT}: config.inc lib/readline/Makefile lib/readline/linenoise.c \
  lib/readline/linenoise.h
	@${ECHO} MAKE $@ -fpic
	@${MAKE} -s -C lib/readline
	@cp lib/readline/readline${LOADEXT} $@

bench: potion${EXE} test/api/gc-bench${EXE}
	@${ECHO}; \
	  ${ECHO} running GC benchmark; \
	  time test/api/gc-bench

test: potion${EXE} test/api/potion-test${EXE} test/api/gc-test${EXE}
	@${ECHO}; \
	${ECHO} running API tests; \
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
		   ${ECHO} running VM tests; \
	  elif [ $$pass -eq 1 ]; then \
		   ${ECHO} running compiler tests; \
		else \
		   ${ECHO} running JIT tests; \
			 jit=`${RUNPOTION} -v | ${SED} "/jit=1/!d"`; \
			 if [ "$$jit" = "" ]; then \
			   ${ECHO} skipping; \
			   break; \
			 fi; \
		fi; \
		for f in test/**/*.pn; do \
			look=`${CAT} $$f | ${SED} "/\#=>/!d; s/.*\#=> //"`; \
			if [ $$pass -eq 0 ]; then \
				for=`${RUNPOTION} -I -B $$f | ${SED} "s/\n$$//"`; \
			elif [ $$pass -eq 1 ]; then \
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

test/api/potion-test${EXE}: ${OBJ_TEST} ${OBJ}
	@${ECHO} LINK potion-test
	@${CC} ${CFLAGS} ${OBJ_TEST} ${OBJ} ${LIBS} -o $@

test/api/gc-test${EXE}: ${OBJ_GC_TEST} ${OBJ}
	@${ECHO} LINK gc-test
	@${CC} ${CFLAGS} ${OBJ_GC_TEST} ${OBJ} ${LIBS} -o $@

test/api/gc-bench${EXE}: ${OBJ_GC_BENCH} ${OBJ}
	@${ECHO} LINK gc-bench
	@${CC} ${CFLAGS} ${OBJ_GC_BENCH} ${OBJ} ${LIBS} -o $@

dist: core/config.h core/version.h core/syntax.c potion${EXE}  \
  libpotion.a libpotion${DLL} lib/readline${LOADEXT}
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
	@rm -f core/*.o core/*.opic core/*.i test/api/*.o ${DOCHTML}
	@rm -f ${GREG} tools/*.o
	@rm -f core/config.h core/version.h core/syntax.c config.inc
	@rm -f potion${EXE} libpotion.* \
	  test/api/potion-test${EXE} test/api/gc-test${EXE} test/api/gc-bench${EXE}

.PHONY: all config clean doc rebuild test bench tarball dist install
