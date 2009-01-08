SRC = core/compile.c core/contrib.c core/file.c core/internal.c core/number.c core/objmodel.c core/primitive.c core/pn-ast.c core/pn-gram.c core/pn-scan.c core/string.c core/table.c core/vm.c
OBJ = ${SRC:.c=.o}
OBJ_POTION = core/potion.o
OBJ_TEST = test/api/potion-test.o test/api/CuTest.o

PREFIX = /usr/local
CC = gcc
CFLAGS = -Wall -DICACHE -DMCACHE -fno-strict-aliasing
DEBUG ?= 0
INCS = -Icore
JIT ?= 1
LEMON = tools/lemon
LIBS = -lm
RAGEL = ragel
STRIP ?= `./tools/config.sh ${CC} strip`
STRIPD := `echo "${DEBUG}" | sed "s/0/${STRIP}/; s/1/ls/"`

DEBUGFLAGS = `echo "${DEBUG}" | sed "s/0/-O2/; s/1/-g -DDEBUG/"`
CFLAGS += ${DEBUGFLAGS}
JITFLAGS = `echo "${JIT}" | sed "s/0/-DNO_JIT/; s/1/-DX86_JIT/"`
CFLAGS += ${JITFLAGS}

DATE = `date +%Y-%m-%d`
REVISION = `git rev-list HEAD | wc -l`
COMMIT = `git rev-list HEAD -1 | head -c 7`

RAGELV = `${RAGEL} -v | sed "/ version /!d; s/.* version //; s/ .*//"`
MINGW_CMD = ./tools/config.sh ${CC} mingw | sed "s/0//; s/1/core\/mingw.o/"
MINGW_OBJ != `$(MINGW_CMD)`
MINGW_OBJ ?= $(shell $(MINGW_CMD))
OBJ += ${MINGW_OBJ}

all: potion usage

usage:
	@echo
	@echo " ~ using potion ~"
	@echo
	@echo " Running a script."
	@echo
	@echo "   $$ ./potion examples/fib.pn"
	@echo
	@echo " Dump the AST and bytecode inspection for a script. "
	@echo
	@echo "   $$ ./potion -V examples/fib.pn"
	@echo
	@echo " Compiling to bytecode."
	@echo
	@echo "   $$ ./potion -c examples/fib.pn"
	@echo "   $$ ./potion examples/fib.pnb"
	@echo
	@echo " Potion builds its JIT compiler by default, but"
	@echo " you can use the bytecode VM by running scripts"
	@echo " with the -B flag."
	@echo
	@echo " If you built with JIT=0, then the bytecode VM"
	@echo " will run by default."
	@echo
	@echo " To verify your build,"
	@echo
	@echo "   $$ make test"
	@echo
	@echo " Kindly email your respects and thoughtful queries"
	@echo " to _why <why@whytheluckystiff.net>. Thankyou."

version:
	@echo "#define POTION_DATE   \"${DATE}\""
	@echo "#define POTION_COMMIT \"${COMMIT}\""
	@echo "#define POTION_REV    ${REVISION}"
	@echo
	@echo "#define POTION_CC     \"${CC}\""
	@echo "#define POTION_CFLAGS \"${CFLAGS}\""
	@echo "#define POTION_DEBUG  ${DEBUG}"
	@echo "#define POTION_JIT    ${JIT}"
	@echo "#define POTION_MAKE   \"${MAKE}\""
	@echo "#define POTION_RAGEL  \"${RAGELV}\""
	@echo "#define POTION_PREFIX \"${PREFIX}\""
	@echo
	@./tools/config.sh ${CC}

core/version.h:
	@${MAKE} -s version > core/version.h

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} ${INCS} -o $@ $<

core/pn-scan.c: core/pn-scan.rl
	@if [ "${RAGELV}" != "6.3" ]; then \
		if [ "${RAGELV}" != "6.2" ]; then \
			echo "** potion may not work with ragel ${RAGELV}! try version 6.2 or 6.3."; \
		fi; \
	fi
	@echo RAGEL core/pn-scan.rl
	@${RAGEL} core/pn-scan.rl -C -o $@

core/pn-gram.c: tools/lemon core/pn-gram.y
	@echo LEMON core/pn-gram.y
	@${LEMON} core/pn-gram.y

tools/lemon: tools/lemon.c
	@echo CC tools/lemon.c
	@${CC} -o tools/lemon tools/lemon.c

potion: core/version.h ${OBJ_POTION} ${OBJ}
	@echo LINK potion
	@${CC} ${CFLAGS} ${OBJ_POTION} ${OBJ} ${LIBS} -o potion
	@echo STRIP potion
	@${STRIP} potion

test: test/api/potion-test
	@echo; \
	echo running API tests; \
	test/api/potion-test; \
	count=0; failed=0; pass=0; \
	while [ $$pass -lt 2 ]; do \
	  if [ $$pass -eq 0 ]; then \
		   echo running VM tests; \
		else \
		   echo; echo running JIT tests; \
			 jit=`./potion -v | sed "/jit=1/!d"`; \
			 if [ "$$jit" = "" ]; then \
			   echo skipping; \
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
				echo; \
				echo "$$f: expected <$$look>, but got <$$for>"; \
				failed=`expr $$failed + 1`; \
			else \
				echo -n .; \
			fi; \
			count=`expr $$count + 1`; \
		done; \
		pass=`expr $$pass + 1`; \
	done; \
	echo; \
	if [ $$failed -gt 0 ]; then \
		echo "$$failed FAILS ($$count tests)"; \
	else \
		echo "OK ($$count tests)"; \
	fi

test/api/potion-test: core/version.h ${OBJ_TEST} ${OBJ}
	@echo LINK potion-test
	@${CC} ${CFLAGS} ${OBJ_TEST} ${OBJ} ${LIBS} -o $@

sloc: clean
	@cp core/pn-scan.rl core/pn-scan-rl.c
	@sloccount core
	@rm -f core/pn-scan-rl.c

todo:
	@grep -rInso 'TODO: \(.\+\)' core

clean:
	@echo cleaning
	@rm -f ${OBJ} ${OBJ_POTION} ${OBJ_TEST}
	@rm -f core/version.h core/pn-gram.c core/pn-gram.h core/pn-gram.out core/pn-scan.c
	@rm -f potion potion.exe test/api/potion-test

.PHONY: all clean test
