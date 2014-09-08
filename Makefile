# posix (linux, bsd, osx, solaris) + mingw with gcc/clang only
.SUFFIXES: .y .c .i .i2 .o .opic .o2 .opic2 .textile .html
.PHONY: all default bins libs pn p2 static usage config clean doc rebuild check test test.pn test.p2 \
	examples bench tarball dist release install grammar doxygen website \
	testable spectest_checkout spectest_init spectest_update
.NOTPARALLEL: test test.pn test.p2

SRC = core/asm.c core/ast.c core/compile.c core/contrib.c core/gc.c core/internal.c core/lick.c core/mt19937ar.c core/number.c core/objmodel.c core/primitive.c core/string.c core/table.c core/vm.c
PLIBS = readline buffile aio
PLIBS_SRC = lib/aio.c lib/buffile.c lib/readline/readline.c lib/readline/linenoise.c
GREGCFLAGS = -O3 -DNDEBUG

# bootstrap config.inc with make -f config.mak
include config.inc

ifneq (${DISABLE_CALLCC},1)
SRC += core/callcc.c
endif
ifneq (${SANDBOX},1)
SRC += core/file.c core/load.c
else
PLIBS = readline aio
PLIBS_SRC = lib/aio.c lib/readline/readline.c lib/readline/linenoise.c
ifeq ($(WIN32),1)
PLIBS_SRC += lib/readline/win32fixes.c
endif
endif

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
  else
    ifneq (${SANDBOX},1)
      PLIBS = readline buffile
    else
      PLIBS = readline
    endif
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
BINS = bin/potion${EXE} bin/p2${EXE}
PNLIB = $(foreach l,potion p2,lib/lib$l${DLL})
PNLIB += $(foreach s,syntax syntax-p5,lib/potion/lib$s${DLL})
#EXTLIBS = $(foreach m,uv pcre,lib/p2/lib$m${LOADEXT})
#EXTLIBS = -L3rd/pcre -lpcre -L3rd/libuv -luv -L3rd/libtommath -llibtommath
EXTLIBS = -Llib -luv
ifeq (${WIN32},1)
LIBUV = lib/libuv-11.dll lib/libuv.dll.a
EXTLIBS += -lw32_32
else
LIBUV = lib/libuv${DLL}
endif
EXTLIBDEPS = ${LIBUV}
DYNLIBS = $(foreach m,${PLIBS},lib/potion/$m${LOADEXT}) lib/p2/aio${LOADEXT}
PLIBS_OBJ = ${PLIBS_SRC:.c=.${OPIC}}
PLIBS_OBJS = ${PLIBS_SRC:.c=.o}
PLIBS_OBJS2 = ${PLIBS_SRC:.c=.o2}
OBJS = o o2
ifneq (${FPIC},)
  OBJS += ${OPIC} ${OPIC}2
endif
DOC = doc/start.textile doc/p2-extensions.textile doc/glossary.textile doc/design-decisions.textile \
  doc/concurrency.textile
DOCHTML = ${DOC:.textile=.html}
LIBPNA_AWAY = if [ -f lib/libpotion.a ]; then mv lib/libpotion.a lib/libpotion.a.tmp; fi
LIBPNA_BACK = if [ -f lib/libpotion.a.tmp ]; then mv lib/libpotion.a.tmp lib/libpotion.a; fi
LIBP2A_AWAY = if [ -f lib/libp2.a ]; then mv lib/libp2.a lib/libp2.a.tmp; fi
LIBP2A_BACK = if [ -f lib/libp2.a.tmp ]; then mv lib/libp2.a.tmp lib/libp2.a; fi

CAT  = /bin/cat
ECHO ?= /bin/echo
MV   = /bin/mv
SED  = sed
EXPR = expr
GREG = bin/greg${EXE}
AR     ?= ar
RANLIB ?= ranlib
ifeq (${CROSS},1)
  GREGCROSS = syn/greg
else
  GREGCROSS = ${GREG}
endif

# perl11.org only
WEBSITE = ../perl11.org

default: pn p2 libs
	+$(MAKE) -s usage

all: default libs static docall test
ifneq (${SANDBOX},1)
pn: bin/potion${EXE} ${PNLIB}
p2: bin/p2${EXE} ${PNLIB}
else
pn: static
p2: static
endif
bins: ${BINS}
libs: ${PNLIB} ${DYNLIBS}
static: lib/libpotion.a bin/potion-s${EXE} lib/libp2.a bin/p2-s${EXE}
rebuild: clean default test

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
	@${ECHO} " Maintained at https://github.com/perl11/p2, branch p2"

