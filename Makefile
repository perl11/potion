# posix (linux, bsd, osx, solaris) + mingw with gcc/clang only
.SUFFIXES: .g .c .i .o .opic .textile .html

SRC = core/asm.c core/ast.c core/callcc.c core/compile.c core/contrib.c core/file.c core/gc.c core/internal.c core/lick.c core/load.c core/mt19937ar.c core/number.c core/objmodel.c core/primitive.c core/string.c core/table.c core/vm.c core/vm-ppc.c core/vm-x86.c
SRC_SYN = core/syntax.c
SRC_P2_SYN = core/syntax-p5.c
SRC_POTION = core/potion.c ${SRC_SYN}
SRC_P2 = core/p2.c ${SRC_P2_SYN}
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
AR ?= ar
DEBUG ?= 0
ECHO = /bin/echo
GREG = tools/greg${EXEEXT}
INCS = -Icore
JIT ?= 1
LIBS = -lm -ldl
EXEEXT  =
LIBEXT  = .a
# http://bastard.sourceforge.net/libdisasm.html
ifeq ($(shell ./tools/config.sh ${CC} lib -llibdism libdisasm.h),1)
	DEFINES += -DHAVE_LIBDISASM
	LIBS += -ldisasm
endif
STRIP ?= `./tools/config.sh ${CC} strip`

ifeq (DEBUG,0)
	DEBUGFLAGS += -O2 -fno-stack-protector
else
	DEFINES += -DDEBUG
	DEBUGFLAGS += -g -fstack-protector
endif
CFLAGS += ${DEFINES} ${DEBUGFLAGS}

ifeq ($(shell ./tools/config.sh ${CC} mingw),1)
	EXEEXT  = .exe
	LOADEXT = .dll
	DLLEXT  = .dll
	INCS += -Itools/dlfcn-win32/include
	LIBS += -Ltools/dlfcn-win32/lib
else
  ifeq ($(shell ./tools/config.sh ${CC} apple),1)
	LOADEXT = .dylib
	DLLEXT  = .bundle
  else
	DLLEXT  = .so
	LOADEXT = .so
    ifeq (CC,gcc)
	CFLAGS += -rdynamic
    endif
  endif
endif

all: pn p2${EXEEXT}
	+${MAKE} -s usage

# EXEEXT .exe
# DLLEXT  so/bundle/dll
# LOADEXT so/dylib/dll
pn: potion${EXEEXT} libpotion.a lib/readline${LOADEXT}

rebuild: clean potion${EXEEXT} test

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
	@${ECHO} "#define POTION_CC     \"${CC}\""
	@${ECHO} "#define POTION_CFLAGS \"${CFLAGS}\""
	@${ECHO} "#define POTION_DEBUG  ${DEBUG}"
	@${ECHO} "#define POTION_JIT    ${JIT}"
	@${ECHO} "#define POTION_MAKE   \"${MAKE}\""
	@${ECHO} "#define POTION_PREFIX \"${PREFIX}\""
	@${ECHO} ${DEFINES} | sed "s,-D\(\w*\),\n#define \1 1,g"
	@${ECHO}
	@./tools/config.sh ${CC}

core/version.h: .git/HEAD .git/refs/heads/master
	@${ECHO} -n "#define POTION_DATE   \"" > core/version.h
	@${ECHO} -n $(shell date +%Y-%m-%d) >> core/version.h
	@${ECHO} "\"" >> core/version.h
	@${ECHO} -n "#define POTION_COMMIT \"" >> core/version.h
	@${ECHO} -n $(shell git rev-list HEAD -1 --abbrev=7 --abbrev-commit) >> core/version.h
	@${ECHO} "\"" >> core/version.h
	@${ECHO} -n "#define POTION_REV    " >> core/version.h
	@${ECHO} $(shell git rev-list --abbrev-commit HEAD | wc -l | sed "s/ //g") >> core/version.h
	@${ECHO} >> core/version.h

core/config.h: core/version.h
	@${ECHO} MAKE $@
	@cat core/version.h > core/config.h
	@${MAKE} -s config >> core/config.h

core/callcc.o: core/callcc.c
	@${ECHO} CC $< +frame-pointer
	@${CC} -c -fno-omit-frame-pointer ${INCS} -o $@ $<

core/callcc.opic: core/callcc.c
	@${ECHO} CC $< +frame-pointer
	@${CC} -c -fno-omit-frame-pointer ${INCS} -fPIC -o $@ $<

%.o: %.c core/config.h
	@${ECHO} CC $<
	@${CC} -c ${CFLAGS} ${INCS} -o $@ $<

%.i: %.c core/config.h
	@${ECHO} CPP $@
	@${CC} -c ${CFLAGS} ${INCS} -o $@ -E -c $<

%.opic: %.c core/config.h
	@${ECHO} CC -fPIC $<
	@${CC} -c ${CFLAGS} ${INCS} -fPIC -o $@ $<


.c.o: core/config.h
	@${ECHO} CC $<
	@${CC} -c ${CFLAGS} ${INCS} -o $@ $<

%.c: %.g ${GREG}
	@${ECHO} GREG $<
	@${GREG} $< > $@

.g.c: ${GREG}
	@${ECHO} GREG $<
	@${GREG} $< > $@

${GREG}: tools/greg.c tools/compile.c tools/tree.c
	@${ECHO} CC $@
	@${CC} -O3 -DNDEBUG -o $@ tools/greg.c tools/compile.c tools/tree.c -Itools

