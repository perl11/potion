# posix (linux, bsd, osx, solaris) + mingw with gcc/clang only
.SUFFIXES: .y .c .i .o .opic .textile .html
.PHONY: all pn static usage config clean doc rebuild test bench tarball dist release install grammar

SRC = core/asm.c core/ast.c core/callcc.c core/compile.c core/contrib.c core/file.c core/gc.c core/internal.c core/lick.c core/load.c core/mt19937ar.c core/number.c core/objmodel.c core/primitive.c core/string.c core/syntax.c core/table.c core/vm.c
GREGCFLAGS = -O3 -DNDEBUG

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

INCS = -Icore
# perl11.org only
WEBSITE = ../perl11.org

all: pn
	+${MAKE} -s usage

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
	  ${MV} tools/greg-new tools/greg; \
	fi

core/callcc.o: core/callcc.c core/config.h
	@${ECHO} CC $@ +frame-pointer
	@${CC} -c ${CFLAGS} -fno-omit-frame-pointer ${INCS} -o $@ $<

core/callcc.opic: core/callcc.c core/config.h
	@${ECHO} CC $@ +frame-pointer
	@${CC} -c ${CFLAGS} ${FPIC} -fno-omit-frame-pointer ${INCS} -o $@ $<

core/vm.o core/vm.opic: core/vm-dis.c core/config.h

# no optimizations
#core/vm-x86.opic: core/vm-x86.c
#	@${ECHO} CC ${FPIC} $@ +frame-pointer
#	@${CC} -c -g3 -fstack-protector -fno-omit-frame-pointer -Wall -fno-strict-aliasing -Wno-return-type# -D_GNU_SOURCE ${FPIC} ${INCS} -o $@ $<

%.i: %.c core/config.h
	@${ECHO} CPP $@
	@${CC} -c ${CFLAGS} ${INCS} -o $@ -E -c $<
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
.c.o: core/config.h
	@${ECHO} CC $@
	@${CC} -c ${CFLAGS} ${INCS} -o $@ $<
ifneq (${FPIC},)
.c.${OPIC}: core/config.h
	@${ECHO} CC $@
	@${CC} -c ${FPIC} ${CFLAGS} ${INCS} -o $@ $<
endif

%.c: %.y ${GREG}
	@${ECHO} GREG $@
	@${GREG} $< > $@-new && ${MV} $@-new $@
.y.c: ${GREG}
	@${ECHO} GREG $@
	@${GREG} $< > $@-new && ${MV} $@-new $@

${GREG}: tools/compile.c tools/tree.c
	@${ECHO} CC $@
	@${CC} ${GREGCFLAGS} -o $@ tools/greg.c tools/compile.c tools/tree.c -Itools

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
	@${ECHO} LINK $@
	@${CC} ${CFLAGS} ${OBJ_POTION} -o $@ ${LIBPTH} ${RPATH} -lpotion ${LIBS}
	@if [ "${DEBUG}" != "1" ]; then \
		${ECHO} STRIP $@; \
	  ${STRIP} $@; \
	fi

potion-s${EXE}: core/potion.o libpotion.a ${LIBHACK}
	@${ECHO} LINK $@
	@${CC} ${CFLAGS} core/potion.o -o $@ ${LIBPTH} ${RPATH} libpotion.a ${LIBS}