config:
	@${ECHO} MAKE -f config.mak $@
	@$(MAKE) -s -f config.mak config.inc core/config.h

# bootstrap config.inc
config.inc: tools/config.sh config.mak
	@$(MAKE) -s -f config.mak $@

core/config.h: config.inc core/version.h tools/config.sh config.mak
	@$(MAKE) -s -f config.mak $@

core/version.h: config.mak $(shell git show-ref HEAD | ${SED} "s,^.* ,.git/,g")
	@$(MAKE) -s -f config.mak $@

grammar: syn/greg.y
	touch syn/greg.y
	+$(MAKE) syn/greg.c

# bootstrap syn/greg.c, syn/compile.c not yet
syn/greg.c: syn/greg.y
	@${ECHO} GREG $<
	@if test -f ${GREG}; then ${GREG} syn/greg.y > syn/greg-new.c && \
	  ${CC} ${GREGCFLAGS} -o syn/greg-new syn/greg-new.c syn/compile.c syn/tree.c -Isyn && \
	  ${MV} syn/greg-new.c syn/greg.c && \
	  ${MV} syn/greg-new syn/greg; \
	fi

core/callcc.o: core/callcc.c core/p2.h core/potion.h core/config.h core/internal.h
	@${ECHO} CC $@ -O0 +frame-pointer
	@${CC} -c ${CFLAGS} -O0 -fno-omit-frame-pointer ${INCS} -o $@ $<
core/callcc.o2: core/callcc.c core/p2.h core/potion.h core/config.h core/internal.h
	@${ECHO} CC $@ -O0 +frame-pointer
	@${CC} -c -DP2 ${CFLAGS} -O0 -fno-omit-frame-pointer ${INCS} -o $@ $<
ifneq (${FPIC},)
core/callcc.${OPIC}: core/callcc.c core/p2.h core/potion.h core/config.h core/internal.h
	@${ECHO} CC $@ -O0 +frame-pointer
	@${CC} -c ${CFLAGS} ${FPIC} -O0 -fno-omit-frame-pointer ${INCS} -o $@ $<
core/callcc.${OPIC}2: core/callcc.c core/config.h core/p2.h core/potion.h core/internal.h
	@${ECHO} CC $@ -O0 +frame-pointer
	@${CC} -c -DP2 ${CFLAGS} ${FPIC} -O0 -fno-omit-frame-pointer ${INCS} -o $@ $<
endif

front/potion.o: front/potion.c core/potion.h core/config.h core/internal.h \
 core/opcodes.h core/khash.h core/table.h
	@${ECHO} CC $@ -O0
	@${CC} -c ${CFLAGS} -O0 ${INCS} -o $@ $<
front/p2.o2: front/p2.c core/p2.h core/potion.h core/config.h core/internal.h \
 core/opcodes.h core/khash.h core/table.h core/khash.h
	@${ECHO} CC $@ -O0
	@${CC} -c -DP2 ${CFLAGS} -O0 ${INCS} -o $@ $<
ifneq (${FPIC},)
front/potion.${OPIC}: front/potion.c core/potion.h core/config.h core/internal.h \
 core/opcodes.h core/khash.h core/table.h
	@${ECHO} CC $@ -O0
	@${CC} -c ${CFLAGS} -O0 ${FPIC} ${INCS} -o $@ $<
front/p2.${OPIC}2: front/p2.c core/p2.h core/potion.h core/config.h core/internal.h \
 core/opcodes.h core/khash.h core/table.h core/khash.h
	@${ECHO} CC $@ -O0
	@${CC} -c -DP2 ${CFLAGS} -O0 ${FPIC} ${INCS} -o $@ $<
endif

core/potion.h: core/config.h
core/p2.h: core/potion.h
core/table.h: core/potion.h core/internal.h core/khash.h
# for c in core/*.c; do gcc -MM -D_GNU_SOURCE  -Icore $c; done |perl -lpe's/^(.+)o:/\$(foreach o,\${OBJS},core\/$1.\${o} ):/'
$(foreach o,${OBJS},core/asm.${o} ): core/asm.c core/p2.h core/potion.h core/config.h core/internal.h \
 core/opcodes.h core/asm.h
$(foreach o,${OBJS},core/ast.${o} ): core/ast.c core/p2.h core/potion.h core/config.h core/internal.h \
 core/ast.h
