include ../../config.inc
INCS += -I../../core
LIBS = -L../.. -lpotion

all: readline${LOADEXT}

%${LOADEXT}: %.c
	@if [ -f ../../libpotion.a ]; then mv ../../libpotion.a ../../libpotion.a.tmp; fi
	@$(CC) $(CFLAGS) -o $@ $(INCS) $(LDDLLFLAGS) linenoise.c $< $(LIBS)
	@if [ -f ../../libpotion.a.tmp ]; then mv ../../libpotion.a.tmp ../../libpotion.a; fi

test:

clean:
	@rm -f ../readline${LOADEXT} readline${LOADEXT}
