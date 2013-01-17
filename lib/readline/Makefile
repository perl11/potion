include ../../config.inc
INCS = -I../../core -L../..
LIBS = -lpotion
CFLAGS += $(INCS) $(LIBS) -fpic -Wl,-flat_namespace

all: readline${LOADEXT}

%.so: %.c
	@$(CC) -shared $(CFLAGS) -o $@ linenoise.c $<

%.dylib: %.c
	@$(CC) -shared $(CFLAGS) -o $@ linenoise.c $<

%.dll: %.c
	@$(CC) -shared $(CFLAGS) -o $@ linenoise.c $<

test:

clean:
	@rm -f ../readline${LOADEXT} readline${LOADEXT}
