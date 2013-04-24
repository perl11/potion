# posix (linux, bsd, osx, solaris) + mingw with gcc/clang only
.SUFFIXES: .y .c .i .i2 .o .opic .o2 .opic2 .textile .html

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
OBJ_P2 = ${SRC_P2:.c=.opic}
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

GREGCFLAGS = -O3 -DNDEBUG
CAT  = /bin/cat
ECHO = /bin/echo
MV   = /bin/mv
SED  = sed
EXPR = expr
GREG = bin/greg${EXE}

RUNPRE = bin/
INCS = -Icore
# perl11.org only
WEBSITE = ../perl11.org

all: pn bin/p2${EXE}
	+${MAKE} -s usage

pn: bin/potion${EXE} lib/potion/readline${LOADEXT}

rebuild: clean bin/potion${EXE} test

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
	@${MAKE} -s -f config.mak

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

core/callcc.o core/callcc.o2: core/callcc.c core/config.h
	@${ECHO} CC $< +frame-pointer
	@${CC} -c ${CFLAGS} -fno-omit-frame-pointer ${INCS} -o $@ $<

core/callcc.opic core/callcc.opic2: core/callcc.c core/config.h
	@${ECHO} CC -fPIC $< +frame-pointer
	@${CC} -c ${CFLAGS} -fPIC -fno-omit-frame-pointer ${INCS} -o $@ $<

core/vm.o core/vm.opic: core/vm-dis.c core/config.h

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

%.c: %.y ${GREG}
	@${ECHO} GREG $<
	@${GREG} $< > $@-new && ${MV} $@-new $@
.y.c: ${GREG}
	@${ECHO} GREG $<
	@${GREG} $< > $@-new && ${MV} $@-new $@

# the installed version assumes bin/potion loading from ../lib/libpotion (relocatable)
# on darwin we generate a parallel p2/../lib to use @executable_path/../lib/libpotion
#ifeq (${APPLE},1)
#LIBHACK  = ../lib/libpotion.dylib ../lib/potion/libsyntax.dylib
#LIBHACK2 = ../lib/libp2.dylib ../lib/potion/libsyntax-p5.dylib
#else
LIBHACK  =
LIBHACK2 =
#endif
#../lib/libpotion.dylib ../lib/potion/libsyntax.dylib:
#	-mkdir -p ../lib/potion
#	-ln -sf `pwd`/libpotion.dylib ../lib/
#	-ln -sf `pwd`/lib/potion/libsyntax.dylib ../lib/potion/
#../lib/libp2.dylib ../lib/potion/libsyntax-p5.dylib:
#	-mkdir -p ../lib/potion
#	-ln -sf `pwd`/libp2.dylib ../lib/
#	-ln -sf `pwd`/lib/potion/libsyntax-p5.dylib ../lib/potion/

bin/potion${EXE}: ${PIC_OBJ_POTION} lib/libpotion${DLL}
	@${ECHO} LINK $@
	@[ -d bin ] || mkdir bin
	@${CC} ${CFLAGS} ${PIC_OBJ_POTION} -o $@ ${LIBPTH} ${RPATH} \
	  -lsyntax -lpotion ${LIBS}
	@if [ "${DEBUG}" != "1" ]; then \
		${ECHO} STRIP $@; \
	  ${STRIP} $@; \
	fi

bin/p2${EXE}: ${OBJ_P2} lib/libp2${DLL} ${LIBHACK2}
	@${ECHO} LINK $@
	@[ -d bin ] || mkdir bin
	@${CC} ${CFLAGS} ${OBJ_P2} -o $@ ${LIBPTH} ${RPATH} \
	  -lsyntax-p5 -lp2 ${LIBS}
	@if [ "${DEBUG}" != "1" ]; then \
		${ECHO} STRIP $@; \
	  ${STRIP} $@; \
	fi

${GREG}: syn/greg.c syn/compile.c syn/tree.c
	@${ECHO} CC $@
	@[ -d bin ] || mkdir bin
	@${CC} ${GREGCFLAGS} -o $@ syn/greg.c syn/compile.c syn/tree.c -Isyn

bin/potion-s${EXE}: ${OBJ_POTION} lib/libpotion.a ${LIBHACK}
	@${ECHO} LINK $@
	@[ -d bin ] || mkdir bin
	@${CC} ${CFLAGS} ${OBJ_POTION} -o $@ ${LIBPTH} lib/libpotion.a ${LIBS}

bin/p2-s${EXE}: ${OBJ_P2} lib/libp2.a ${LIBHACK}
	@${ECHO} LINK $@
	@[ -d bin ] || mkdir bin
	@${CC} ${CFLAGS} ${OBJ_P2} -o $@ ${LIBPTH} lib/libp2.a ${LIBS}

lib/libpotion.a: ${OBJ_SYN} ${OBJ} core/config.h core/potion.h
	@${ECHO} AR $@
	@if [ -e $@ ]; then rm -f $@; fi
	@${AR} rcs $@ ${OBJ_SYN} ${OBJ} > /dev/null

lib/libpotion${DLL}: ${PIC_OBJ} lib/potion/libsyntax${DLL} core/config.h core/potion.h
	@${ECHO} LD $@
	@if [ -e $@ ]; then rm -f $@; fi
	@${CC} ${DEBUGFLAGS} -o $@ ${LDDLLFLAGS} ${RPATH} \
	  ${PIC_OBJ} ${LIBPTH} -lsyntax ${LIBS} > /dev/null

lib/libp2.a: ${OBJ_P2_SYN} $(subst .o,.o2,${OBJ}) core/config.h core/potion.h
	@${ECHO} AR $@
	@if [ -e $@ ]; then rm -f $@; fi
	@${AR} rcs $@ ${OBJ_P2_SYN} $(subst .o,.o2,${OBJ}) > /dev/null

