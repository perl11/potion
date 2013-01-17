# posix (linux, bsd, osx, solaris) + mingw with gcc/clang only
.SUFFIXES: .g .c .i .o .opic .textile .html

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

PREFIX = /usr/local
CC ?= gcc
CFLAGS = -Wall -fno-strict-aliasing -Wno-return-type -D_GNU_SOURCE
LDEXEFLAGS = -L. -Wl,-rpath=. -Wl,-rpath=../lib
LDDLLFLAGS = -shared -fpic
INCS = -Icore
LIBS = -lm -ldl
AR ?= ar
JIT ?= 1
DEBUG ?= 0
ECHO = /bin/echo
CAT  = /bin/cat
SED  = sed
EXPR = expr
GREG = tools/greg${EXE}
EXE  =

# http://bastard.sourceforge.net/libdisasm.html
ifeq ($(shell ./tools/config.sh ${CC} lib -llibdism libdisasm.h),1)
	DEFINES += -DHAVE_LIBDISASM
	LIBS += -ldisasm
endif
STRIP ?= `./tools/config.sh ${CC} strip`

ifeq (${DEBUG},0)
	DEBUGFLAGS += -O2 -fno-stack-protector
else
	DEFINES += -DDEBUG
	DEBUGFLAGS += -g -fstack-protector
endif
ifneq ($(shell ./tools/config.sh ${CC} clang),0)
	CLANG = 1
	CFLAGS += -Wno-unused-value
endif
CFLAGS += ${DEFINES} ${DEBUGFLAGS}

ifeq ($(shell ./tools/config.sh ${CC} mingw),1)
        # cygwin is NOT win32
        WIN32   = 1
	EXE  = .exe
	LOADEXT = .dll
	DLL  = .dll
	INCS += -Itools/dlfcn-win32/include
	LIBS += -Ltools/dlfcn-win32/lib
	RUNPOTION = potion.exe
	RUNP2 = p2.exe
else
  ifeq ($(shell ./tools/config.sh ${CC} apple),1)
        APPLE   = 1
	DLL      = .dylib
	LOADEXT  = .bundle
	RUNPOTION = DYLD_LIBRARY_PATH=`pwd` ./potion
	RUNP2 = DYLD_LIBRARY_PATH=`pwd` ./p2
# in builddir: mkdir ../lib; ln -s `pwd`/libpotion.dylib ../lib/
	LDDLLFLAGS = -shared -fpic -install_name "@executable_path/../lib/libpotion${DLL}"
	LDEXEFLAGS = -L.
  else
	RUNPOTION = ./potion
	RUNP2 = ./p2
	DLL  = .so
	LOADEXT = .so
    ifeq (${CC},gcc)
	CFLAGS += -rdynamic
    endif
  endif
endif

all: pn p2${EXE} libp2.a
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

core/version.h: .git/HEAD .git/refs/heads/p2
	@${ECHO} MAKE $@
	@${ECHO} "# -*- makefile -*-" > config.inc
	@${ECHO} -n "#define POTION_DATE   \"" > core/version.h
	@${ECHO} -n $(shell date +%Y-%m-%d) >> core/version.h
	@${ECHO} "\"" >> core/version.h
	@${ECHO} -n "#define POTION_COMMIT \"" >> core/version.h
	@${ECHO} -n $(shell git rev-list HEAD -1 --abbrev=7 --abbrev-commit) >> core/version.h
	@${ECHO} "\"" >> core/version.h
	@${ECHO} -n "#define POTION_REV    " >> core/version.h
	@${ECHO} -n $(shell git rev-list --abbrev-commit HEAD | wc -l | ${SED} "s/ //g") >> core/version.h
	@${ECHO} >> core/version.h

config:
	@${MAKE} -s -B core/config.h