$(foreach o,${OBJS},core/compile.${o} ): core/compile.c core/p2.h core/potion.h core/config.h \
 core/internal.h core/ast.h core/opcodes.h core/asm.h
$(foreach o,${OBJS},core/contrib.${o} ): core/contrib.c core/config.h
$(foreach o,${OBJS},core/file.${o} ): core/file.c core/p2.h core/potion.h core/config.h core/internal.h \
 core/table.h core/khash.h
$(foreach o,${OBJS},core/gc.${o} ): core/gc.c core/p2.h core/potion.h core/config.h core/internal.h \
 core/gc.h core/khash.h core/table.h
$(foreach o,${OBJS},core/internal.${o} ): core/internal.c core/p2.h core/potion.h core/config.h \
 core/internal.h core/table.h core/khash.h core/gc.h
$(foreach o,${OBJS},core/lick.${o} ): core/lick.c core/p2.h core/potion.h core/config.h core/internal.h
$(foreach o,${OBJS},core/load.${o} ): core/load.c core/p2.h core/potion.h core/config.h core/internal.h \
 core/table.h core/khash.h
$(foreach o,${OBJS},core/mt19937ar.${o} ): core/mt19937ar.c core/p2.h core/potion.h core/config.h
$(foreach o,${OBJS},core/number.${o} ): core/number.c core/p2.h core/potion.h core/config.h \
 core/internal.h
$(foreach o,${OBJS},core/objmodel.${o} ): core/objmodel.c core/p2.h core/potion.h core/config.h \
 core/internal.h core/khash.h core/table.h core/asm.h
$(foreach o,${OBJS},core/primitive.${o} ): core/primitive.c core/p2.h core/potion.h core/config.h \
 core/internal.h
$(foreach o,${OBJS},core/string.${o} ): core/string.c core/p2.h core/potion.h core/config.h \
 core/internal.h core/khash.h core/table.h
$(foreach o,${OBJS},core/table.${o} ): core/table.c core/p2.h core/potion.h core/config.h \
 core/internal.h core/khash.h core/table.h
$(foreach o,${OBJS},core/vm.${o} ): core/vm.c core/p2.h core/potion.h core/config.h \
 core/internal.h core/opcodes.h core/asm.h core/khash.h core/table.h core/vm-dis.c
$(foreach o,${OBJS},core/vm-ppc.${o} ): core/vm-ppc.c core/p2.h core/potion.h core/config.h \
 core/internal.h core/opcodes.h core/asm.h
$(foreach o,${OBJS},core/vm-x86.${o} ): core/vm-x86.c core/p2.h core/potion.h core/config.h \
 core/internal.h core/opcodes.h core/asm.h core/khash.h core/table.h

%.i: %.c core/config.h
	@${ECHO} CPP $@
	@${CC} -c ${CFLAGS} ${INCS} -o $@ -E -c $<
%.i2: %.c core/config.h
	@${ECHO} CPP $@
	@${CC} -c -DP2 ${CFLAGS} ${INCS} -o $@ -E -c $<
%.in: %.c core/config.h
	@${ECHO} CPP ASTYLE $@
	@${CC} -c ${CFLAGS} ${INCS} -E -c $< | perl -pe's,^# (\d+) ",//# \1 ",' > $@.tmp && \
	  astyle -s2 < $@.tmp > $@
%.in2: %.c core/config.h
	@${ECHO} CPP ASTYLE $@
	@${CC} -c -DP2 ${CFLAGS} ${INCS} -E -c $< | perl -pe's,^# (\d+) ",//# \1 ",' > $@.tmp && \
	  astyle -s2 < $@.tmp > $@
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
.c.in: core/config.h
	@${ECHO} CPP ASTYLE $@
	@${CC} -c ${CFLAGS} ${INCS} -E -c $< | perl -pe's,^# (\d+) ",//# \1 ",' > $@.tmp && \
	  astyle -s2 < $@.tmp > $@
.c.in2: core/config.h
	@${ECHO} CPP ASTYLE $@
	@${CC} -c -DP2 ${CFLAGS} ${INCS} -E -c $< | perl -pe's,^# (\d+) ",//# \1 ",' > $@.tmp && \
	  astyle -s2 < $@.tmp > $@
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

%.c: %.y ${GREGCROSS}
	@${ECHO} GREG $@
	@${GREGCROSS} $< > $@-new && ${MV} $@-new $@
.y.c: ${GREGCROSS}
	@${ECHO} GREG $@
	@${GREGCROSS} $< > $@-new && ${MV} $@-new $@

