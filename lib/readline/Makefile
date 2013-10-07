include ../../config.inc
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
	@$(CC) $(CFLAGS) -o $@ $(INCS) $(LDDLLFLAGS) $(SRC) $< ${LIBPTH} -lpotion $(LIBS)

test:

clean:
	@rm -f ../potion/readline${LOADEXT} readline${LOADEXT} *.o