config.inc.echo:
	@${ECHO} "PREFIX  = ${PREFIX}"
	@${ECHO} "EXE  = ${EXE}"
	@${ECHO} "DLL  = ${DLL}"
	@${ECHO} "LOADEXT = ${LOADEXT}"
	@${ECHO} "ECHO    = ${ECHO}"
	@${ECHO} "CC      = ${CC}"
	@${ECHO} "CFLAGS  = ${CFLAGS}"
	@${ECHO} "JIT     = ${JIT}"
	@${ECHO} "LIBS    = ${LIBS}"
	@${ECHO} "DEFINES = ${DEFINES}"
	@${ECHO} "DEBUGFLAGS = ${DEBUGFLAGS}"
	@${ECHO} "STRIP   = ${STRIP}"
	@${ECHO} "APPLE   = ${APPLE}"
	@${ECHO} "WIN32   = ${WIN32}"
	@${ECHO} "CLANG   = ${CLANG}"
	@${ECHO} "DEBUG   = ${DEBUG}"
	@${ECHO} -n "REVISION = "
	@${ECHO} $(shell git rev-list --abbrev-commit HEAD | wc -l | ${SED} "s/ //g")

config.h.echo:
	@${ECHO} "#define POTION_CC     \"${CC}\""
	@${ECHO} "#define POTION_CFLAGS \"${CFLAGS}\""
	@${ECHO} "#define POTION_DEBUG  ${DEBUG}"
	@${ECHO} "#define POTION_JIT    ${JIT}"
	@${ECHO} "#define POTION_MAKE   \"${MAKE}\""
	@${ECHO} "#define POTION_PREFIX \"${PREFIX}\""
	@${ECHO} ${DEFINES} | ${SED} "s,-D\(\w*\),\n#define \1 1,g"
	@${ECHO}
	@./tools/config.sh ${CC}

# Force sync with config.inc
core/config.h: core/version.h tools/config.sh #Makefile
	@${ECHO} MAKE $@
	@${MAKE} -s -B config.inc
	@${CAT} core/version.h > core/config.h
	@${MAKE} -s config.h.echo >> core/config.h

config.inc: tools/config.sh #Makefile
	@${ECHO} MAKE $@
	@${MAKE} -s config.inc.echo > $@

core/callcc.o: core/callcc.c
	@${ECHO} CC $< +frame-pointer
	@${CC} -c -fno-omit-frame-pointer ${INCS} -o $@ $<

core/callcc.opic: core/callcc.c
	@${ECHO} CC -fPIC $< +frame-pointer
	@${CC} -c -fPIC -fno-omit-frame-pointer ${INCS} -o $@ $<

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

potion${EXE}: ${OBJ_POTION} libpotion${DLL}
	@${ECHO} LINK $@
	@${CC} ${CFLAGS} ${OBJ_POTION} -o $@ ${LDEXEFLAGS} -lpotion ${LIBS}
	@if [ "${DEBUG}" != "1" ]; then \
		${ECHO} STRIP $@; \
	  ${STRIP} $@; \
	fi

p2${EXE}: ${OBJ_P2} libp2${DLL}
	@${ECHO} LINK $@
	${CC} ${CFLAGS} ${OBJ_P2} -o $@ $(subst potion,p2,${LDEXEFLAGS}) -lp2 ${LIBS}
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
	@${CC} ${DEBUGFLAGS} -shared -fpic -o $@ ${LDDLLFLAGS} \
	  ${PIC_OBJ_SYN} ${PIC_OBJ} > /dev/null

libp2.a: ${OBJ_P2_SYN} ${OBJ}
	@${ECHO} AR $@
	@if [ -e $@ ]; then rm -f $@; fi
	@${AR} rcs $@ ${OBJ_P2_SYN} ${OBJ} > /dev/null

libp2${DLL}: ${PIC_OBJ_P2_SYN} ${PIC_OBJ}
	@${ECHO} LD $@ -fpic
	@if [ -e $@ ]; then rm -f $@; fi
	@${CC} ${DEBUGFLAGS} -o $@ ${LDDLLFLAGS} \
	  ${PIC_OBJ_P2_SYN} ${PIC_OBJ} > /dev/null

lib/readline${LOADEXT}: config.inc lib/readline/Makefile \
  lib/readline/linenoise.c lib/readline/linenoise.h
	@${ECHO} MAKE $@ -fpic
	@${MAKE} -s -C lib/readline
	@cp lib/readline/readline${LOADEXT} $@

bench: potion${EXE} test/api/gc-bench${EXE}
	@${ECHO}; \
	  ${ECHO} running GC benchmark; \
	  time test/api/gc-bench