${GREG}: syn/greg.c syn/compile.c syn/tree.c
	@${ECHO} CC $@
	@${CC} ${GREGCFLAGS} -o $@ syn/greg.c syn/compile.c syn/tree.c -Isyn

bin/potion${EXE}: ${PIC_OBJ_POTION} lib/libpotion${DLL}
	@${ECHO} LINK $@
	@${LIBPNA_AWAY}
	@${CC} ${CFLAGS} ${LDFLAGS} ${PIC_OBJ_POTION} -o $@ ${LIBPTH} ${RPATH} -lpotion  ${LIBS}
	@${LIBPNA_BACK}
	@if [ "${DEBUG}" != "1" ]; then ${ECHO} STRIP $@; ${STRIP} $@; fi

bin/p2${EXE}: ${OBJ_P2} lib/libp2${DLL}
	@${ECHO} LINK $@
	@${LIBP2A_AWAY}
	@${CC} ${CFLAGS} ${LDFLAGS} ${OBJ_P2} -o $@ ${LIBPTH} ${RPATH} -lp2 ${LIBS}
	@${LIBP2A_BACK}
	@if [ "${DEBUG}" != "1" ]; then ${ECHO} STRIP $@; ${STRIP} $@; fi

bin/potion-s${EXE}: lib/libpotion.a ${PLIBS_OBJS}
	@${ECHO} LINK $@
	@${CC} -c ${CFLAGS} ${INCS} -DSTATIC -o front/potion.os front/potion.c
	@${CC} ${CFLAGS} ${LDFLAGS} front/potion.os -o $@ ${PLIBS_OBJS} \
          lib/libpotion.a ${LIBPTH} ${RPATH} ${EXTLIBS} ${LIBS}
	@if [ "${DEBUG}" != "1" ]; then ${ECHO} STRIP $@; ${STRIP} $@; fi
	@if [ "${SANDBOX}" = "1" ]; then rm bin/potion${EXE}; cd bin; ln -s potion-s${EXE} potion${EXE}; cd ..; fi

bin/p2-s${EXE}: lib/libp2.a ${PLIBS_OBJS2}
	@${ECHO} LINK $@
	@${CC} -c ${CFLAGS} ${INCS} -DSTATIC -DP2 -o front/p2.os front/p2.c
	@${CC} ${CFLAGS} ${LDFLAGS} front/p2.os -o $@ ${PLIBS_OBJS2} \
          lib/libp2.a ${LIBPTH} ${RPATH} ${EXTLIBS} ${LIBS}
	@if [ "${DEBUG}" != "1" ]; then ${ECHO} STRIP $@; ${STRIP} $@; fi
	@if [ "${SANDBOX}" = "1" ]; then rm bin/p2${EXE}; cd bin; ln -s p2-s${EXE} p2${EXE}; cd ..; fi

lib/readline/readline.o lib/readline/readline.o2: lib/readline/readline.c lib/readline/linenoise.c
	@${ECHO} CC $@
	@${LIBPNA_AWAY}
	@$(MAKE) -s -C lib/readline static
	@ln -sf readline.o lib/readline/readline.o2
	@${LIBPNA_BACK}

lib/libpotion.a: ${OBJ_SYN} ${OBJ} core/config.h core/potion.h
	@${ECHO} AR $@
	@if [ -e $@ ]; then rm -f $@; fi
	@${AR} rcs $@ ${OBJ_SYN} ${OBJ} > /dev/null
	@${ECHO} RANLIB $@
	@-${RANLIB} $@

lib/libp2.a: ${OBJ_P2_SYN} ${OBJ2} core/config.h core/potion.h
	@${ECHO} AR $@
	@if [ -e $@ ]; then rm -f $@; fi
	@${AR} rcs $@ ${OBJ_P2_SYN} ${OBJ2} > /dev/null
	@${ECHO} RANLIB $@
	@-${RANLIB} $@

lib/libpotion${DLL}: ${PIC_OBJ} ${PIC_OBJ_SYN} core/config.h core/potion.h
	@${ECHO} LD $@
	@if [ -e $@ ]; then rm -f $@; fi
	@${CC} ${DEBUGFLAGS} -o $@ ${LDDLLFLAGS} ${RPATH} \
	  ${PIC_OBJ} ${PIC_OBJ_SYN} ${LIBPTH} ${LIBS} > /dev/null
	@if [ x${DLL} = x.dll ]; then cp $@ bin/; fi

