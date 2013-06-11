# posix (linux, bsd, osx, solaris) + mingw with gcc/clang only
.SUFFIXES: .y .c .i .i2 .o .opic .o2 .opic2 .textile .html
.PHONY: all bins libs pn p2 static default config clean doc rebuild check test test.pn test.p2 \
	bench tarball dist release install grammar doxygen website

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

FPIC =
OPIC = o
ifneq (${WIN32},1)
  ifneq (${CYGWIN},1)
    FPIC = -fPIC
    OPIC = opic
  endif
endif
OBJ = ${SRC:.c=.o}
OBJ2 = ${SRC:.c=.o2}
OBJ_SYN = ${SRC_SYN:.c=.o}
OBJ_POTION = ${SRC_POTION:.c=.o}
OBJ_P2 = ${SRC_P2:.c=.${OPIC}2}
OBJ_P2_SYN = ${SRC_P2_SYN:.c=.o2}
PIC_OBJ = ${SRC:.c=.${OPIC}}
PIC_OBJ_SYN = ${SRC_SYN:.c=.${OPIC}}
PIC_OBJ_POTION = ${SRC_POTION:.c=.${OPIC}}
PIC_OBJ_P2 = ${SRC_P2:.c=.${OPIC}2}
PIC_OBJ_P2_SYN = ${SRC_P2_SYN:.c=.${OPIC}2}
OBJ_TEST = test/api/potion-test.o test/api/CuTest.o
OBJ_P2_TEST = test/api/p2-test.o test/api/CuTest.o
OBJ_GC_TEST = test/api/gc-test.o test/api/CuTest.o
OBJ_GC_BENCH = test/api/gc-bench.o
DOC = doc/start.textile doc/p2-extensions.textile doc/glossary.textile doc/design-decisions.textile
DOCHTML = ${DOC:.textile=.html}
BINS = bin/potion${EXE} bin/p2${EXE}
PLIBS = $(foreach l,potion p2,lib/lib$l${DLL})
PLIBS += $(foreach s,syntax syntax-p5,lib/potion/lib$s${DLL})
#EXTLIBS = $(foreach m,uv pcre,lib/lib$m.a)
#EXTLIBS = -L3rd/pcre -lpcre -L3rd/libuv -luv
DYNLIBS = $(foreach m,readline buffile,lib/potion/$m${LOADEXT})
OBJS = .o .o2
ifneq (${FPIC},)
  OBJS += ${OPIC} ${OPIC}2
endif

GREGCFLAGS = -O3 -DNDEBUG
CAT  = /bin/cat
ECHO ?= /bin/echo
MV   = /bin/mv
SED  = sed
EXPR = expr
GREG = syn/greg${EXE}
RANLIB ?= ranlib

RUNPRE = bin/
# perl11.org only
WEBSITE = ../perl11.org

default: pn p2
	+${MAKE} -s usage

all: default libs static doc test
bins: ${BINS}
libs: ${PLIBS} ${DYNLIBS}
pn: bin/potion${EXE} libs
p2: bin/p2${EXE} libs
static: lib/libpotion.a bin/potion-s${EXE} lib/libp2.a bin/p2-s${EXE}

rebuild: clean pn p2 test

usage:
	@${ECHO} " "
	@${ECHO} " ~ using p2 ~ (same as perl)"
	@${ECHO} " "
	@${ECHO} " Running a script or code."
	@${ECHO} " "
	@${ECHO} "   $$ p2 example/fib.pl"
	@${ECHO} "   $$ p2 -e \"code\""
	@${ECHO} " "
	@${ECHO} " Dump the AST and bytecode inspection for a script. "
	@${ECHO} " "
	@${ECHO} "   $$ p2 --verbose example/fib.pl"
	@${ECHO} " "
	@${ECHO} " Compiling to bytecode."
	@${ECHO} " "
	@${ECHO} "   $$ p2 --compile example/fib.pl"
	@${ECHO} "   $$ p2 example/fib.plc"
	@${ECHO} " "
	@${ECHO} " Potion builds its JIT compiler by default, but"
	@${ECHO} " you can use the bytecode VM by running scripts"
	@${ECHO} " with the -B or --bytecode flag."
	@${ECHO} " If you built with JIT=0, then the bytecode VM"
	@${ECHO} " will run by default."
	@${ECHO} " "
	@${ECHO} " To verify your build,"
	@${ECHO} " "
	@${ECHO} "   $$ make test"
	@${ECHO} " "
	@${ECHO} " Written by _why and rurban."
	@${ECHO} " Maintained at https://github.com/perl11/potion, branch p2"

