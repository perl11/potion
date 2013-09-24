include ../../config.inc
INCS += -I../../core
LIBS += -L.. -lpotion
SRC   = linenoise.c
ifeq ($(WIN32),1)
  SRC  += win32fixes.c
  LIBS += -lws2_32
endif

all: readline${LOADEXT}

static: readline.o linenoise.o

%.o: %.c
	@$(CC) -c $(CFLAGS) -o $@ $(INCS) $<

%${LOADEXT}: %.c
	@if [ -f ../../libpotion.a ]; then mv ../../libpotion.a ../../libpotion.a.tmp; fi
	@$(CC) $(CFLAGS) -o $@ $(INCS) $(LDDLLFLAGS) $(SRC) $< $(LIBS)
	@if [ -f ../../libpotion.a.tmp ]; then mv ../../libpotion.a.tmp ../../libpotion.a; fi

test:

clean:
	@rm -f ../readline${LOADEXT} readline${LOADEXT} readline.o linenoise.o