lib/libp2${DLL}: $(subst .${OPIC},.${OPIC}2,${PIC_OBJ}) ${PIC_OBJ_P2_SYN} core/config.h core/potion.h
	@${ECHO} LD $@
	@if [ -e $@ ]; then rm -f $@; fi
	@${CC} ${DEBUGFLAGS} -o $@ $(subst libpotion,libp2,${LDDLLFLAGS}) ${RPATH} \
	  $(subst .${OPIC},.${OPIC}2,${PIC_OBJ}) ${PIC_OBJ_P2_SYN} ${LIBPTH} ${LIBS} > /dev/null
	@if [ x${DLL} = x.dll ]; then cp $@ bin/; fi

lib/potion/libsyntax${DLL}: syn/syntax.${OPIC} lib/libpotion${DLL}
	@${ECHO} LD $@
	@$(CC) ${DEBUGFLAGS} -o $@ $(INCS) ${LDDLLFLAGS} ${RPATH} \
	  $< ${LIBPTH} -lpotion $(LIBS)

lib/potion/libsyntax-p5${DLL}: syn/syntax-p5.${OPIC}2 lib/libp2${DLL}
	@${ECHO} LD $@
	@${CC} ${DEBUGFLAGS} -o $@ $(INCS) $(subst libpotion,potion/libsyntax-p5,${LDDLLFLAGS}) \
	  $< ${LIBPTH} -lp2 $(LIBS)

# 3rdparty EXTLIBS statically linked
3rd/libuv/Makefile.am: .gitmodules
	git submodule update --init
	@touch $@

ifeq (${WIN32},1)
PATCH_PHLPAPI2 = sed -i -e"s,-lphlpapi2,-liphlpapi," 3rd/libuv/Makefile.am
CROSSHOST = --host=$(subst -gcc,,${CC})
else
PATCH_PHLPAPI2 = echo
CROSSHOST =
endif

3rd/libuv/Makefile: 3rd/libuv/Makefile.am
	@${ECHO} AUTOGEN $@
	@${PATCH_PHLPAPI2}
	cd 3rd/libuv && ./autogen.sh
	-grep "libuv 0." 3rd/libuv/configure && sed -i -e's,libuv 0.,libuv-0.,' 3rd/libuv/configure
	cd 3rd/libuv && CC="${CC}" ./configure --disable-dtrace --enable-shared --prefix="$(shell pwd)" \
	  "${CROSSHOST}"

lib/libuv.a: config.inc 3rd/libuv/Makefile
	@${ECHO} MAKE $@
	+$(MAKE) -s -C 3rd/libuv libuv.la
	cp 3rd/libuv/.libs/libuv.a lib/
	@touch $@

# default: shared
${LIBUV}: config.inc 3rd/libuv/Makefile
	@${ECHO} MAKE $@
	+$(MAKE) -s -C 3rd/libuv libuv.la
	rsync -a 3rd/libuv/.libs/libuv*${DLL}* lib/ || cp 3rd/libuv/.libs/libuv.a lib/;
	@touch $@

lib/libsregex.a: core/config.h core/potion.h \
  3rd/sregex/Makefile
	@${ECHO} MAKE $@
	@$(MAKE) -s -C 3rd/sregex CC="${CC}"
	@cp 3rd/sregex/libsregex.a lib/

# default: static
lib/libpcre.a: core/config.h core/potion.h \
  3rd/pcre/Makefile
	@${ECHO} MAKE $@
	@$(MAKE) -s -C 3rd/pcre CC="${CC}"
	@cp 3rd/pcre/.libs/libpcre.a lib/

lib/libpcre$(DLL): core/config.h core/potion.h \
  3rd/pcre/Makefile
	@${ECHO} MAKE $@
	@$(MAKE) -s -C 3rd/pcre CC="${CC}"
	@cp 3rd/pcre/.libs/libpcre${DLL}* lib/

# DYNLIBS
lib/potion/readline${LOADEXT}: core/config.h core/potion.h \
  lib/readline/Makefile lib/readline/linenoise.c \
  lib/readline/linenoise.h lib/libpotion${DLL}
	@${ECHO} MAKE $@
	@${LIBPNA_AWAY}
	@$(MAKE) -s -C lib/readline
	@${LIBPNA_BACK}
	@cp lib/readline/readline${LOADEXT} $@

