SRC = core/compile.c core/file.c core/internal.c core/number.c core/objmodel.c core/primitive.c core/pn-ast.c core/pn-gram.c core/pn-scan.c core/string.c core/table.c core/vm.c
OBJ = ${SRC:.c=.o}
OBJ_POTION = core/potion.o
OBJ_TEST = test/potion-test.o test/CuTest.o

PREFIX = /usr/local
CC = gcc
CFLAGS = -Wall -DICACHE -DMCACHE
DEBUG ?= 0
INCS = -Icore
LEMON = tools/lemon
LIBS =
RAGEL = ragel

DFLAGS = `echo "${DEBUG}" | sed "s/0/-O2/; s/1/-g -DDEBUG/"`
CFLAGS += ${DFLAGS}

DATE = `date +%Y-%m-%d`
REVISION = `git rev-list HEAD | wc -l`
COMMIT = `git rev-list HEAD -1 | head -c 7`

CCEX = ${CC} -x c - && ./a.out && rm -f a.out
ULONG = `echo "\#include <stdio.h>int main() { printf(\\\"%zd\\\", sizeof(unsigned long)); }" | ${CCEX}`
UINT  = `echo "\#include <stdio.h>int main() { printf(\\\"%zd\\\", sizeof(unsigned int )); }" | ${CCEX}`
LLONG = `echo "\#include <stdio.h>int main() { printf(\\\"%zd\\\", sizeof(unsigned long long)); }" | ${CCEX}`

all: potion test

version:
	@echo "#define POTION_DATE   \"${DATE}\""
	@echo "#define POTION_COMMIT \"${COMMIT}\""
	@echo "#define POTION_REV    ${REVISION}"
	@echo
	@echo "#define POTION_CC     \"${CC}\""
	@echo "#define POTION_CFLAGS \"${CFLAGS}\""
	@echo "#define POTION_DEBUG  ${DEBUG}"
	@echo "#define POTION_MAKE   \"${MAKE}\""
	@echo "#define POTION_PREFIX \"${PREFIX}\""
	@echo
	@echo "#define PN_SIZE_T     ${ULONG}"
	@echo "#define ULONG_SIZE_T  ${ULONG}"
	@echo "#define UINT_SIZE_T   ${UINT}"
	@echo "#define ULONGLONG_SIZE_T  ${ULONG}"

core/version.h:
	@${MAKE} -s version > core/version.h

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} ${INCS} -o $@ $<

core/pn-scan.c: core/pn-scan.rl
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
	@${CC} ${CFLAGS} ${OBJ_POTION} ${OBJ} -o potion

test: test/potion-test
	@echo running tests
	@test/potion-test

test/potion-test: core/version.h ${OBJ_TEST} ${OBJ}
	@echo LINK potion-test
	@${CC} ${CFLAGS} ${OBJ_TEST} ${OBJ} -o $@

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
	@rm -f potion potion.exe test/potion-test

.PHONY: all clean test
