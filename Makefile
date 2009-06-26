SRC = core/asm.c core/callcc.c core/compile.c core/contrib.c core/file.c core/gc.c core/internal.c core/lick.c core/mt19937ar.c core/number.c core/objmodel.c core/primitive.c core/pn-ast.c core/pn-gram.c core/pn-scan.c core/string.c core/table.c core/vm.c core/vm-ppc.c core/vm-x86.c
OBJ = ${SRC:.c=.o}
OBJ_POTION = core/potion.o
OBJ_TEST = test/api/potion-test.o test/api/CuTest.o
OBJ_GC_TEST = test/api/gc-test.o test/api/CuTest.o
OBJ_GC_BENCH = test/api/gc-bench.o
DOC = doc/start.textile
DOCHTML = ${DOC:.textile=.html}

PREFIX = /usr/local
CC = gcc
CFLAGS = -Wall -fno-strict-aliasing -Wno-return-type
DEBUG ?= 0
ECHO = /bin/echo
INCS = -Icore
JIT ?= 1
LEMON = tools/lemon
LIBS = -lm
RAGEL = ragel
STRIP ?= `./tools/config.sh ${CC} strip`

# TODO: -O2 doesn't include -fno-stack-protector
DEBUGFLAGS = `${ECHO} "${DEBUG}" | sed "s/0/-O2 -DICACHE -DMCACHE/; s/1/-g -DDEBUG/"`
CFLAGS += ${DEBUGFLAGS}

VERSION = `./tools/config.sh ${CC} version`
DATE = `date +%Y-%m-%d`
REVISION = `git rev-list HEAD | wc -l | sed "s/ //g"`
COMMIT = `git rev-list HEAD -1 --abbrev=7 --abbrev-commit`

RAGELV = `${RAGEL} -v | sed "/ version /!d; s/.* version //; s/ .*//"`

RELEASE ?= ${VERSION}.${REVISION}
PKG := "potion-${RELEASE}"

all: potion
	+${MAKE} -s usage

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
	@${ECHO} "#define POTION_RAGEL  \"${RAGELV}\"" >> core/version.h
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

.c.o: core/config.h
	@${ECHO} CC $<
	@${CC} -c ${CFLAGS} ${INCS} -o $@ $<

core/pn-scan.c: core/pn-scan.rl
	@if [ "${RAGELV}" != "6.5" ]; then \
		if [ "${RAGELV}" != "6.4" ]; then \
			if [ "${RAGELV}" != "6.3" ]; then \
				${ECHO} "** potion may not work with ragel ${RAGELV}! try version 6.3 or up."; \
			fi; \
		fi; \
	fi
	@${ECHO} RAGEL core/pn-scan.rl
	@${RAGEL} core/pn-scan.rl -C -o $@

core/pn-gram.c: tools/lemon core/pn-gram.y
	@${ECHO} LEMON core/pn-gram.y
	@${LEMON} core/pn-gram.y

tools/lemon: tools/lemon.c
	@${ECHO} CC tools/lemon.c
	@${CC} -o tools/lemon tools/lemon.c

potion: ${OBJ_POTION} ${OBJ}
	@${ECHO} LINK potion
	@${CC} ${CFLAGS} ${OBJ_POTION} ${OBJ} ${LIBS} -o potion
	@if [ "${DEBUG}" != "1" ]; then \
		${ECHO} STRIP potion; \
	  ${STRIP} potion; \
	fi

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

tarball: core/version.h core/pn-scan.c core/pn-gram.c
	mkdir -p pkg
	rm -rf ${PKG}
	git checkout-index --prefix=${PKG}/ -a
	rm -f ${PKG}/.gitignore
	cp core/version.h ${PKG}/core/
	cp core/pn-scan.c ${PKG}/core/
	cp core/pn-gram.c ${PKG}/core/
	tar czvf pkg/${PKG}.tar.gz ${PKG}
	rm -rf ${PKG}

%.html: %.textile
	@${ECHO} DOC $<
	@${ECHO} "<html><head><style type=\"text/css\">@import 'doc.css';</style>" > $@
	@${ECHO} "<div id='potion'><div id='subbox1'><div id='subbox2'><div id='subbox3'>" >> $@
	@${ECHO} "<img src='potion-1.png' /></div></div></div></div>" >> $@
	@${ECHO} "</head><body><div id='central'>" >> $@
	@redcloth $< >> $@
	@${ECHO} "</div></body></html>" >> $@

doc: ${DOCHTML}

sloc: clean
	@cp core/pn-scan.rl core/pn-scan-rl.c
	@sloccount core
	@rm -f core/pn-scan-rl.c

todo:
	@grep -rInso 'TODO: \(.\+\)' core

clean:
	@${ECHO} cleaning
	@rm -f ${OBJ} ${OBJ_POTION} ${OBJ_TEST} ${OBJ_GC_TEST} ${OBJ_GC_BENCH} ${DOCHTML} tools/lemon
	@rm -f core/config.h core/version.h core/pn-gram.c core/pn-gram.h core/pn-gram.out core/pn-scan.c
	@rm -f potion potion.exe test/api/potion-test test/api/gc-test test/api/gc-bench

.PHONY: all clean doc rebuild test
