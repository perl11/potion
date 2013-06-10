# posix (linux, bsd, osx, solaris) + mingw with gcc/clang only
.SUFFIXES: .y .c .i .o .opic .textile .html
.PHONY: all pn static usage config clean doc rebuild test bench tarball dist release install

SRC = core/asm.c core/ast.c core/callcc.c core/compile.c core/contrib.c core/file.c core/gc.c core/internal.c core/lick.c core/load.c core/mt19937ar.c core/number.c core/objmodel.c core/primitive.c core/string.c core/syntax.c core/table.c core/vm.c

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

V = 0
FPIC =
OPIC = o
ifneq (${WIN32},1)
  ifneq (${CYGWIN},1)
    FPIC = -fPIC
    OPIC = opic
  endif
endif
OBJ = ${SRC:.c=.o}
PIC_OBJ = ${SRC:.c=.${OPIC}}
OBJ_POTION = core/potion.${OPIC}
OBJ_TEST = test/api/potion-test.o test/api/CuTest.o
OBJ_GC_TEST = test/api/gc-test.o test/api/CuTest.o
OBJ_GC_BENCH = test/api/gc-bench.o
PLIBS = libpotion${DLL} lib/readline${LOADEXT}
DOC = doc/start.textile
DOCHTML = ${DOC:.textile=.html}

CAT  = /bin/cat
ECHO = /bin/echo
MV   = /bin/mv
SED  = sed
EXPR = expr
GREG = tools/greg${EXE}
RANLIB ?= ranlib
RUNPRE ?= ./

ifeq ("$(V)","1")
  Q :=
  VECHO = @true
  devnull =
  MAKE_S  = $(MAKE)
else
  Q := @
  VECHO = @echo
  devnull = > /dev/null
  MAKE_S  = $(MAKE) -s
endif

all: pn
	+$(Q)$(MAKE) -s usage

pn: potion${EXE} ${PLIBS}
static: libpotion.a potion-s${EXE}
rebuild: clean pn test

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
	@${ECHO} " Originally written by _why the lucky stiff"
	@${ECHO} " Maintained at https://github.com/fogus/potion"

config:
	$(VECHO) MAKE -f config.mak $@
	$(Q)$(MAKE) -s -f config.mak

# bootstrap config.inc
config.inc: tools/config.sh config.mak
	$(VECHO) MAKE -f config.mak $@
	$(Q)$(MAKE) -s -f config.mak $@ V=$(V)

core/config.h: config.inc core/version.h tools/config.sh config.mak
	$(VECHO) MAKE -f config.mak $@
	$(Q)$(MAKE) -s -f config.mak $@ V=$(V)

core/version.h: config.mak $(shell git show-ref HEAD | ${SED} "s,^.* ,.git/,g")
	$(VECHO) MAKE -f config.mak $@
	$(Q)$(MAKE) -s -f config.mak $@ V=$(V)

core/callcc.o: core/callcc.c
	$(VECHO) CC +frame-pointer $@
	$(Q)$(CC) -c ${CFLAGS} -fno-omit-frame-pointer ${INCS} -o $@ $<

core/callcc.opic: core/callcc.c
	$(VECHO) CC +frame-pointer $@
	$(Q)$(CC) -c ${CFLAGS} ${FPIC} -fno-omit-frame-pointer ${INCS} -o $@ $<

core/vm.o core/vm.opic: core/vm-dis.c

# no optimizations
#core/vm-x86.opic: core/vm-x86.c
#	@${ECHO} CC ${FPIC} $@ +frame-pointer
#	@${CC} -c -g3 -fstack-protector -fno-omit-frame-pointer -Wall -fno-strict-aliasing -Wno-return-type# -D_GNU_SOURCE ${FPIC} ${INCS} -o $@ $<

%.i: %.c core/config.h
	$(VECHO) CPP $@
	$(Q)$(CC) -c ${CFLAGS} ${INCS} -o $@ -E -c $<
%.o: %.c core/config.h
	$(VECHO) CC $@
	$(Q)$(CC) -c ${CFLAGS} ${INCS} -o $@ $<