config:
	@${ECHO} MAKE -f config.mak $@
	@${MAKE} -s -f config.mak config.inc core/config.h

# bootstrap config.inc
config.inc: tools/config.sh config.mak
	@${MAKE} -s -f config.mak $@

core/config.h: config.inc core/version.h tools/config.sh config.mak
	@${MAKE} -s -f config.mak $@

core/version.h: config.mak $(shell git show-ref HEAD | ${SED} "s,^.* ,.git/,g")
	@${MAKE} -s -f config.mak $@

# bootstrap syn/greg.c, syn/compile.c not yet
grammar: syn/greg.y
	touch syn/greg.y
	${MAKE} syn/greg.c

syn/greg.c: syn/greg.y
	@${ECHO} GREG $<
	if [ -f ${GREG} ]; then ${GREG} syn/greg.y > syn/greg-new.c && \
	  ${CC} ${GREGCFLAGS} -o syn/greg-new syn/greg.c syn/compile.c syn/tree.c -Isyn && \
	  ${MV} syn/greg-new.c syn/greg.c && \
	  ${MV} syn/greg-new syn/greg; \
        fi

core/callcc.o core/callcc.o2: core/callcc.c core/config.h core/p2.h core/internal.h
	@${ECHO} CC $@ +frame-pointer
	@${CC} -c ${CFLAGS} -fno-omit-frame-pointer ${INCS} -o $@ $<
ifneq (${FPIC},)
core/callcc.${OPIC} core/callcc.${OPIC}2: core/callcc.c core/config.h core/p2.h core/internal.h
	@${ECHO} CC $@ +frame-pointer
	@${CC} -c ${CFLAGS} ${FPIC} -fno-omit-frame-pointer ${INCS} -o $@ $<
endif

core/potion.h: core/config.h
core/p2.h: core/potion.h
core/table.h: core/potion.h core/internal.h core/khash.h
$(foreach o,${OBJS},core/asm${o} ): core/asm.c core/p2.h core/config.h core/potion.h core/internal.h core/opcodes.h core/asm.h
$(foreach o,${OBJS},core/ast${o} ): core/ast.c core/p2.h core/config.h core/potion.h core/internal.h core/ast.h
$(foreach o,${OBJS},core/compile${o} ): core/compile.c core/p2.h core/config.h core/potion.h core/internal.h core/ast.h core/opcodes.h core/asm.h
$(foreach o,${OBJS},core/contrib${o} ): core/contrib.c core/config.h
$(foreach o,${OBJS},core/file${o} ): core/file.c core/p2.h core/config.h core/potion.h core/internal.h core/table.h
$(foreach o,${OBJS},core/gc${o} ): core/gc.c core/p2.h core/config.h core/potion.h core/internal.h core/table.h core/khash.h core/gc.h
$(foreach o,${OBJS},core/internal${o} ): core/internal.c core/p2.h core/config.h core/potion.h core/internal.h core/table.h core/gc.h
$(foreach o,${OBJS},core/lick${o} ): core/lick.c core/p2.h core/config.h core/potion.h core/internal.h
$(foreach o,${OBJS},core/load${o} ): core/load.c core/p2.h core/config.h core/potion.h core/internal.h core/table.h
$(foreach o,${OBJS},core/mt19937ar${o} ): core/mt19937ar.c core/p2.h
$(foreach o,${OBJS},core/number${o} ): core/number.c core/p2.h core/config.h core/potion.h core/internal.h
$(foreach o,${OBJS},core/objmodel${o} ): core/objmodel.c core/p2.h core/config.h core/potion.h core/internal.h core/table.h core/khash.h core/asm.h
$(foreach o,${OBJS},core/primitive${o} ): core/primitive.c core/p2.h core/config.h core/potion.h core/internal.h
$(foreach o,${OBJS},core/string${o} ): core/string.c core/p2.h core/config.h core/potion.h core/internal.h core/table.h core/khash.h
$(foreach o,${OBJS},core/table${o} ): core/table.c core/p2.h core/config.h core/potion.h core/internal.h core/khash.h core/table.h
$(foreach o,${OBJS},core/vm${o} ): core/vm.c core/vm-dis.c core/p2.h core/config.h core/potion.h core/internal.h core/opcodes.h core/khash.h core/table.h
$(foreach o,${OBJS},core/vm-ppc${o} ): core/vm-ppc.c core/p2.h core/config.h core/potion.h core/internal.h core/opcodes.h
$(foreach o,${OBJS},core/vm-x86${o} ): core/vm-x86.c core/p2.h core/config.h core/potion.h core/internal.h core/opcodes.h core/khash.h core/table.h


