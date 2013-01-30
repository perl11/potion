# -*- makefile-bsdmake -*-
.include "Makefile.common"

pn: potion${EXE} libpotion.a lib/readline${LOADEXT}

config:
	@${ECHO} MAKE -f config.mk $@
	@${MAKE} -s -f config.mk

# Force sync with config.inc
core/config.h: core/version.h tools/config.sh config.mk
	@${ECHO} MAKE -f config.mk $@
	@${MAKE} -s -f config.mk $@

core/version.h: .git/HEAD .git/$(shell git symbolic-ref HEAD)
	@${MAKE} -s -f config.mk $@

# bootstrap config.inc
config.inc: tools/config.sh config.mk
	@${ECHO} MAKE -f config.mk $@
	@${MAKE} -s -f config.mk $@

lib/readline${LOADEXT}: config.inc lib/readline/Makefile lib/readline/linenoise.c \
  lib/readline/linenoise.h
	@${ECHO} MAKE $@ -fpic
	@cd lib/readline && ${MAKE} -s
	@cp lib/readline/readline${LOADEXT} $@

.include "Makefile.targets"

.PHONY: all config clean doc rebuild test bench tarball dist install