ifneq (${FPIC},)
%.${OPIC}: %.c core/config.h
	$(VECHO) CC $@
	$(Q)$(CC) -c ${FPIC} ${CFLAGS} ${INCS} -o $@ $<
endif

.c.i: core/config.h
	$(VECHO) CPP $@
	$(Q)$(CC) -c ${CFLAGS} ${INCS} -o $@ -E -c $<
.c.o: core/config.h
	$(VECHO) CC $@
	$(Q)$(CC) -c ${CFLAGS} ${INCS} -o $@ $<
ifneq (${FPIC},)
.c.${OPIC}: core/config.h
	$(VECHO) CC $@
	$(Q)$(CC) -c ${FPIC} ${CFLAGS} ${INCS} -o $@ $<
endif

%.c: %.y $(GREG)
	$(VECHO) GREG $@
	$(Q)$(GREG) $< > $@-new && ${MV} $@-new $@
.y.c: $(GREG)
	$(VECHO) GREG $@
	$(Q)$(GREG) $< > $@-new && ${MV} $@-new $@

$(GREG): tools/greg.c tools/compile.c tools/tree.c
	$(Q)$(CC) -O3 -DNDEBUG -o $@ tools/greg.c tools/compile.c tools/tree.c -Itools

# the installed version assumes bin/potion loading from ../lib/libpotion (relocatable)
# on darwin we generate a parallel potion/../lib to use @executable_path/../lib/libpotion
ifeq (${APPLE},1)
LIBHACK = ../lib/libpotion.dylib
else
LIBHACK =
endif
../lib/libpotion.dylib:
	-mkdir ../lib
	-ln -s `pwd`/libpotion.dylib ../lib/

potion${EXE}: ${OBJ_POTION} libpotion${DLL} ${LIBHACK}
	$(VECHO) LINK $@
	$(Q)$(CC) ${CFLAGS} ${OBJ_POTION} -o $@ ${LIBPTH} ${RPATH} -lpotion ${LIBS}
	@if [ "${DEBUG}" != "1" ]; then \
		${ECHO} STRIP $@; \
	  ${STRIP} $@; \
	fi

potion-s${EXE}: core/potion.o libpotion.a ${LIBHACK}
	$(VECHO) LINK $@
	$(Q)$(CC) ${CFLAGS} core/potion.o -o $@ ${LIBPTH} ${RPATH} libpotion.a ${LIBS}

libpotion.a: ${OBJ} core/config.h core/potion.h
	$(VECHO) AR $@
	@if [ -e $@ ]; then rm -f $@; fi
	$(Q)$(AR) rcs $@ core/*.o $(devnull)

libpotion${DLL}: ${PIC_OBJ} core/config.h core/potion.h
	$(VECHO) LD $@
	@if [ -e $@ ]; then rm -f $@; fi
	$(Q)$(CC) ${DEBUGFLAGS} -o $@ ${LDDLLFLAGS} ${RPATH} \
	  ${PIC_OBJ} ${LIBS} $(devnull)

lib/readline${LOADEXT}: core/config.h core/potion.h \
  lib/readline/Makefile lib/readline/linenoise.c \
  lib/readline/linenoise.h
	${VECHO} MAKE $@
	$(Q)$(MAKE_S) -C lib/readline
	$(Q)cp lib/readline/readline${LOADEXT} $@

bench: potion${EXE} test/api/gc-bench${EXE}
	@${ECHO}; \
	  ${ECHO} running GC benchmark; \
	  time test/api/gc-bench

test: potion${EXE} test/api/potion-test${EXE} test/api/gc-test${EXE}
	@${ECHO}; \
	${ECHO} running API tests; \
	DYLD_LIBRARY_PATH=`pwd`:$DYLD_LIBRARY_PATH \
	export DYLD_LIBRARY_PATH; \
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
			 jit=`${RUNPRE}potion -v | ${SED} "/jit=1/!d"`; \
			 if [ "$$jit" = "" ]; then \
			   ${ECHO} skipping; \
			   break; \
			 fi; \
		fi; \
		for f in test/**/*.pn; do \
			look=`${CAT} $$f | ${SED} "/\#=>/!d; s/.*\#=> //"`; \
			if [ $$pass -eq 0 ]; then \
				for=`${RUNPRE}potion -I -B $$f | ${SED} "s/\n$$//"`; \
			elif [ $$pass -eq 1 ]; then \
				${RUNPRE}potion -c $$f $(devnull); \
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

