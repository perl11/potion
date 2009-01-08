SRC = core/compile.c core/contrib.c core/file.c core/internal.c core/number.c core/objmodel.c core/primitive.c core/pn-ast.c core/pn-gram.c core/pn-scan.c core/string.c core/table.c core/vm.c
OBJ = ${SRC:.c=.o}
OBJ_POTION = core/potion.o
OBJ_TEST = test/api/potion-test.o test/api/CuTest.o

PREFIX = /usr/local
CC = gcc
CFLAGS = -Wall -DICACHE -DMCACHE -fno-strict-aliasing
DEBUG ?= 0
ECHO = /bin/echo
INCS = -Icore
JIT ?= 1
LEMON = tools/lemon
LIBS = -lm
RAGEL = ragel
STRIP ?= `./tools/config.sh ${CC} strip`

DEBUGFLAGS = `${ECHO} "${DEBUG}" | sed "s/0/-O2/; s/1/-g -DDEBUG/"`
CFLAGS += ${DEBUGFLAGS}
JITFLAGS = `${ECHO} "${JIT}" | sed "s/0/-DNO_JIT/; s/1/-DX86_JIT/"`
CFLAGS += ${JITFLAGS}

DATE = `date +%Y-%m-%d`
REVISION = `git rev-list HEAD | wc -l`
COMMIT = `git rev-list HEAD -1 | head -c 7`

RAGELV = `${RAGEL} -v | sed "/ version /!d; s/.* version //; s/ .*//"`

all: potion usage

rebuild: clean potion test

usage:
	@${ECHO}
	@${ECHO} " ~ using potion ~"
	@${ECHO}
	@${ECHO} " Running a script."
	@${ECHO}
	@${ECHO} "   $$ ./potion examples/fib.pn"
	@${ECHO}
	@${ECHO} " Dump the AST and bytecode inspection for a script. "
	@${ECHO}
	@${ECHO} "   $$ ./potion -V examples/fib.pn"
	@${ECHO}
	@${ECHO} " Compiling to bytecode."
	@${ECHO}
	@${ECHO} "   $$ ./potion -c examples/fib.pn"
	@${ECHO} "   $$ ./potion examples/fib.pnb"
	@${ECHO}
	@${ECHO} " Potion builds its JIT compiler by default, but"
	@${ECHO} " you can use the bytecode VM by running scripts"
	@${ECHO} " with the -B flag."
	@${ECHO}
	@${ECHO} " If you built with JIT=0, then the bytecode VM"
	@${ECHO} " will run by default."
	@${ECHO}
	@${ECHO} " To verify your build,"
	@${ECHO}
	@${ECHO} "   $$ make test"
	@${ECHO}
	@${ECHO} " Kindly email your respects and thoughtful queries"
	@${ECHO} " to _why <why@whytheluckystiff.net>. Thankyou."

version:
	@${ECHO} "#define POTION_DATE   \"${DATE}\""
	@${ECHO} "#define POTION_COMMIT \"${COMMIT}\""
	@${ECHO} "#define POTION_REV    ${REVISION}"
	@${ECHO}
	@${ECHO} "#define POTION_CC     \"${CC}\""
	@${ECHO} "#define POTION_CFLAGS \"${CFLAGS}\""
	@${ECHO} "#define POTION_DEBUG  ${DEBUG}"
	@${ECHO} "#define POTION_JIT    ${JIT}"
	@${ECHO} "#define POTION_MAKE   \"${MAKE}\""
	@${ECHO} "#define POTION_RAGEL  \"${RAGELV}\""
	@${ECHO} "#define POTION_PREFIX \"${PREFIX}\""
	@${ECHO}
	@./tools/config.sh ${CC}

core/version.h:
	@${MAKE} -s version > core/version.h

.c.o:
	@${ECHO} CC $<
	@${CC} -c ${CFLAGS} ${INCS} -o $@ $<

core/pn-scan.c: core/pn-scan.rl
	@if [ "${RAGELV}" != "6.3" ]; then \
		if [ "${RAGELV}" != "6.2" ]; then \
			${ECHO} "** potion may not work with ragel ${RAGELV}! try version 6.2 or 6.3."; \
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

potion: core/version.h ${OBJ_POTION} ${OBJ}
	@${ECHO} LINK potion
	@${CC} ${CFLAGS} ${OBJ_POTION} ${OBJ} ${LIBS} -o potion
	@if [ "${DEBUG}" != "1" ]; then \
		${ECHO} STRIP potion; \
	  ${STRIP} potion; \
	fi

test: test/api/potion-test
	@${ECHO}; \
	${ECHO} running API tests; \
	test/api/potion-test; \
	count=0; failed=0; pass=0; \
	while [ $$pass -lt 2 ]; do \
	  if [ $$pass -eq 0 ]; then \
		   ${ECHO} running VM tests; \
		else \
		   ${ECHO}; ${ECHO} running JIT tests; \
			 jit=`./potion -v | sed "/jit=1/!d"`; \
			 if [ "$$jit" = "" ]; then \
			   ${ECHO} skipping; \
			   break; \
			 fi; \
		fi; \
		for f in test/**/*.pn; do \
			look=`cat $$f | sed "/\#/!d; s/.*\# //"`; \
			flags=-B; \
			if [ $$pass -eq 1 ]; then \
			  flags=-X; \
			fi; \
			for=`./potion -I $$flags $$f | sed "s/\n$$//"`; \
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

test/api/potion-test: core/version.h ${OBJ_TEST} ${OBJ}
	@${ECHO} LINK potion-test
	@${CC} ${CFLAGS} ${OBJ_TEST} ${OBJ} ${LIBS} -o $@

sloc: clean
	@cp core/pn-scan.rl core/pn-scan-rl.c
	@sloccount core
	@rm -f core/pn-scan-rl.c

todo:
	@grep -rInso 'TODO: \(.\+\)' core

clean:
	@${ECHO} cleaning
	@rm -f ${OBJ} ${OBJ_POTION} ${OBJ_TEST}
	@rm -f core/version.h core/pn-gram.c core/pn-gram.h core/pn-gram.out core/pn-scan.c
	@rm -f potion potion.exe test/api/potion-test

.PHONY: all clean rebuild test
