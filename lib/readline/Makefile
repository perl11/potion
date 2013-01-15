CC ?= gcc
LOADEXT ?= .so
INCS = -I../../core -L../..
LIBS = -lpotion
CFLAGS = $(INCS) $(LIBS) -Wall -fno-strict-aliasing -Wno-return-type -fpic -Wl,-flat_namespace

all: readline${LOADEXT}

%.so: %.c
	@$(CC) -shared $(CFLAGS) -o $@ linenoise.c $<

%.dylib: %.c
	@$(CC) -shared $(CFLAGS) -o $@ linenoise.c $<

%.dll: %.c
	@$(CC) -shared $(CFLAGS) -o $@ linenoise.c $<
