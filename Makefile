# posix (linux, bsd, osx, solaris) + mingw with gcc/clang only
.SUFFIXES: .y .c .i .o .opic .textile .html
.PHONY: all default bins libs pn static usage config clean doc rebuild check test bench tarball dist \
        examples release install grammar doxygen website testable
.NOTPARALLEL: test

SRC = core/asm.c core/ast.c core/compile.c core/contrib.c core/gc.c core/internal.c core/lick.c core/mt19937ar.c core/number.c core/objmodel.c core/primitive.c core/string.c core/syntax.c core/table.c core/vm.c
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
PLIBS := $(subst buffile,,${PLIBS})
PLIBS_SRC := $(subst lib/buffile.c,,${PLIBS_SRC})
ifeq ($(WIN32),1)
PLIBS_SRC += lib/readline/win32fixes.c
endif
SRC += ${PLIBS_SRC}
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

FPIC =
OPIC = o
ifneq (${WIN32},1)
  ifneq (${CYGWIN},1)
    FPIC = -fPIC
    OPIC = opic
  else
    PLIBS = $(subst aio,,${PLIBS})
  endif
endif
OBJ = ${SRC:.c=.o}
PIC_OBJ = ${SRC:.c=.${OPIC}}
PIC_OBJ_POTION = core/potion.${OPIC}
OBJ_TEST = test/api/potion-test.o test/api/CuTest.o
OBJ_GC_TEST = test/api/gc-test.o test/api/CuTest.o
OBJ_GC_BENCH = test/api/gc-bench.o
PNLIB = lib/libpotion${DLL}
EXTLIBS = -Llib -luv
ifeq (${WIN32},1)
LIBUV = lib/libuv-11.dll lib/libuv.dll.a
EXTLIBS += -lw32_32
else
LIBUV = lib/libuv${DLL}
endif
EXTLIBDEPS = ${LIBUV}
DYNLIBS = $(foreach m,${PLIBS},lib/potion/$m${LOADEXT})
PLIBS_OBJ = ${PLIBS_SRC:.c=.${OPIC}}
PLIBS_OBJS = ${PLIBS_SRC:.c=.o}
OBJS = o
ifneq (${FPIC},)
  OBJS += ${OPIC}
endif
DOC = doc/start.textile doc/glossary.textile
DOCHTML = ${DOC:.textile=.html}
LIBPNA_AWAY = if [ -f lib/libpotion.a ]; then mv lib/libpotion.a lib/libpotion.a.tmp; fi
LIBPNA_BACK = if [ -f lib/libpotion.a.tmp ]; then mv lib/libpotion.a.tmp lib/libpotion.a; fi

CAT  = /bin/cat
ECHO ?= /bin/echo
MV   = /bin/mv
SED  = sed
EXPR = expr
GREG = bin/greg${EXE}
AR     ?= ar
RANLIB ?= ranlib
ifeq (${CROSS},1)
  GREGCROSS = bin/greg
else
  GREGCROSS = ${GREG}
endif

RUNPRE = bin/
# perl11.org only
WEBSITE = ../perl11.org

default: pn libs
	+${MAKE} -s usage

all: default libs static docall test
ifneq (${SANDBOX},1)
pn: bin/potion${EXE} ${PNLIB}
bins: bin/potion${EXE}
else
pn: static
bins: bin/potion-s${EXE}
endif
libs: ${PNLIB} ${DYNLIBS}
static: lib/libpotion.a bin/potion-s${EXE}
rebuild: clean default test

usage:
	@${ECHO} " "
	@${ECHO} " ~ using potion ~"
	@${ECHO} " "
	@${ECHO} " Running a script or code."
	@${ECHO} " "
	@${ECHO} "   $$ bin/potion example/fib.pn"
	@${ECHO} "   $$ bin/potion -e \"code\""
	@${ECHO} " "
	@${ECHO} " Dump the AST and bytecode inspection for a script. "
	@${ECHO} " "
	@${ECHO} "   $$ bin/potion -V example/fib.pn"
	@${ECHO} " "
	@${ECHO} " Compiling to bytecode."
	@${ECHO} " "
	@${ECHO} "   $$ bin/potion -c example/fib.pn"
	@${ECHO} "   $$ bin/potion example/fib.pnb"
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
	@${ECHO} " Originally written by _why the lucky stiff"
	@${ECHO} " Maintained at https://github.com/perl11/potion"

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