lib/potion/buffile${LOADEXT}: core/config.h core/potion.h \
  lib/buffile.${OPIC} lib/buffile.c lib/libpotion${DLL}
	@${ECHO} LD $@
	@${LIBPNA_AWAY}
	@${CC} $(DEBUGFLAGS) -o $@ $(subst libpotion,potion/buffile,${LDDLLFLAGS}) ${RPATH} \
	  lib/buffile.${OPIC} ${LIBPTH} -lpotion ${LIBS} > /dev/null
	@${LIBPNA_BACK}

lib/p2/buffile${LOADEXT}: core/config.h core/potion.h core/p2.h \
  lib/buffile.${OPIC}2 lib/buffile.c lib/libp2${DLL}
	@${ECHO} LD $@
	@${LIBP2A_AWAY}
	@${CC} $(DEBUGFLAGS) -o $@ $(subst libpotion,p2/buffile,${LDDLLFLAGS}) ${RPATH} \
	  lib/buffile.${OPIC}2 ${LIBPTH} -lp2 ${LIBS} > /dev/null
	@${LIBP2A_BACK}

ifeq ($(HAVE_LIBUV),1)
AIO_DEPS =
#TODO: libtool?
AIO_DEPLIBS =
else
AIO_DEPS = ${LIBUV}
AIO_DEPLIBS := `perl -ane'/dependency_libs=(.*)/ && print substr($$1,2,-1)' 3rd/libuv/libuv.la`
endif

lib/potion/aio${LOADEXT}: core/config.h core/potion.h \
  lib/aio.c $(AIO_DEPS) lib/libpotion${DLL}
	@${ECHO} CC lib/aio.${OPIC}
	@${CC} -c ${FPIC} ${CFLAGS} ${INCS} -o lib/aio.${OPIC} lib/aio.c > /dev/null
	@${ECHO} LD $@
	@${LIBPNA_AWAY}
	@${CC} $(DEBUGFLAGS) -o $@ $(subst libpotion,potion/aio,${LDDLLFLAGS}) ${RPATH} \
	  lib/aio.${OPIC} ${LIBPTH} -lpotion ${EXTLIBS} ${LIBS} ${AIO_DEPLIBS} > /dev/null
	@${LIBPNA_BACK}

lib/p2/aio${LOADEXT}: core/config.h core/potion.h \
  lib/aio.c $(AIO_DEPS) lib/libp2${DLL}
	@${ECHO} CC lib/aio.${OPIC}2
	@${CC} -c ${FPIC} -DP2 ${CFLAGS} ${INCS} -o lib/aio.${OPIC}2 lib/aio.c > /dev/null
	@${ECHO} LD $@
	@${LIBP2A_AWAY}
	@${CC} $(DEBUGFLAGS) -o $@ $(subst libp2,p2/aio,${LDDLLFLAGS}) ${RPATH} \
	  lib/aio.${OPIC}2 ${LIBPTH} -lp2 ${EXTLIBS} ${LIBS} ${AIO_DEPLIBS} > /dev/null
	@${LIBP2A_BACK}

ifeq ($(HAVE_PCRE),1)
PCRE_DEPS =
else
PCRE_DEPS = lib/libpcre.a
endif

lib/p2/pcre${LOADEXT}: core/config.h core/potion.h \
  lib/pcre/Makefile lib/pcre/pcre.c $(PCRE_DEPS) lib/libpotion${DLL}
	@${ECHO} MAKE $@
	@${LIBP2A_AWAY}
	@$(MAKE) -s -C lib/pcre
	@${LIBP2A_BACK}
	@cp lib/pcre/pcre${LOADEXT} $@

lib/p2/m_apm${LOADEXT}: core/config.h core/potion.h \
  lib/m_apm/Makefile lib/libpotion${DLL}
	@${ECHO} MAKE $@
	@${LIBP2A_AWAY}
	@$(MAKE) -s -C lib/m_apm
	@${LIBP2A_BACK}
	@cp lib/m_apm/m_apm${LOADEXT} $@

lib/p2/libtommath${LOADEXT}: core/config.h core/potion.h \
  3rd/libtommath/makefile.shared lib/libpotion${DLL}
	@${ECHO} MAKE $@
	@${LIBP2A_AWAY}
	cd 3rd/libtommath; ${CC} -c -I. ${FPIC} ${CFLAGS} *.c; \
	  ${CC} ${DEBUGFLAGS} -o ../../$@ $(subst libpotion,p2/libtommath,${LDDLLFLAGS}) \
	  *.o ${LIBPTH} -lpotion ${LIBS}; cd ../..
	@${LIBP2A_BACK}

