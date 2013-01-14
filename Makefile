.SUFFIXES: .g .c .o

SRC = core/asm.c core/ast.c core/callcc.c core/compile.c core/contrib.c core/file.c core/gc.c core/internal.c core/lick.c core/load.c core/mt19937ar.c core/number.c core/objmodel.c core/primitive.c core/string.c core/syntax.c core/table.c core/vm.c core/vm-ppc.c core/vm-x86.c
OBJ = ${SRC:.c=.o}
PIC_OBJ = ${SRC:.c=.opic}
OBJ_POTION = core/potion.o
OBJ_TEST = test/api/potion-test.o test/api/CuTest.o
OBJ_GC_TEST = test/api/gc-test.o test/api/CuTest.o
OBJ_GC_BENCH = test/api/gc-bench.o
DOC = doc/start.textile
DOCHTML = ${DOC:.textile=.html}

PREFIX = /usr/local
CC = gcc
CFLAGS = -Wall -fno-strict-aliasing -Wno-return-type -D_GNU_SOURCE
AR ?= ar
DEBUG ?= 0
ECHO = /bin/echo
GREG = tools/greg
INCS = -Icore
JIT ?= 1
LIBS = -lm -ldl
# http://bastard.sourceforge.net/libdisasm.html
if HAVE_LIBDISASM
LIBS += -ldisasm
endif
STRIP ?= `./tools/config.sh ${CC} strip`

# TODO: -O2 doesn't include -fno-stack-protector
DEBUGFLAGS = `${ECHO} "${DEBUG}" | sed "s/0/-O2/; s/1/-g -DDEBUG/"`
CFLAGS += ${DEBUGFLAGS}

VERSION = $(shell ./tools/config.sh ${CC} version)
DATE = $(shell date +%Y-%m-%d)
REVISION = $(shell git rev-list HEAD | wc -l | sed "s/ //g")
COMMIT = $(shell git rev-list HEAD -1 --abbrev=7 --abbrev-commit)
RELEASE ?= ${VERSION}.${REVISION}
PKG = potion-${RELEASE}

ifeq ($(shell ./tools/config.sh ${CC} mingw),0)
	CFLAGS += -rdynamic
else
	INCS += -Itools/dlfcn-win32/include
	LIBS += -Ltools/dlfcn-win32/lib
endif

all: pn
	+${MAKE} -s usage

pn: potion libpotion.a lib/readline/readline.so

rebuild: clean potion test

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
	@${ECHO} " Kindly email your respects and thoughtful queries"
	@${ECHO} " to _why <why@whytheluckystiff.net>. Thankyou."

config:
	@${ECHO} "#define POTION_CC     \"${CC}\""
	@${ECHO} "#define POTION_CFLAGS \"${CFLAGS}\""
	@${ECHO} "#define POTION_DEBUG  ${DEBUG}"
	@${ECHO} "#define POTION_JIT    ${JIT}"
	@${ECHO} "#define POTION_MAKE   \"${MAKE}\""
	@${ECHO} "#define POTION_PREFIX \"${PREFIX}\""
	@${ECHO}
	@./tools/config.sh ${CC}

core/version.h:
	@${ECHO} "#define POTION_DATE   \"${DATE}\"" > core/version.h
	@${ECHO} "#define POTION_COMMIT \"${COMMIT}\"" >> core/version.h
	@${ECHO} "#define POTION_REV    ${REVISION}" >> core/version.h
	@${ECHO} >> core/version.h

core/config.h: core/version.h
	@${ECHO} MAKE $@
	@cat core/version.h > core/config.h
	@${MAKE} -s config >> core/config.h

core/callcc.o: core/callcc.c
	@${ECHO} CC $< +frame-pointer
	@${CC} -c -fno-omit-frame-pointer ${INCS} -o $@ $<

%.o: %.c core/config.h
	@${ECHO} CC $<
	@${CC} -c ${CFLAGS} ${INCS} -o $@ $<

%.opic: %.c core/config.h
	@${ECHO} CC -fPIC $<
	@${CC} -c ${CFLAGS} ${INCS} -fPIC -o $@ $<


.c.o: core/config.h
	@${ECHO} CC $<
	@${CC} -c ${CFLAGS} ${INCS} -o $@ $<

%.c: %.g tools/greg
	@${ECHO} GREG $<
	@${GREG} $< > $@

.g.c: tools/greg
	@${ECHO} GREG $<
	@${GREG} $< > $@

tools/greg: tools/greg.c tools/compile.c tools/tree.c
	@${ECHO} CC $@
	@${CC} -O3 -DNDEBUG -o $@ tools/greg.c tools/compile.c tools/tree.c -Itools