# no optimizations
#core/vm-x86.${OPIC}: core/vm-x86.c
#	@${ECHO} CC ${FPIC} $< +frame-pointer
#	@${CC} -c -g3 -fstack-protector -fno-omit-frame-pointer -Wall -fno-strict-aliasing -Wno-return-type# -D_GNU_SOURCE ${FPIC} ${INCS} -o $@ $<

%.i: %.c core/config.h
	@${ECHO} CPP $@
	@${CC} -c ${CFLAGS} ${INCS} -o $@ -E -c $<
%.i2: %.c core/config.h
	@${ECHO} CPP $@
	@${CC} -c -DP2 ${CFLAGS} ${INCS} -o $@ -E -c $<
%.o: %.c core/config.h
	@${ECHO} CC $@
	@${CC} -c ${CFLAGS} ${INCS} -o $@ $<
%.o2: %.c core/config.h
	@${ECHO} CC $@
	@${CC} -c -DP2 ${CFLAGS} ${INCS} -o $@ $<
ifneq (${FPIC},)
%.${OPIC}: %.c core/config.h
	@${ECHO} CC $@
	@${CC} -c ${FPIC} ${CFLAGS} ${INCS} -o $@ $<
%.${OPIC}2: %.c core/config.h
	@${ECHO} CC $@
	@${CC} -c -DP2 ${FPIC} ${CFLAGS} ${INCS} -o $@ $<
endif

.c.i: core/config.h
	@${ECHO} CPP $@
	@${CC} -c ${CFLAGS} ${INCS} -o $@ -E -c $<
.c.i2: core/config.h
	@${ECHO} CPP $@
	@${CC} -c -DP2 ${CFLAGS} ${INCS} -o $@ -E -c $<
.c.o: core/config.h
	@${ECHO} CC $@
	@${CC} -c ${CFLAGS} ${INCS} -o $@ $<
.c.o2: core/config.h
	@${ECHO} CC $@
	@${CC} -c -DP2 ${CFLAGS} ${INCS} -o $@ $<
ifneq (${FPIC},)
.c.${OPIC}: core/config.h
	@${ECHO} CC $@
	@${CC} -c ${FPIC} ${CFLAGS} ${INCS} -o $@ $<
.c.${OPIC}2: core/config.h
	@${ECHO} CC $<
	@${CC} -c -DP2 ${FPIC} ${CFLAGS} ${INCS} -o $@ $<
endif
%.c: %.y ${GREG}
	@${ECHO} GREG $@
	@${GREG} $< > $@-new && ${MV} $@-new $@
.y.c: ${GREG}
	@${ECHO} GREG $@
	@${GREG} $< > $@-new && ${MV} $@-new $@

bin/potion${EXE}: ${PIC_OBJ_POTION} lib/libpotion${DLL}
	@${ECHO} LINK $@
	@[ -d bin ] || mkdir bin
	@${CC} ${CFLAGS} ${PIC_OBJ_POTION} -o $@ ${LIBPTH} ${RPATH} -lpotion ${LIBS}
	@if [ "${DEBUG}" != "1" ]; then ${ECHO} STRIP $@; ${STRIP} $@; fi