grammar: tools/greg.y
	touch tools/greg.y
	${MAKE} tools/greg.c

# bootstrap tools/greg.c, tools/compile.c not yet
tools/greg.c: tools/greg.y tools/greg.h tools/compile.c tools/tree.c
	@${ECHO} GREG $<
	@if test -f ${GREG}; then ${GREG} tools/greg.y > tools/greg-new.c && \
	  ${CC} ${GREGCFLAGS} -o tools/greg-new tools/greg-new.c tools/compile.c tools/tree.c -Itools && \
	  ${MV} tools/greg-new.c tools/greg.c && \
	  ${MV} tools/greg-new ${GREG}; \
	fi

core/callcc.o: core/callcc.c core/potion.h core/config.h core/internal.h
	@${ECHO} CC $@ -O0 +frame-pointer
	@${CC} -c ${CFLAGS} -O0 -fno-omit-frame-pointer ${INCS} -o $@ $<
core/potion.o: core/potion.c core/potion.h core/config.h core/internal.h \
 core/opcodes.h core/khash.h core/table.h
	@${ECHO} CC $@ -O0
	@${CC} -c ${CFLAGS} -O0 ${INCS} -o $@ $<
ifneq (${FPIC},)
core/callcc.${OPIC}: core/callcc.c core/potion.h core/config.h core/internal.h
	@${ECHO} CC $@ -O0 +frame-pointer
	@${CC} -c ${CFLAGS} -O0 ${FPIC} -fno-omit-frame-pointer ${INCS} -o $@ $<
core/potion.${OPIC}: core/potion.c core/potion.h core/config.h core/internal.h \
 core/opcodes.h core/khash.h core/table.h
	@${ECHO} CC $@ -O0
	@${CC} -c ${CFLAGS} -O0 ${FPIC} ${INCS} -o $@ $<
endif

core/potion.h: core/config.h
core/table.h: core/potion.h core/internal.h core/khash.h
# for c in core/*.c; do gcc -MM -D_GNU_SOURCE  -Icore $c; done |perl -lpe's/^(.+)o:/\$(foreach o,\${OBJS},core\/$1.\${o} ):/'
$(foreach o,${OBJS},core/asm.${o} ): core/asm.c core/potion.h core/config.h core/internal.h \
 core/opcodes.h core/asm.h
$(foreach o,${OBJS},core/ast.${o} ): core/ast.c core/potion.h core/config.h core/internal.h core/ast.h
$(foreach o,${OBJS},core/compile.${o} ): core/compile.c core/potion.h core/config.h core/internal.h \
 core/ast.h core/opcodes.h core/asm.h
$(foreach o,${OBJS},core/contrib.${o} ): core/contrib.c core/config.h
$(foreach o,${OBJS},core/file.${o} ): core/file.c core/potion.h core/config.h core/internal.h \
 core/table.h core/khash.h
$(foreach o,${OBJS},core/gc.${o} ): core/gc.c core/potion.h core/config.h core/internal.h core/gc.h \
 core/khash.h core/table.h
$(foreach o,${OBJS},core/internal.${o} ): core/internal.c core/potion.h core/config.h core/internal.h \
 core/table.h core/khash.h core/gc.h
$(foreach o,${OBJS},core/lick.${o} ): core/lick.c core/potion.h core/config.h core/internal.h
$(foreach o,${OBJS},core/load.${o} ): core/load.c core/potion.h core/config.h core/internal.h \
 core/table.h core/khash.h