libpotion.a: ${OBJ} core/config.h core/potion.h
	@${ECHO} AR $@
	@if [ -e $@ ]; then rm -f $@; fi
	@${AR} rcs $@ core/*.o > /dev/null

libpotion${DLL}: ${PIC_OBJ} core/config.h core/potion.h
	@${ECHO} LD $@
	@if [ -e $@ ]; then rm -f $@; fi
	@${CC} ${DEBUGFLAGS} -o $@ ${LDDLLFLAGS} ${RPATH} \
	  ${PIC_OBJ} ${LIBS} > /dev/null

lib/readline${LOADEXT}: core/config.h core/potion.h \
  lib/readline/Makefile lib/readline/linenoise.c \
  lib/readline/linenoise.h
	@${ECHO} MAKE $@
	@${MAKE} -s -C lib/readline
	@cp lib/readline/readline${LOADEXT} $@

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

test/api/potion-test${EXE}: ${OBJ_TEST} ${OBJ}
	@${ECHO} LINK potion-test
	@${CC} ${CFLAGS} ${OBJ_TEST} ${OBJ} ${LIBS} -o $@

test/api/gc-test${EXE}: ${OBJ_GC_TEST} ${OBJ}
	@${ECHO} LINK gc-test
	@${CC} ${CFLAGS} ${OBJ_GC_TEST} ${OBJ} ${LIBS} -o $@

test/api/gc-bench${EXE}: ${OBJ_GC_BENCH} ${OBJ}
	@${ECHO} LINK gc-bench
	@${CC} ${CFLAGS} ${OBJ_GC_BENCH} ${OBJ} ${LIBS} -o $@

dist: pn static docall
	+${MAKE} -f dist.mak $@ PREFIX=${PREFIX} EXE=${EXE} DLL=${DLL} LOADEXT=${LOADEXT}

install: dist
	+${MAKE} -f dist.mak $@ PREFIX=${PREFIX}

tarball: dist
	+${MAKE} -f dist.mak $@ PREFIX=${PREFIX}

release: dist
	+${MAKE} -f dist.mak $@ PREFIX=${PREFIX}

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
docall: doc GTAGS

doxygen: doc/html/files.html
	@${ECHO} DOXYGEN core
	@perl -pe's/^  //;s/^~ /## ~ /;' README > README.md
	@doc/footer.sh > doc/footer.inc
	@doxygen doc/Doxyfile

doc/html/files.html: ${SRC} core/*.h doc/Doxyfile doc/footer.sh potion${EXE}
	@${ECHO} DOXYGEN
	doc/footer.sh > doc/footer.inc
	@doxygen doc/Doxyfile 2>&1 |egrep -v "  parameter 'P|self|cl'"

# perl11.org only. needs: doxygen redcloth global
website:
	test -d ${WEBSITE} || exit
	@${MAKE} doc
	cp doc/*.html ${WEBSITE}/potion/
	@${MAKE} doxygen
	cp -r doc/html/* ${WEBSITE}/potion/html/
	@${MAKE} GTAGS
	cp -r HTML/* ${WEBSITE}/potion/ref/
	cd ${WEBSITE}/potion/ && git add *.html html ref && git ci -m'doc: automatic update'
	@${ECHO} "need to cd ${WEBSITE}; git push"

# in seperate clean subdir. do not index work files
GTAGS: ${SRC} core/*.h
	+${MAKE} -f dist.mak $@

TAGS: ${SRC} core/*.h
	@rm -f TAGS
	/usr/bin/find  \( -name \*.c -o -name \*.h \) -exec etags -a --language=c \{\} \;

sloc: clean
	@sloccount core

todo:
	@grep -rInso 'TODO: \(.\+\)' core tools

clean:
	@${ECHO} cleaning
	@rm -f core/*.o core/*.opic core/*.i test/api/*.o
	@rm -f tools/*.o tools/*~ doc/*~ example/*~ tools/config.c
	@rm -f ${DOCHTML}
	@rm -f core/config.h core/version.h
	@rm -f potion${EXE} potion-s${EXE} libpotion.* \
	  test/api/potion-test${EXE} test/api/gc-test${EXE} test/api/gc-bench${EXE}
	@rm -rf doc/html doc/latex
	@rm -f lib/readline${LOADEXT} lib/readline/readline${LOADEXT}

realclean: clean
	@rm -f config.inc ${GREG} core/syntax.c
	@rm -f GPATH GTAGS GRTAGS
	@rm -rf HTML
	@find . -name \*.gcov -delete

.PHONY: all config clean doc docall rebuild check test test.pn test.p2 bench tarball dist \
        release install grammar doxygen website