potion${EXEEXT}: ${OBJ_POTION} ${OBJ}
	@${ECHO} LINK $@
	@${CC} ${CFLAGS} ${OBJ_POTION} ${OBJ} ${LIBS} -o $@
	@if [ "${DEBUG}" != "1" ]; then \
		${ECHO} STRIP $@; \
	  ${STRIP} $@; \
	fi

p2${EXEEXT}: ${OBJ_P2} ${OBJ}
	@${ECHO} LINK $@
	@${CC} ${CFLAGS} ${OBJ_P2} ${OBJ} ${LIBS} -o $@
	@if [ "${DEBUG}" != "1" ]; then \
		${ECHO} STRIP $@; \
	  ${STRIP} $@; \
	fi

libpotion.a: ${OBJ_SYN} ${OBJ}
	@${ECHO} AR $@
	@if [ -e $@ ]; then rm -f $@; fi
	@${AR} rcs $@ ${OBJ_POTION} ${OBJ} > /dev/null

libpotion${DLLEXT}: core/potion.opic ${PIC_OBJ}
	@${ECHO} LD $@ -fpic
	@if [ -e $@ ]; then rm -f $@; fi
	@${CC} ${DEBUGFLAGS} -shared -fpic -o $@ ${PIC_OBJ_POTION} ${PIC_OBJ} > /dev/null

libp2.a: ${OBJ_P2_SYN} ${OBJ}
	@${ECHO} AR $@
	@if [ -e $@ ]; then rm -f $@; fi
	@${AR} rcs $@ ${OBJ_P2} ${OBJ} > /dev/null

libp2${DLLEXT}: ${PIC_OBJ_P2_SYN} ${PIC_OBJ}
	@${ECHO} LD $@ -fpic
	@if [ -e $@ ]; then rm -f $@; fi
	@${CC} ${DEBUGFLAGS} -shared -fpic -o $@ ${PIC_OBJ_P2} ${PIC_OBJ} > /dev/null


lib/readline${LOADEXT}: libpotion${DLLEXT}
	@${ECHO} MAKE $@ -fpic
	@cd lib/readline; \
	${MAKE} -s readline${LOADEXT} CC=${CC} LOADEXT=${LOADEXT}; \
	cp readline${LOADEXT} ../; \
	cd ../..

bench: potion${EXEEXT} test/api/gc-bench${EXEEXT}
	@${ECHO}; \
	${ECHO} running GC benchmark; \
	time test/api/gc-bench

test: potion${EXEEXT} p2${EXEEXT} \
  test/api/p2-test${EXEEXT} test/api/potion-test${EXEEXT} \
  test/api/gc-test${EXEEXT}
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
                cmd=p2; t=0; \
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
			failed=`expr $$failed + 1`; \
		else \
			${ECHO} -n .; \
		fi; \
		count=`expr $$count + 1`; \
	  done; \
	  pass=`expr $$pass + 1`; \
	done; \
	${ECHO}; \
	if [ $$failed -gt 0 ]; then \
		${ECHO} "$$failed FAILS ($$count tests)"; \
	else \
		${ECHO} "OK ($$count tests)"; \
	fi

test/api/potion-test${EXEEXT}: ${OBJ_TEST} ${OBJ} ${OBJ_SYN}
	@${ECHO} LINK potion-test
	${CC} ${CFLAGS} ${OBJ_TEST} ${OBJ} ${OBJ_SYN} ${LIBS} -o $@

test/api/p2-test: ${OBJ_P2_TEST} ${OBJ_P2} ${OBJ_P2_SYN}
	@${ECHO} LINK p2-test
	${CC} ${CFLAGS} ${OBJ_P2_TEST} ${OBJ} ${OBJ_P2_SYN} ${LIBS} -o $@

test/api/gc-test${EXEEXT}: ${OBJ_GC_TEST} ${OBJ} ${OBJ_SYN}
	@${ECHO} LINK gc-test
	@${CC} ${CFLAGS} ${OBJ_GC_TEST} ${OBJ} ${OBJ_SYN} ${LIBS} -o $@

test/api/gc-bench${EXEEXT}: ${OBJ_GC_BENCH} ${OBJ} ${OBJ_SYN}
	@${ECHO} LINK gc-bench
	@${CC} ${CFLAGS} ${OBJ_GC_BENCH} ${OBJ} ${OBJ_SYN} ${LIBS} -o $@

dist: libp2.a libpotion${DLLEXT} libp2${DLLEXT}
	+${MAKE} -f dist.mak $@ PREFIX=${PREFIX}

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
	@rm -f ${OBJ} ${OBJ_POTION} ${OBJ_TEST} ${OBJ_GC_TEST} ${OBJ_GC_BENCH} ${DOCHTML} core/*.opic
	@rm -f tools/greg${EXEEXT} tools/greg.o tools/compile.o tools/tree.o
	@rm -f core/config.h core/version.h core/syntax.c
	@rm -f potion${EXEEXT} p2${EXEEXT} libpotion.* libp2.* \
	  test/api/potion-test${EXEEXT} test/api/potion-test${EXEEXT} \
	  test/api/gc-test${EXEEXT} test/api/gc-bench${EXEEXT}

.PHONY: all config clean doc rebuild test bench tarball sloc todo