bin/p2${EXE}: ${OBJ_P2} lib/libp2${DLL}
	@${ECHO} LINK $@
	@[ -d bin ] || mkdir bin
	@${CC} ${CFLAGS} ${OBJ_P2} -o $@ ${LIBPTH} ${RPATH} -lp2 ${LIBS} ${EXTLIBS}
	@if [ "${DEBUG}" != "1" ]; then ${ECHO} STRIP $@; ${STRIP} $@; fi

${GREG}: syn/greg.c syn/compile.c syn/tree.c
	@${ECHO} CC $@
	@[ -d bin ] || mkdir bin
	@${CC} ${GREGCFLAGS} -o $@ syn/greg.c syn/compile.c syn/tree.c -Isyn

bin/potion-s${EXE}: ${OBJ_POTION} lib/libpotion.a
	@${ECHO} LINK $@
	@[ -d bin ] || mkdir bin
	@${CC} ${CFLAGS} ${OBJ_POTION} -o $@ ${LIBPTH} lib/libpotion.a ${LIBS}

bin/p2-s${EXE}: ${OBJ_P2} lib/libp2.a
	@${ECHO} LINK $@
	@[ -d bin ] || mkdir bin
	@${CC} ${CFLAGS} ${OBJ_P2} -o $@ ${LIBPTH} lib/libp2.a ${LIBS} ${EXTLIBS}

lib/libpotion.a: ${OBJ_SYN} ${OBJ} core/config.h core/potion.h
	@${ECHO} AR $@
	@if [ -e $@ ]; then rm -f $@; fi
	@${AR} rcs $@ ${OBJ_SYN} ${OBJ} > /dev/null
	@${ECHO} RANLIB $@
	@-${RANLIB} $@

lib/libp2.a: ${OBJ_P2_SYN} ${OBJ2} core/config.h core/potion.h
	@${ECHO} AR $@
	@if [ -e $@ ]; then rm -f $@; fi
	@${AR} rcs $@ ${OBJ_P2_SYN} $(subst .o,.o2,${OBJ}) > /dev/null
	@${ECHO} RANLIB $@
	@-${RANLIB} $@

lib/libpotion${DLL}: ${PIC_OBJ} ${PIC_OBJ_SYN} core/config.h core/potion.h
	@${ECHO} LD $@
	@[ -d lib/potion ] || mkdir lib/potion
	@if [ -e $@ ]; then rm -f $@; fi
	@${CC} ${DEBUGFLAGS} -o $@ ${LDDLLFLAGS} ${RPATH} \
	  ${PIC_OBJ} ${PIC_OBJ_SYN} ${LIBS} > /dev/null
	@if [ x${DLL} = x.dll ]; then cp $@ bin/; fi

lib/libp2${DLL}: $(subst .${OPIC},.${OPIC}2,${PIC_OBJ}) ${PIC_OBJ_P2_SYN} core/config.h core/potion.h
	@${ECHO} LD $@
	@[ -d lib/potion ] || mkdir lib/potion
	@if [ -e $@ ]; then rm -f $@; fi
	@${CC} ${DEBUGFLAGS} -o $@ ${LDDLLFLAGS} $(subst libpotion,libp2,${RDLLFLAGS}) ${RPATH} \
	  $(subst .${OPIC},.${OPIC}2,${PIC_OBJ}) ${PIC_OBJ_P2_SYN} ${LIBS} ${EXTLIBS} > /dev/null
	@if [ x${DLL} = x.dll ]; then cp $@ bin/; fi

lib/potion/libsyntax${DLL}: syn/syntax.${OPIC}
	@${ECHO} LD $@
	@[ -d lib/potion ] || mkdir lib/potion
	@$(CC) ${DEBUGFLAGS} -o $@ $(INCS) ${LDDLLFLAGS} ${RPATH} \
	  $< ${LIBPTH} -lpotion $(LIBS)