test: potion${EXE} p2${EXE} libpotion.a libp2.a \
  test/api/p2-test${EXE} test/api/potion-test${EXE} \
  test/api/gc-test${EXE}
	@${ECHO}; \
	${ECHO} running potion API tests; \
	test/api/potion-test; \
	${ECHO} running p2 API tests; \
	test/api/p2-test; \
	${ECHO} running GC tests; \
	test/api/gc-test; \
	count=0; failed=0; pass=0; cmd="potion"; \
	while [ $$pass -lt 6 ]; do \
	  ${ECHO}; \
	  if [ $$pass -eq 0 ]; then \
		t=0; \
		${ECHO} running $$cmd VM tests; \
	  elif [ $$pass -eq 1 ]; then \
                t=1; \
		${ECHO} running $$cmd compiler tests; \
	  elif [ $$pass -eq 2 ]; then \
                t=2; \
		${ECHO} running $$cmd JIT tests; \
		jit=`./$$cmd -v | sed "/jit=1/!d"`; \
		if [ "$$jit" = "" ]; then \
		    ${ECHO} skipping; \
		    break; \
		fi; \
	  elif [ $$pass -eq 3 ]; then \
                cmd="p2"; t=0; \
		${ECHO} running $$cmd VM tests; \
	  elif [ $$pass -eq 4 ]; then \
                t=1; \
		${ECHO} running $$cmd compiler tests; \
	  elif [ $$pass -eq 5 ]; then \
                t=2; \
		${ECHO} running $$cmd JIT tests; \
		jit=`./$$cmd -v | sed "/jit=1/!d"`; \
		if [ "$$jit" = "" ]; then \
		    ${ECHO} skipping; \
		    break; \
		fi; \
	  fi; \
	  for f in test/**/*.pn; do \
		look=`cat $$f | sed "/\#/!d; s/.*\# //"`; \
		if [ $$t -eq 0 ]; then \
			for=`./$$cmd -I -B $$f | sed "s/\n$$//"`; \
		elif [ $$t -eq 1 ]; then \
			./$$cmd -c $$f > /dev/null; \
			fb="$$f"b; \
			for=`./$$cmd -I -B $$fb | sed "s/\n$$//"`; \
			rm -rf $$fb; \
		else \
			for=`./$$cmd -I -X $$f | sed "s/\n$$//"`; \
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
	else \
		${ECHO} "OK ($$count tests)"; \
	fi

test/api/potion-test${EXE}: ${OBJ_TEST} ${OBJ} ${OBJ_SYN}
	@${ECHO} LINK potion-test
	${CC} ${CFLAGS} ${OBJ_TEST} ${OBJ} ${OBJ_SYN} ${LIBS} -o $@

test/api/p2-test${EXE}: ${OBJ_P2_TEST} ${OBJ} ${OBJ_P2_SYN}
	@${ECHO} LINK p2-test
	@${CC} ${CFLAGS} ${OBJ_P2_TEST} ${OBJ} ${OBJ_P2_SYN} ${LIBS} -o $@

test/api/gc-test${EXE}: ${OBJ_GC_TEST} ${OBJ} ${OBJ_SYN}
	@${ECHO} LINK gc-test
	@${CC} ${CFLAGS} ${OBJ_GC_TEST} ${OBJ} ${OBJ_SYN} ${LIBS} -o $@

test/api/gc-bench${EXE}: ${OBJ_GC_BENCH} ${OBJ} ${OBJ_SYN}
	@${ECHO} LINK gc-bench
	@${CC} ${CFLAGS} ${OBJ_GC_BENCH} ${OBJ} ${OBJ_SYN} ${LIBS} -o $@

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
	       core/*.i core/*.opic
	@rm -f ${GREG} tools/*.o core/config.h core/version.h core/syntax.c
	@rm -f lib/readline${LOADEXT} lib/readline/readline${LOADEXT}
	@rm -f test/api/potion-test${EXE} test/api/gc-test${EXE} \
               test/api/gc-bench${EXE}
	@rm -f potion${EXE} libpotion.*
	@rm -f p2${EXE} libp2.* core/syntax-p5.c

.PHONY: all config config.inc.echo config.h.echo clean doc rebuild test bench tarball dist install