bench: bin/gc-bench${EXE} bin/potion${EXE}
	@${ECHO} running GC benchmark; time bin/gc-bench
	$(MAKE) -s examples

check: test
test:  test.pn test.p2

test.pn: pn libs testable
	+test/runtests.sh -q -pn

test.p2: p2 libs testable
	+test/runtests.sh -q -p2

testable : bin/potion${EXE} bin/p2${EXE} libs bin/potion-test${EXE} bin/p2-test${EXE} bin/gc-test${EXE}

spectest_checkout : test/spec
test/spec :
	@${ECHO} UPDATE $@
	@git submodule update --init test/spec

spectest_init :
	@git submodule add https://github.com/rakudo-p5/roast5 test/spec

spectest_update :
	-cd test/spec && git pull

spectest: testable test/spectest.data
	 perl test/harness --tests-from-file=test/spectest.data

fulltest: testable test/spectest.data
	 perl test/harness test/spec/*.t test/spec/*/*.t

# for LTO gold -O4
bin/potion-test${EXE}: ${OBJ_TEST} lib/libpotion.a
	@${ECHO} LINK $@
	@if ${CC} ${CFLAGS} ${LDFLAGS} ${OBJ_TEST} -o $@ lib/libpotion.a ${RPATH} ${LIBPTH} ${EXTLIBS} ${LIBS}; then true; else \
	  ${CC} ${CFLAGS} ${LDFLAGS} ${OBJ_TEST} -o $@ ${OBJ} ${OBJ_SYN} ${LIBPTH} ${LIBS}; fi

bin/gc-test${EXE}: ${OBJ_GC_TEST} lib/libp2.a
	@${ECHO} LINK $@
	@${CC} ${CFLAGS} ${LDFLAGS} ${OBJ_GC_TEST} -o $@ lib/libp2.a ${RPATH} ${LIBPTH} ${EXTLIBS} ${LIBS}

bin/gc-bench${EXE}: ${OBJ_GC_BENCH} lib/libp2.a
	@${ECHO} LINK $@
	@${CC} ${CFLAGS} ${LDFLAGS} ${OBJ_GC_BENCH} -o $@ lib/libp2.a ${RPATH} ${LIBPTH} ${EXTLIBS} ${LIBS}

bin/p2-test${EXE}: ${OBJ_P2_TEST} lib/libp2.a
	@${ECHO} LINK $@
	@if ${CC} ${CFLAGS} ${LDFLAGS} ${OBJ_P2_TEST} -o $@ lib/libp2.a ${LIBS}; then true; else \
	  ${CC} ${CFLAGS} ${LDFLAGS} ${OBJ_P2_TEST} -o $@ ${OBJ2} ${OBJ_P2_SYN} ${LIBS}; fi