test/api/potion-test${EXE}: ${OBJ_TEST} ${OBJ}
	$(VECHO) LINK potion-test
	$(Q)$(CC) ${CFLAGS} ${OBJ_TEST} ${OBJ} ${LIBS} -o $@

test/api/gc-test${EXE}: ${OBJ_GC_TEST} ${OBJ}
	$(VECHO) LINK gc-test
	$(Q)$(CC) ${CFLAGS} ${OBJ_GC_TEST} ${OBJ} ${LIBS} -o $@

test/api/gc-bench${EXE}: ${OBJ_GC_BENCH} ${OBJ}
	$(VECHO) LINK gc-bench
	$(Q)$(CC) ${CFLAGS} ${OBJ_GC_BENCH} ${OBJ} ${LIBS} -o $@

dist: pn static docall
	+${MAKE} -f dist.mak $@ PREFIX=${PREFIX} EXE=${EXE} DLL=${DLL} LOADEXT=${LOADEXT}

install: dist
	+${MAKE} -f dist.mak $@ PREFIX=${PREFIX}

tarball: dist
	+${MAKE} -f dist.mak $@ PREFIX=${PREFIX}

release: dist
	+${MAKE} -f dist.mak $@ PREFIX=${PREFIX}

%.html: %.textile
	$(VECHO) DOC $@
	$(Q)$(ECHO) "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"DTD/xhtml1-transitional.dtd\">" > $@
	$(Q)$(ECHO) "<html xmlns=\"http://www.w3.org/1999/xhtml\" lang=\"en\" xml:lang=\"en\">" >> $@
	$(Q)$(ECHO) "<head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />" >> $@
	$(Q)$(ECHO) "<style type=\"text/css\">@import 'doc.css';</style>" >> $@
	$(Q)$(ECHO) "<div id='potion'><img src='potion-1.png' /></div>" >> $@
	$(Q)$(ECHO) "</head><body><div id='central'>" >> $@
	$(Q)redcloth $< >> $@
	$(Q)$(ECHO) "</div></body></html>" >> $@

doc: ${DOCHTML}
docall: doc GTAGS

MANIFEST:
	git ls-tree -r --name-only HEAD > $@

# in seperate clean subdir. do not index work files
GTAGS: ${SRC} core/*.h
	+${MAKE} -f dist.mak $@

TAGS: ${SRC} core/*.h
	$(Q)rm -f TAGS
	$(Q)/usr/bin/find  \( -name \*.c -o -name \*.h \) -exec etags -a --language=c \{\} \;

sloc: clean
	sloccount core

todo:
	$(Q)grep -rInso 'TODO: \(.\+\)' core tools

clean:
	$(VECHO) cleaning
	$(Q)rm -f core/*.o core/*.opic core/*.i test/api/*.o ${DOCHTML}
	$(Q)rm -f tools/*.o tools/*~ doc/*~ example/*~
	$(Q)rm -f core/config.h core/version.h
	$(Q)rm -f potion${EXE} potion-s${EXE} libpotion.* \
	  test/api/potion-test${EXE} test/api/gc-test${EXE} test/api/gc-bench${EXE}
	$(Q)$(MAKE_S) -C lib/readline clean
	$(Q)rm -rf doc/html doc/latex

realclean: clean
	$(Q)rm -f config.inc $(GREG) core/syntax.c
	$(Q)rm -f GPATH GTAGS GRTAGS
	$(Q)rm -rf HTML
	$(Q)find . -name \*.gcov -delete