$(foreach o,${OBJS},core/mt19937ar.${o} ): core/mt19937ar.c core/potion.h core/config.h
$(foreach o,${OBJS},core/number.${o} ): core/number.c core/potion.h core/config.h core/internal.h
$(foreach o,${OBJS},core/objmodel.${o} ): core/objmodel.c core/potion.h core/config.h core/internal.h \
 core/khash.h core/table.h core/asm.h
$(foreach o,${OBJS},core/primitive.${o} ): core/primitive.c core/potion.h core/config.h core/internal.h
$(foreach o,${OBJS},core/string.${o} ): core/string.c core/potion.h core/config.h core/internal.h \
 core/khash.h core/table.h
$(foreach o,${OBJS},core/table.${o} ): core/table.c core/potion.h core/config.h core/internal.h \
 core/khash.h core/table.h
$(foreach o,${OBJS},core/vm.${o} ): core/vm.c core/potion.h core/config.h core/internal.h \
 core/opcodes.h core/asm.h core/khash.h core/table.h core/vm-dis.c
$(foreach o,${OBJS},core/vm-ppc.${o} ): core/vm-ppc.c core/potion.h core/config.h core/internal.h \
 core/opcodes.h core/asm.h
$(foreach o,${OBJS},core/vm-x86.${o} ): core/vm-x86.c core/potion.h core/config.h core/internal.h \
 core/opcodes.h core/asm.h core/khash.h core/table.h

%.i: %.c core/config.h
	@${ECHO} CPP $@
	@${CC} -c ${CFLAGS} ${INCS} -o $@ -E -c $<
%.in: %.c core/config.h
	@${ECHO} CPP ASTYLE $@
	@${CC} -c ${CFLAGS} ${INCS} -E -c $< | perl -pe's,^# (\d+) ",//# \1 ",' > $@.tmp && \
	  astyle -s2 < $@.tmp > $@
%.o: %.c core/config.h
	@${ECHO} CC $@
	@${CC} -c ${CFLAGS} ${INCS} -o $@ $<
ifneq (${FPIC},)
%.${OPIC}: %.c core/config.h
	@${ECHO} CC $@
	@${CC} -c ${FPIC} ${CFLAGS} ${INCS} -o $@ $<
endif

.c.i: core/config.h
	@${ECHO} CPP $@
	@${CC} -c ${CFLAGS} ${INCS} -o $@ -E -c $<
.c.in: core/config.h
	@${ECHO} CPP ASTYLE $@
	@${CC} -c ${CFLAGS} ${INCS} -E -c $< | perl -pe's,^# (\d+) ",//# \1 ",' > $@.tmp && \
	  astyle -s2 < $@.tmp > $@
.c.o: core/config.h
	@${ECHO} CC $@
	@${CC} -c ${CFLAGS} ${INCS} -o $@ $<
ifneq (${FPIC},)
.c.${OPIC}: core/config.h
	@${ECHO} CC $@
	@${CC} -c ${FPIC} ${CFLAGS} ${INCS} -o $@ $<
endif

%.c: %.y ${GREGCROSS}
	@${ECHO} GREG $@
	@${GREGCROSS} $< > $@-new && ${MV} $@-new $@
.y.c: ${GREGCROSS}
	@${ECHO} GREG $@
	@${GREGCROSS} $< > $@-new && ${MV} $@-new $@

${GREG}: tools/greg.c tools/compile.c tools/tree.c
	@${ECHO} CC $@
	@${CC} ${GREGCFLAGS} -o $@ tools/greg.c tools/compile.c tools/tree.c -Itools

bin/potion${EXE}: ${PIC_OBJ_POTION} lib/libpotion${DLL}
	@${ECHO} LINK $@
	@${LIBPNA_AWAY}
	@${CC} ${CFLAGS} ${LDFLAGS} ${PIC_OBJ_POTION} -o $@ ${LIBPTH} ${RPATH} -lpotion  ${LIBS}
	@${LIBPNA_BACK}
	@if [ "${DEBUG}" != "1" ]; then ${ECHO} STRIP $@; ${STRIP} $@; fi