potion: ${OBJ_POTION} ${OBJ}
	@${ECHO} LINK potion
	@${CC} ${CFLAGS} ${OBJ_POTION} ${OBJ} ${LIBS} -o potion
	@if [ "${DEBUG}" != "1" ]; then \
		${ECHO} STRIP potion; \
	  ${STRIP} potion; \
	fi

libpotion.a: ${OBJ_POTION} ${OBJ}
	@${ECHO} AR $@
	@if [ -e $@ ]; then rm -f $@; fi
	@${AR} rcs $@ core/*.o > /dev/null

libpotion.so: core/potion.opic ${PIC_OBJ}
	@${ECHO} LD $@ -fpic
	@if [ -e $@ ]; then rm -f $@; fi
	@${CC} -O3 -DNDEBUG -shared -fpic -o $@ core/*.opic > /dev/null

lib/readline/readline.so:
	@${ECHO} MAKE $@
	@cd lib/readline; \
	${MAKE} -s readline.so; \
	cd ../..

bench: potion test/api/gc-bench
	@${ECHO}; \
	${ECHO} running GC benchmark; \
	time test/api/gc-bench

test: potion test/api/potion-test test/api/gc-test
	@${ECHO}; \
	${ECHO} running API tests; \
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
			 jit=`./potion -v | sed "/jit=1/!d"`; \
			 if [ "$$jit" = "" ]; then \
			   ${ECHO} skipping; \
			   break; \
			 fi; \
		fi; \
		for f in test/**/*.pn; do \
			look=`cat $$f | sed "/\#/!d; s/.*\# //"`; \
			if [ $$pass -eq 0 ]; then \
				for=`./potion -I -B $$f | sed "s/\n$$//"`; \
			elif [ $$pass -eq 1 ]; then \
				./potion -c $$f > /dev/null; \
				fb="$$f"b; \
				for=`./potion -I -B $$fb | sed "s/\n$$//"`; \
				rm -rf $$fb; \
			else \
				for=`./potion -I -X $$f | sed "s/\n$$//"`; \
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

test/api/potion-test: ${OBJ_TEST} ${OBJ}
	@${ECHO} LINK potion-test
	@${CC} ${CFLAGS} ${OBJ_TEST} ${OBJ} ${LIBS} -o $@

test/api/gc-test: ${OBJ_GC_TEST} ${OBJ}
	@${ECHO} LINK gc-test
	@${CC} ${CFLAGS} ${OBJ_GC_TEST} ${OBJ} ${LIBS} -o $@

test/api/gc-bench: ${OBJ_GC_BENCH} ${OBJ}
	@${ECHO} LINK gc-bench
	@${CC} ${CFLAGS} ${OBJ_GC_BENCH} ${OBJ} ${LIBS} -o $@

dist: bin-dist src-dist

install: bin-dist
	sudo tar xfz pkg/${PKG}.tar.gz -C $(PREFIX)/

bin-dist: pkg/${PKG}.tar.gz

pkg/${PKG}.tar.gz: core/config.h core/version.h core/syntax.c potion libpotion.a lib/readline/readline.so
	rm -rf dist
	mkdir -p dist dist/bin dist/include/potion dist/lib/readline dist/share/${PKG}/doc \
	  dist/share/${PKG}/example
	cp core/*.h dist/include/potion/
	cp potion dist/bin/
	cp libpotion.a dist/lib/
	cp lib/readline/readline.so dist/lib/readline/
	cp doc/* dist/share/${PKG}/doc/
	cp example/* dist/share/${PKG}/example/
	-mkdir -p pkg
	cd dist && tar czvf pkg/${PKG}.tar.gz dist/* -C dist
	rm -rf dist

src-dist: pkg/${PKG}-src.tar.gz

pkg/${PKG}-src.tar.gz: tarball

tarball: core/version.h core/syntax.c
	-mkdir -p pkg
	rm -rf ${PKG}
	git checkout-index --prefix=${PKG}/ -a
	rm -f ${PKG}/.gitignore
	cp core/version.h ${PKG}/core/
	cp core/syntax.c ${PKG}/core/
	tar czvf pkg/${PKG}-src.tar.gz ${PKG}
	rm -rf ${PKG}

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

sloc: clean
	@cp core/syntax.g core/syntax-g.c
	@sloccount core
	@rm -f core/syntax-g.c

todo:
	@grep -rInso 'TODO: \(.\+\)' core

clean:
	@${ECHO} cleaning
	@rm -f ${OBJ} ${OBJ_POTION} ${OBJ_TEST} ${OBJ_GC_TEST} ${OBJ_GC_BENCH} ${DOCHTML}
	@rm -f tools/greg tools/greg.o tools/compile.o tools/tree.o
	@rm -f core/config.h core/version.h core/syntax.c
	@rm -f potion potion.exe libpotion.a test/api/potion-test test/api/gc-test test/api/gc-bench

.PHONY: all clean doc rebuild test