lib/potion/libsyntax-p5${DLL}: syn/syntax-p5.${OPIC}2
	@${ECHO} LD $@
	@[ -d lib/potion ] || mkdir lib/potion
	@${CC} ${DEBUGFLAGS} -o $@ $(INCS) ${LDDLLFLAGS} ${RPATH} \
	  $< ${LIBPTH} -lp2 $(LIBS)

# 3rdparty EXTLIBS statically linked
lib/libuv.a: core/config.h core/potion.h \
  3rd/libuv/Makefile
	@${ECHO} MAKE $@
	@${MAKE} -s -C 3rd/libuv libuv${DLL}
	@cp 3rd/libuv/libuv.a lib/

3rd/libuv/libuv$(DLL): core/config.h core/potion.h \
  3rd/libuv/Makefile
	@${ECHO} MAKE $@
	@${MAKE} -s -C 3rd/libuv libuv${DLL}
	@cp 3rd/libuv/libuv$(DLL) lib/

lib/libsregex.a: core/config.h core/potion.h \
  3rd/sregex/Makefile
	@${ECHO} MAKE $@
	@${MAKE} -s -C 3rd/sregex CC="${CC}"
	@cp 3rd/sregex/libsregex.a lib/

lib/libpcre.a: core/config.h core/potion.h \
  3rd/pcre/Makefile
	@${ECHO} MAKE $@
	@${MAKE} -s -C 3rd/pcre CC="${CC}"
	@cp 3rd/pcre/libpcre.a lib/

# DYNLIBS
lib/potion/readline${LOADEXT}: core/config.h core/potion.h \
  lib/readline/Makefile lib/readline/linenoise.c \
  lib/readline/linenoise.h
	@${ECHO} MAKE $@
	@${MAKE} -s -C lib/readline
	@[ -d lib/potion ] || mkdir lib/potion
	@cp lib/readline/readline${LOADEXT} $@

lib/potion/buffile${LOADEXT}: core/config.h core/potion.h \
  lib/buffile.${OPIC}2 lib/buffile.c
	@${ECHO} LD $@
	@[ -d lib/potion ] || mkdir lib/potion
	@${CC} $(DEBUGFLAGS) -o $@ ${LDDLLFLAGS} \
	  lib/buffile.${OPIC}2 ${LIBPTH} ${LIBS} > /dev/null

lib/potion/aio${LOADEXT}: core/config.h core/potion.h \
  lib/aio.c
	@[ -d lib/potion ] || mkdir lib/potion
	@${ECHO} CC lib/aio.${OPIC}2
	@${CC} -c -DP2 ${FPIC} ${CFLAGS} ${INCS} -I3rd/libuv/include -o lib/aio.${OPIC}2 lib/aio.c > /dev/null
	@${ECHO} LD $@
	@${CC} $(DEBUGFLAGS) -o $@ $(subst libpotion,aio,${LDDLLFLAGS}) \
	  lib/aio.${OPIC}2 ${LIBS} ${EXTLIBS} > /dev/null

lib/potion/pcre${LOADEXT}: core/config.h core/potion.h \
  lib/pcre/Makefile lib/pcre/pcre.c
	@${ECHO} MAKE $@
	@${MAKE} -s -C lib/pcre
	@[ -d lib/potion ] || mkdir lib/potion
	@cp lib/pcre/pcre${LOADEXT} $@

lib/potion/m_apm${LOADEXT}: core/config.h core/potion.h \
  lib/m_apm/Makefile
	@${ECHO} MAKE $@
	@${MAKE} -s -C lib/m_apm
	@[ -d lib/potion ] || mkdir lib/potion
	@cp lib/m_apm/m_apm${LOADEXT} $@

lib/potion/libtommath${LOADEXT}: core/config.h core/potion.h \
  lib/libtommath/makefile.shared
	@${ECHO} MAKE $@
	cd lib/libtommath; ${CC} -c -I. ${FPIC} ${CFLAGS} *.c; \
	  ${CC} ${DEBUGFLAGS} -o libtommath${LOADEXT} ${LDDLLFLAGS} \
	  *.o ${LIBPTH} ${LIBS}; cd ../..
	@${ECHO} @${MAKE} -s -C lib/libtommath -f makefile.shared
	@[ -d lib/potion ] || mkdir lib/potion
	@cp lib/libtommath/libtommath${LOADEXT} $@