examples: pn p2
	for e in example/*.pn; do echo $$e; time bin/potion $$e; done
	for e in example/*.pl; do echo $$e; time bin/p2 $$e; done

dist: bins libs $(AIO_DEPS) static ${SRC_SYN} ${SRC_P2_SYN} ${GREG}
	@if [ -n "${RPATH}" ]; then \
	  rm -f ${BINS} ${PNLIB}; \
	  +$(MAKE) bins libs RPATH="${RPATH_INSTALL}"; \
	fi
	+$(MAKE) -f dist.mak $@ PREFIX="${PREFIX}" EXE=${EXE} DLL=${DLL} LOADEXT=${LOADEXT}

install: bins libs $(AIO_DEPS) ${GREG}
	+$(MAKE) -f dist.mak $@ PREFIX="${PREFIX}"

tarball:
	+$(MAKE) -f dist.mak $@ PREFIX="${PREFIX}"

release: dist
	+$(MAKE) -f dist.mak $@ PREFIX="${PREFIX}"

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

MANIFEST:
	git ls-tree -r --name-only HEAD > $@

doc: ${DOCHTML} doc/html/files.html
ifeq (${CYGWIN},1)
CHM = doc/html/p2.chm
else
CHM =
endif

docall: doc GTAGS ${CHM}
chm: ${CHM}
DOXY_PRE = doc/footer.sh > doc/footer.inc
DOXY_POST = rm README.md

doxygen: ${DOCHTML} doc/html/files.html
	@${ECHO} DOXYGEN -f core lib
	@perl -pe's/^  //;s/^~ /## ~ /;' README > README.md;
	@${DOXY_PRE}
	@doxygen doc/Doxyfile
	-@${DOXY_POST}
doc/html/index.hhp: doc/html/files.html doc/Doxyfile.chm
	@${ECHO} DOXYGEN doc/html/index.hhp
	@perl -pe's/^  //;s/^~ /## ~ /;' README > README.md;
	@${DOXY_PRE}
	-rm -rf doc/html/*
	@doxygen doc/Doxyfile.chm
	-@${DOXY_POST}
doc/html/p2.chm: doc/html/index.hhp
	@${ECHO} HHC $@
	-cd doc/html; PATH=/cygdrive/c/Program\ Files/HTML\ Help\ Workshop:$PATH hhc index.hhp
doc/html/files.html: ${SRC} doc/Doxyfile doc/footer.sh Makefile
	@${ECHO} DOXYGEN core
	@perl -pe's/^  //;s/^~ /## ~ /;' README > README.md;
	@${DOXY_PRE}
	-rm -rf doc/html/*
	@doxygen doc/Doxyfile 2>&1 |egrep -v "  parameter 'P|self|cl'"
	-@${DOXY_POST}

# perl11.org admins only. requires: doxygen redcloth global
website:
	test -d ${WEBSITE} || exit
	@$(MAKE) doxygen
	cp -r doc/html/* ${WEBSITE}/p2/html/
	@$(MAKE) doc
	cp doc/*.html ${WEBSITE}/p2/
	@$(MAKE) GTAGS
	cp -r HTML/* ${WEBSITE}/p2/ref/
	cd ${WEBSITE}/p2/ && git add *.html html ref && git ci -m'doc: automatic update'
	@${ECHO} "need to cd ${WEBSITE}; git push"

# in seperate clean subdir. do not index work files
GTAGS: ${SRC} core/*.h
	+$(MAKE) -f dist.mak $@ PREFIX=${PREFIX}

TAGS: ${SRC} core/*.h
	@rm -f TAGS
	/usr/bin/find core lib syn front \( -name \*.c -o -name \*.h \) -exec etags -a --language=c \{\} \;

sloc: clean
	@rm -rf dist
	@git checkout-index --prefix=dist/ -a
	@cd dist && \
	  rm syn/greg.c && \
	  sloccount core syn front lib && \
	  cd ..  && \
	rm -rf dist

todo:
	@grep -rInso 'TODO: \(.\+\)' core syn front lib tools

clean:
	@${ECHO} cleaning
	@rm -f $(foreach ext,o o2 opic opic2 i gcda gcno,$(foreach dir,core syn front lib test/api lib/*,${dir}/*.${ext}))
	@rm -f bin/* lib/libpotion.* lib/potion/*${DLL} lib/*/*${LOADEXT} lib/*/*.o front/*.os
	@rm -rf lib/*/*.bundle.dSYM
	@rm -f lib/libp2.* lib/p2/*${DLL}
	@rm -f ${DOCHTML} README.md doc/footer.inc
	@rm -f tools/*.o core/config.h core/version.h
	@rm -f tools/*~ doc/*~ example/*~ core/*~ config.inc~ tools/config.c
	@rm -rf doc/latex

# also config.inc and files needed for cross-compilation
realclean: clean
	@rm -f config.inc ${GREG} ${GREGCROSS} ${SRC_SYN} ${SRC_P2_SYN}
	@rm -f GPATH GTAGS GRTAGS
	@rm -rf doc/ref doc/html
	@rm -rf lib/*${DLL} lib/*${LOADEXT} lib/*.a
	@$(MAKE) -s clean -C 3rd/libuv
	@if test -f 3rd/libuv/Makefile.am; then rm 3rd/libuv/Makefile; fi
	@find . -name \*.gcov -delete

test.c: bin/potion${EXE}
	f=test/classes/creature.pn && \
	look=`${CAT} $$f | ${SED} "/\#=>/!d; s/.*\#=> //"` && \
	for=`bin/potion -I -B $$f | ${SED} "s/\n$$//"` && \
	if [ "$$look" != "$$for" ]; then \
	  ${ECHO} "$$f: expected <$$look>, but got <$$for>"; \
	fi

test.u: bin/p2${EXE}
	f=test/closures/upvals.pl && \
	look=`${CAT} $$f | ${SED} "/\#=>/!d; s/.*\#=> //"` && \
	for=`bin/p2 -I -X $$f | ${SED} "s/\n$$//"` && \
	if [ "$$look" != "$$for" ]; then \
	  ${ECHO} "$$f: expected <$$look>, but got <$$for>"; \
	fi
