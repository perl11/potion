include Makefile.common

pn: potion${EXE} libpotion.a lib/readline${LOADEXT}

config:
	@${ECHO} MAKE -f config.mak $@
	@${MAKE} -s -f config.mak

# Force sync with config.inc
core/config.h: core/version.h tools/config.sh config.mak
	@${ECHO} MAKE -f config.mak $@
	@${MAKE} -s -f config.mak $@

core/version.h: .git/HEAD .git/$(shell git symbolic-ref HEAD)
	@${MAKE} -s -f config.mak $@

# bootstrap config.inc
config.inc: tools/config.sh config.mak
	@${ECHO} MAKE -f config.mak $@
	@${MAKE} -s -f config.mak $@

lib/readline${LOADEXT}: config.inc lib/readline/Makefile lib/readline/linenoise.c \
  lib/readline/linenoise.h
	@${ECHO} MAKE $@ -fpic
	@${MAKE} -s -C lib/readline
	@cp lib/readline/readline${LOADEXT} $@

include Makefile.targets