bin/potion-s${EXE}: core/potion.o lib/libpotion.a ${PLIBS_OBJS}
	@${ECHO} LINK $@
	@${CC} -c ${CFLAGS} ${INCS} -DSTATIC -o core/potion.os core/potion.c
	@${CC} ${CFLAGS} ${LDFLAGS} core/potion.os -o $@ ${PLIBS_OBJS} \
	  lib/libpotion.a ${LIBPTH} ${RPATH} ${EXTLIBS} ${LIBS}
	@if [ "${SANDBOX}" = "1" ]; then rm bin/potion${EXE}; cd bin; ln -s potion-s${EXE} potion${EXE}; cd ..; fi

lib/readline/readline.o: lib/readline/readline.c lib/readline/linenoise.c
	@${ECHO} CC $@
	@${LIBPNA_AWAY}
	@${MAKE} -s -C lib/readline static
	@${LIBPNA_BACK}

lib/libpotion.a: ${OBJ} core/config.h core/potion.h
	@${ECHO} AR $@
	@if [ -e $@ ]; then rm -f $@; fi
	@${AR} rcs $@ ${OBJ} > /dev/null
	@${ECHO} RANLIB $@
	@-${RANLIB} $@

lib/libpotion${DLL}: ${PIC_OBJ} core/config.h core/potion.h
	@${ECHO} LD $@
	@if [ -e $@ ]; then rm -f $@; fi
	@${CC} ${DEBUGFLAGS} -o $@ ${LDDLLFLAGS} ${RPATH} \
	  ${PIC_OBJ} ${LIBPTH} ${LIBS} > /dev/null
	@if [ x${DLL} = x.dll ]; then cp $@ bin/; fi

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
	@if test -f 3rd/libuv/Makefile.am; then \
	  ${MAKE} -s -C 3rd/libuv libuv.la  && \
	  cp 3rd/libuv/.libs/libuv.a lib/; \
	else \
	  ${MAKE} -s -C 3rd/libuv libuv.a && \
	  cp 3rd/libuv/libuv.a lib/; \
	fi
	@touch $@

# default: shared
${LIBUV}: config.inc 3rd/libuv/Makefile
	@${ECHO} MAKE $@
	@if test -f 3rd/libuv/Makefile.am; then \
	  ${MAKE} -s -C 3rd/libuv libuv.la && \
	  rsync -a 3rd/libuv/.libs/libuv*${DLL}* lib/ || cp 3rd/libuv/.libs/libuv.a lib/; \
	else \
	  ${MAKE} -s -C 3rd/libuv libuv${DLL} && \
	  rsync -a 3rd/libuv/libuv*${DLL}* lib/ || cp 3rd/libuv/.libs/libuv.a lib/; \
        fi
	@touch $@

# DYNLIBS
lib/potion/readline${LOADEXT}: core/config.h core/potion.h \
  lib/readline/Makefile lib/readline/linenoise.c \
  lib/readline/linenoise.h lib/libpotion${DLL}
	@${ECHO} MAKE $@
	@${LIBPNA_AWAY}
	@${MAKE} -s -C lib/readline
	@${LIBPNA_BACK}
	@cp lib/readline/readline${LOADEXT} $@

lib/potion/buffile${LOADEXT}: core/config.h core/potion.h \
  lib/buffile.${OPIC} lib/buffile.c lib/libpotion${DLL}
	@${ECHO} LD $@
	@${LIBPNA_AWAY}
	@${CC} $(DEBUGFLAGS) -o $@ $(subst libpotion,potion/buffile,${LDDLLFLAGS}) ${RPATH} \
	  lib/buffile.${OPIC} ${LIBPTH} -lpotion ${LIBS} > /dev/null
	@${LIBPNA_BACK}

