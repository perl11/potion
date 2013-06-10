include ../../config.inc
INCS += -I../../core -I../../3rd/pcre
LIBS += -L.. -lpotion -L../../3rd/pcre -lpcre
SRC   =

all: pcre${LOADEXT}

%${LOADEXT}: %.c
	@if [ -f ../../libpotion.a ]; then mv ../../libpotion.a ../../libpotion.a.tmp; fi
	@$(CC) $(CFLAGS) -o $@ $(INCS) $(LDDLLFLAGS) $(SRC) $< $(LIBS)
	@if [ -f ../../libpotion.a.tmp ]; then mv ../../libpotion.a.tmp ../../libpotion.a; fi

test:

clean:
	@rm -f ../pcre${LOADEXT} pcre${LOADEXT} *.o
