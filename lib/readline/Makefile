CC ?= gcc
INCS = -I../../core -L../..
LIBS = -lpotion
CFLAGS = $(INCS) $(LIBS) -Wall -fno-strict-aliasing -Wno-return-type -fpic -Wl,-flat_namespace

all: readline.so

%.so: %.c
	@$(CC) -shared $(CFLAGS) -o $@ linenoise.c $<
