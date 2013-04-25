include ../../config.inc
INCS = -I../../core
LIBS += -L../potion -lpotion

all: readline${LOADEXT}

%${LOADEXT}: %.c
	@$(CC) $(CFLAGS) -o $@ $(LDDLLFLAGS) $(INCS) linenoise.c $< $(LIBS)

test:

clean:
	@rm -f ../readline${LOADEXT} readline${LOADEXT}