ifeq ($(HAVE_LIBUV),1)
AIO_DEPS =
#TODO: libtool?
AIO_DEPLIBS =
else
AIO_DEPS = ${LIBUV}
#AIO_DEPLIBS =
#ifeq ($(WIN32),1)
AIO_DEPLIBS := `perl -ane'/dependency_libs=(.*)/ && print substr($$1,2,-1)' 3rd/libuv/libuv.la`
#endif
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

bench: test/api/gc-bench${EXE} bin/potion${EXE}
	@${ECHO}; \
	  ${ECHO} running GC benchmark; \
	  time test/api/gc-bench

check: test
test: pn libs test/api/potion-test${EXE} test/api/gc-test${EXE}
	@${ECHO}; \
	${ECHO} running potion API tests; \
	test/api/potion-test; \
	count=0; failed=0; pass=0; \
	if $$(grep "SANDBOX = 1" config.inc >/dev/null); then \
	    what=`ls test/**/*.pn|grep -Ev "test/misc/(buffile|load)\.pn"`; \
	else \
	    what=test/**/*.pn; \
	fi; \
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
	  for f in $$what; do \
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

testable : bin/potion${EXE} libs test/api/potion-test${EXE} test/api/gc-test${EXE} test/api/gc-bench${EXE}

# for LTO gold -O4
test/api/potion-test${EXE}: ${OBJ_TEST} lib/libpotion.a
	@${ECHO} LINK $@
	@if ${CC} ${CFLAGS} ${LDFLAGS} ${OBJ_TEST} -o $@ lib/libpotion.a ${RPATH} ${LIBPTH} ${EXTLIBS} ${LIBS}; then true; else \
	  ${CC} ${CFLAGS} ${LDFLAGS} ${OBJ_TEST} -o $@ ${OBJ} ${OBJ_SYN} ${LIBPTH} ${LIBS}; fi

test/api/gc-test${EXE}: ${OBJ_GC_TEST} lib/libpotion.a
	@${ECHO} LINK $@
	@${CC} ${CFLAGS} ${LDFLAGS} ${OBJ_GC_TEST} -o $@ lib/libpotion.a ${RPATH} ${LIBPTH} ${EXTLIBS} ${LIBS}

test/api/gc-bench${EXE}: ${OBJ_GC_BENCH} lib/libpotion.a
	@${ECHO} LINK $@
	@${CC} ${CFLAGS} ${LDFLAGS} ${OBJ_GC_BENCH} -o $@ lib/libpotion.a ${RPATH} ${LIBPTH} ${EXTLIBS} ${LIBS}

