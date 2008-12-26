SRC = core/file.c core/internal.c core/number.c core/objmodel.c core/pn-ast.c core/pn-gram.c core/pn-scan.c core/potion.c core/string.c core/table.c
OBJ = ${SRC:.c=.o}

PREFIX = /usr/local
CC = gcc
CFLAGS = -Wall -DICACHE -DMCACHE
ifeq (${DEBUG}, 1)
	CFLAGS += -g -DDEBUG
else
	CFLAGS += -O2
endif

INCS = -Icore
LIBS =

# pre-processors
LEMON = lemon
RAGEL = ragel

all: potion

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} -o $@ $<

core/pn-scan.c: core/pn-scan.rl
	@echo RAGEL $<
	@${RAGEL} $< -C -o $@

core/pn-gram.c: core/pn-gram.y
	@echo LEMON $<
	@${LEMON} $<

potion: ${OBJ}
	${CC} ${CFLAGS} ${OBJ} -o potion

sloc: clean
	@cp core/pn-scan.rl core/pn-scan-rl.c
	@sloccount .
	@rm -f core/pn-scan-rl.c

todo:
	@grep -rInso 'TODO: \(.\+\)' core

clean:
	@echo cleaning
	@rm -f potion ${OBJ} core/pn-gram.c core/pn-gram.h core/pn-gram.out core/pn-scan.c

.PHONY: all clean