lib/libp2${DLL}: $(subst .opic,.opic2,${PIC_OBJ}) lib/potion/libsyntax-p5${DLL} core/config.h core/potion.h
	@${ECHO} LD $@
	@if [ -e $@ ]; then rm -f $@; fi
	@${CC} ${DEBUGFLAGS} -o $@ $(subst libpotion,libp2,${LDDLLFLAGS}) ${RPATH} \
	  $(subst .opic,.opic2,${PIC_OBJ}) ${LIBPTH} -lsyntax-p5 ${LIBS} > /dev/null

lib/potion/libsyntax${DLL}: syn/syntax.opic
	@${ECHO} LD $@
	@[ -d lib/potion ] || mkdir lib/potion
	@$(CC) ${DEBUGFLAGS} -o $@ \
	  $(subst libpotion,potion/libsyntax,${LDDLLFLAGS}) ${RPATH} \
	  $(INCS) $< $(LIBS)

lib/potion/libsyntax-p5${DLL}: syn/syntax-p5.opic2
	@${ECHO} LD $@
	@[ -d lib/potion ] || mkdir lib/potion
	@${CC} ${DEBUGFLAGS} -o $@ ${LDDLLFLAGS} \
	  $(subst libpotion,potion/libsyntax-p5,${LDDLLFLAGS}) ${RPATH} \
	  $(INCS) $< $(LIBS)

lib/potion/readline${LOADEXT}: core/config.h core/potion.h \
  lib/readline/Makefile lib/readline/linenoise.c \
  lib/readline/linenoise.h
	@${ECHO} MAKE $@
	@${MAKE} -s -C lib/readline
	@[ -d lib/potion ] || mkdir lib/potion
	@cp lib/readline/readline${LOADEXT} $@

bench: bin/gc-bench${EXE} bin/potion${EXE}
	@${ECHO}; \
	  ${ECHO} running GC benchmark; \
	  time bin/gc-bench

check: test.pn test.p2

test: test.pn test.p2

test.pn: bin/potion${EXE} bin/potion-test${EXE}
	@${ECHO}; \
	${ECHO} running potion API tests; \
	LD_LIBRARY_PATH=`pwd`/lib:`pwd`/lib/potion:$LD_LIBRARY_PATH \
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

bin/potion-test${EXE}: ${OBJ_TEST} lib/libpotion.a
	@${ECHO} LINK potion-test
	@${CC} ${CFLAGS} ${OBJ_TEST} -o $@ lib/libpotion.a ${LIBS}

bin/gc-test${EXE}: ${OBJ_GC_TEST} lib/libp2.a
	@${ECHO} LINK gc-test
	@${CC} ${CFLAGS} ${OBJ_GC_TEST} -o $@ lib/libp2.a ${LIBS}

bin/gc-bench${EXE}: ${OBJ_GC_BENCH} lib/libp2.a
	@${ECHO} LINK gc-bench
	@${CC} ${CFLAGS} ${OBJ_GC_BENCH} -o $@ lib/libp2.a ${LIBS}

bin/p2-test${EXE}: ${OBJ_P2_TEST} lib/libp2.a
	@${ECHO} LINK p2-test
	@${CC} ${CFLAGS} ${OBJ_P2_TEST} -o $@ lib/libp2.a ${LIBS}

dist: bin/potion${EXE} bin/p2${EXE} lib/libpotion.a lib/libp2.a bin/p2-s bin/potion-s doc
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

doc: ${DOCHTML} doc/html/files.html

doxygen: doc/html/files.html
	@${ECHO} DOXYGEN -f core
	@perl -pe's/^  //;s/^~ /## ~ /;' README > README.md
	@doc/footer.sh > doc/footer.inc
	@doxygen doc/Doxyfile

doc/html/files.html: ${SRC} core/*.h doc/Doxyfile doc/footer.sh bin/p2${EXE}
	@${ECHO} DOXYGEN core
	@perl -pe's/^  //;s/^~ /## ~ /;' README > README.md
	doc/footer.sh > doc/footer.inc
	@doxygen doc/Doxyfile 2>&1 |egrep -v "  parameter 'P|self|cl'"

# perl11.org only. needs doxygen redcloth global
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

# in seperate clean subdir. do not index work files
GTAGS: ${SRC} core/*.h
	+${MAKE} -f dist.mak $@ PREFIX=${PREFIX}

TAGS: ${SRC} core/*.h
	@rm -f TAGS
	/usr/bin/find  \( -name \*.c -o -name \*.h \) -exec etags -a --language=c \{\} \;

sloc: clean
	@mv syn/greg.c syn/greg-c.tmp
	@sloccount core syn front
	@mv syn/greg-c.tmp syn/greg.c

todo:
	@grep -rInso 'TODO: \(.\+\)' core syn front

clean:
	@${ECHO} cleaning
	@rm -f core/*.o test/api/*.o front/*.o syn/*.i syn/*.o syn/*.opic \
	       core/*.i core/*.opic core/*.opic2 core/*.o2 front/*.opic
	@rm -f bin/* lib/potion/* lib/libpotion${DLL} lib/libp2${DLL}
	@rm -f ${DOCHTML} README.md doc/footer.inc
	@rm -f ${GREG} tools/*.o core/config.h core/version.h ${SRC_SYN}
	@rm -f tools/*~ doc/*~ example/*~ tools/config.c
	@rm -f lib/readline/readline${LOADEXT}
	@rm -f ${SRC_P2_SYN}
	@rm -rf doc/html doc/latex HTML

realclean: clean
	@rm -f config.inc

.PHONY: all config clean doc rebuild check test test.pn test.p2 bench tarball dist release \
	install grammar doxygen website
