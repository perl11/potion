CC ?= gcc
INCS = -I../../core -L../..
LIBS = -lreadline -lpotion
CFLAGS = $(INCS) $(LIBS) -Wall -fno-strict-aliasing -Wno-return-type -fpic -Wl,-flat_namespace

all: readline.so

readline.so:
	@$(CC) -shared $(CFLAGS) -o $@ readline.c