examples: pn
	for e in example/*.pn; do echo $$e; time bin/potion $$e; done

dist: bins libs $(AIO_DEPS) static ${GREG}
	@if [ -n "${RPATH}" ]; then \
	  rm -f ${BINS} ${PNLIB}; \
	  ${MAKE} bins libs RPATH="${RPATH_INSTALL}"; \
	fi
	+${MAKE} -f dist.mak $@ PREFIX="${PREFIX}" EXE=${EXE} DLL=${DLL} LOADEXT=${LOADEXT}

install: bins libs $(AIO_DEPS) ${GREG}
	+${MAKE} -f dist.mak $@ PREFIX="${PREFIX}"

tarball:
	+${MAKE} -f dist.mak $@ PREFIX="${PREFIX}"

release: dist
	+${MAKE} -f dist.mak $@ PREFIX="${PREFIX}"

%.html: %.textile
	@${ECHO} DOC $@
	@${ECHO} "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"DTD/xhtml1-transitional.dtd\">" > $@
	@${ECHO} "<html xmlns=\"http://www.w3.org/1999/xhtml\" lang=\"en\" xml:lang=\"en\">" >> $@
	@${ECHO} "<head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />" >> $@
	@${ECHO} "<style type=\"text/css\">@import 'doc.css';</style>" >> $@
	@${ECHO} "<div id='potion'><img src='potion-1.png' /></div>" >> $@
	@${ECHO} "</head><body><div id='central'>" >> $@
	@redcloth $< >> $@
	@${ECHO} "</div></body></html>" >> $@

MANIFEST:
	git ls-tree -r --name-only HEAD > $@

doc: ${DOCHTML} doc/html/files.html
ifeq (${CYGWIN},1)
CHM = doc/html/potion.chm
else
CHM =
endif

docall: doc GTAGS ${CHM}
chm: ${CHM}
DOXY_PRE = doc/footer.sh > doc/footer.inc; \
	test -f core/syntax.c && mv core/syntax.c core/syntax-c.tmp
DOXY_POST = test -f core/syntax-c.tmp && mv core/syntax-c.tmp core/syntax.c; \
	rm README.md

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
doc/html/potion.chm: doc/html/index.hhp
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
	@${MAKE} doxygen
	cp -r doc/html/* ${WEBSITE}/potion/html/
	@${MAKE} doc
	cp doc/*.html ${WEBSITE}/potion/
	@${MAKE} GTAGS
	cp -r doc/ref/* ${WEBSITE}/potion/ref/
	cd ${WEBSITE}/potion/ && git add *.html html ref && git ci -m'doc: automatic update'
	@${ECHO} "need to cd ${WEBSITE}; git push"

# in seperate clean subdir. do not index work files
GTAGS: ${SRC} core/*.h
	+${MAKE} -f dist.mak $@ PREFIX=${PREFIX}

TAGS: ${SRC} core/*.h
	@rm -f TAGS
	/usr/bin/find core lib \( -name \*.c -o -name \*.h \) -exec etags -a --language=c \{\} \;

sloc: clean
	@rm -rf dist
	@git checkout-index --prefix=dist/ -a
	@cd dist && \
	  rm tools/greg.c && \
	  sloccount core lib && \
	  cd ..  && \
	rm -rf dist

todo:
	@grep -rInso 'TODO: \(.\+\)' core tools lib

clean:
	@${ECHO} cleaning
	@rm -f core/*.o core/*.opic core/*.i test/api/*.o core/potion.os
	@rm -f bin/* lib/libpotion.* lib/potion/*${DLL} lib/*/*${LOADEXT} lib/*/*.o lib/*.o lib/*.opic
	@rm -rf lib/*/*.bundle.dSYM
	@rm -f ${DOCHTML} README.md doc/footer.inc
	@rm -f bin/potion${EXE} bin/potion-s${EXE} \
	  test/api/potion-test${EXE} test/api/gc-test${EXE} test/api/gc-bench${EXE}
	@rm -f tools/*.o core/config.h core/version.h
	@rm -f tools/*~ doc/*~ example/*~ core/*~ config.inc~ tools/config.c
	@rm -rf doc/latex

# also config.inc and files needed for cross-compilation
realclean: clean
	@rm -f config.inc ${GREG} ${GREGCROSS} core/syntax.c
	@rm -f GPATH GTAGS GRTAGS
	@rm -rf doc/ref doc/html
	@rm -rf lib/*${DLL} lib/*${LOADEXT} lib/*.a
	@${MAKE} -s clean -C 3rd/libuv
	@if test -f 3rd/libuv/Makefile.am; then rm 3rd/libuv/Makefile; fi
	@find . -name \*.gcov -delete

test.c: bin/potion${EXE}
	f=test/classes/creature.pn && \
	look=`${CAT} $$f | ${SED} "/\#=>/!d; s/.*\#=> //"` && \
	for=`bin/potion -I -B $$f | ${SED} "s/\n$$//"` && \
	if [ "$$look" != "$$for" ]; then \
	  ${ECHO} "$$f: expected <$$look>, but got <$$for>"; \
	fi

test.u: bin/potion${EXE}
	f=test/closures/upvals.pn && \
	look=`${CAT} $$f | ${SED} "/\#=>/!d; s/.*\#=> //"` && \
	for=`bin/potion -I -X $$f | ${SED} "s/\n$$//"` && \
	if [ "$$look" != "$$for" ]; then \
	  ${ECHO} "$$f: expected <$$look>, but got <$$for>"; \
	fi
