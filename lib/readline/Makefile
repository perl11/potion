.SUFFIXES: .c .i .o .opic .so .dll .bundle

include ../../config.inc

INCS = -I../../core -L../..
LIBS = -lpotion
CFLAGS += $(INCS) $(LIBS) -fpic -Wl,-flat_namespace

all: readline${LOADEXT}

%${LOADEXT}: %.c
	@if [ -f ../../libpotion.a ]; then mv ../../libpotion.a ../../libpotion.a.tmp; fi
	@$(CC) -shared $(CFLAGS) -o $@ linenoise.c $<
	@if [ -f ../../libpotion.a.tmp ]; then mv ../../libpotion.a.tmp ../../libpotion.a; fi
.c${LOADEXT}:
	@if [ -f ../../libpotion.a ]; then mv ../../libpotion.a ../../libpotion.a.tmp; fi
	@$(CC) -shared $(CFLAGS) -o $@ linenoise.c $<
	@if [ -f ../../libpotion.a.tmp ]; then mv ../../libpotion.a.tmp ../../libpotion.a; fi

test:

clean:
	@rm -f ../readline${LOADEXT} readline${LOADEXT}

.PHONY: all test clean