bench: bin/gc-bench${EXE} bin/potion${EXE}
	@${ECHO}; \
	  ${ECHO} running GC benchmark; \
	  time bin/gc-bench

check: test.pn test.p2
test:  test.pn test.p2

test.pn: bin/potion${EXE} bin/potion-test${EXE}
	@${ECHO}; \
	${ECHO} running potion API tests; \
	LD_LIBRARY_PATH=`pwd`/lib/potion:$LD_LIBRARY_PATH \
	export LD_LIBRARY_PATH; \
	${RUNPRE}potion-test; \
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
		jit=`${RUNPRE}potion -v | ${SED} "/jit=1/!d"`; \
		if [ "$$jit" = "" ]; then \
		    ${ECHO} skipping; \
		    break; \
		fi; \
	  fi; \
	  for f in test/**/*.pn; do \
		look=`${CAT} $$f | ${SED} "/\#=>/!d; s/.*\#=> //"`; \
		if [ $$t -eq 0 ]; then \
			for=`${RUNPRE}potion -I -B $$f | ${SED} "s/\n$$//"`; \
		elif [ $$t -eq 1 ]; then \
			${RUNPRE}potion -c $$f > /dev/null; \
			fb="$$f"b; \
			for=`${RUNPRE}potion -I -B $$fb | ${SED} "s/\n$$//"`; \
			rm -rf $$fb; \
		else \
			for=`${RUNPRE}potion -I -X $$f | ${SED} "s/\n$$//"`; \
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

test.p2: bin/p2${EXE} bin/p2-test${EXE} bin/gc-test${EXE}
	@${ECHO}; \
	${ECHO} running p2 API tests; \
	LD_LIBRARY_PATH=`pwd`/lib:`pwd`/lib/potion:$LD_LIBRARY_PATH \
	export LD_LIBRARY_PATH; \
	${RUNPRE}p2-test; \
	${ECHO} running GC tests; \
	${RUNPRE}gc-test; \
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
		jit=`${RUNPRE}p2 -v | ${SED} "/jit=1/!d"`; \
		if [ "$$jit" = "" ]; then \
		    ${ECHO} skipping; \
		    break; \
		fi; \
	  fi; \
	  for f in test/**/*.pl; do \
		look=`${CAT} $$f | ${SED} "/\#=>/!d; s/.*\#=> //"`; \
		if [ $$t -eq 0 ]; then \
			for=`${RUNPRE}p2 --inspect -B $$f | ${SED} "s/\n$$//"`; \
		elif [ $$t -eq 1 ]; then \
			${RUNPRE}p2 --compile $$f > /dev/null; \
			fb="$$f"c; \
			for=`${RUNPRE}p2 --inspect -B $$fb | ${SED} "s/\n$$//"`; \
			rm -rf $$fb; \
		else \
			for=`${RUNPRE}p2 --inspect -J $$f | ${SED} "s/\n$$//"`; \
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

# for LTO gold -O4
bin/potion-test${EXE}: ${OBJ_TEST} lib/libpotion.a
	@${ECHO} LINK $@
	@if ${CC} ${CFLAGS} ${OBJ_TEST} -o $@ lib/libpotion.a ${LIBS}; then true; else \
	  ${CC} ${CFLAGS} ${OBJ_TEST} -o $@ ${OBJ} ${OBJ_SYN} ${LIBS}; fi

bin/gc-test${EXE}: ${OBJ_GC_TEST} lib/libp2.a
	@${ECHO} LINK $@
	@${CC} ${CFLAGS} ${OBJ_GC_TEST} -o $@ lib/libp2.a ${LIBS} ${EXTLIBS}

bin/gc-bench${EXE}: ${OBJ_GC_BENCH} lib/libp2.a
	@${ECHO} LINK $@
	@${CC} ${CFLAGS} ${OBJ_GC_BENCH} -o $@ lib/libp2.a ${LIBS} ${EXTLIBS}

bin/p2-test${EXE}: ${OBJ_P2_TEST} lib/libp2.a
	@${ECHO} LINK $@
	@if ${CC} ${CFLAGS} ${OBJ_P2_TEST} -o $@ lib/libp2.a ${LIBS} ${EXTLIBS}; then true; else \
	  ${CC} ${CFLAGS} ${OBJ_P2_TEST} -o $@ ${OBJ2} ${OBJ_P2_SYN} ${LIBS} ${EXTLIBS}; fi

dist: bins libs static doc ${SRC_SYN} ${SRC_P2_SYN}
	@if [ -n "${RPATH}" ]; then \
	  rm -f ${BINS} ${PLIBS}; \
	  ${MAKE} bins libs RPATH="${RPATH_INSTALL}"; \
	fi
	+${MAKE} -f dist.mak $@ PREFIX=${PREFIX} EXE=${EXE} DLL=${DLL} LOADEXT=${LOADEXT}

install: dist
	+${MAKE} -f dist.mak $@ PREFIX="${PREFIX}"

tarball:
	+${MAKE} -f dist.mak $@ PREFIX="${PREFIX}"

release: dist
	+${MAKE} -f dist.mak $@ PREFIX="${PREFIX}"

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

doc: ${DOCHTML} doc/html/files.html
docall: doc GTAGS

doxygen: doc/html/files.html
	@${ECHO} DOXYGEN -f core
	@perl -pe's/^  //;s/^~ /## ~ /;' README > README.md
	@doc/footer.sh > doc/footer.inc
	@doxygen doc/Doxyfile
	@rm README.md

doc/html/files.html: core/*.c core/*.h doc/Doxyfile doc/footer.sh Makefile
	@${ECHO} DOXYGEN core
	@perl -pe's/^  //;s/^~ /## ~ /;' README > README.md
	doc/footer.sh > doc/footer.inc
	@doxygen doc/Doxyfile 2>&1 |egrep -v "  parameter 'P|self|cl'"
	@rm README.md

# perl11.org admins only. requires: doxygen redcloth global
website:
	test -d ${WEBSITE} || exit
	@${MAKE} doxygen
	cp -r doc/html/* ${WEBSITE}/p2/html/
	@${MAKE} doc
	cp doc/*.html ${WEBSITE}/p2/
	@${MAKE} GTAGS
	cp -r HTML/* ${WEBSITE}/p2/ref/
	cd ${WEBSITE}/p2/ && git add *.html html ref && git ci -m'doc: automatic update'
	@${ECHO} "need to cd ${WEBSITE}; git push"

MANIFEST:
	git ls-tree -r --name-only HEAD > $@

# in seperate clean subdir. do not index work files
GTAGS: ${SRC} core/*.h
	+${MAKE} -f dist.mak $@ PREFIX=${PREFIX}

TAGS: ${SRC} core/*.h
	@rm -f TAGS
	/usr/bin/find core syn front \( -name \*.c -o -name \*.h \) -exec etags -a --language=c \{\} \;

sloc: clean
	@mv syn/greg.c syn/greg-c.tmp
	@sloccount core syn front
	@mv syn/greg-c.tmp syn/greg.c

todo:
	@grep -rInso 'TODO: \(.\+\)' core syn front

clean:
	@${ECHO} cleaning
	@rm -f $(foreach ext,o o2 opic opic2 i gcda gcno,$(foreach dir,core syn front lib test/api lib/*,${dir}/*.${ext}))
	@rm -f bin/* lib/potion/* lib/*.a lib/*${DLL} lib/*${LOADEXT}
	@rm -f ${DOCHTML} README.md doc/footer.inc
	@rm -f tools/*.o core/config.h core/version.h
	@rm -f tools/*~ doc/*~ example/*~ core/*~ config.inc~ tools/config.c
	@rm -f lib/*/*${LOADEXT} lib/*/*.o
	@rm -rf doc/html doc/latex

# also config.inc and files needed for cross-compilation
realclean: clean
	@rm -f config.inc ${SRC_SYN} ${SRC_P2_SYN} ${GREG}
	@rm -f GPATH GTAGS GRTAGS
	@rm -rf HTML
	@find . -name \*.gcov -delete

