# -*- makefile-bsdmake -*-
# create config.inc and core/config.h
PREFIX = /usr/local
CC     ?= gcc
CFLAGS = -Wall -fno-strict-aliasing -Wno-return-type -D_GNU_SOURCE
LDEXEFLAGS = -L. -Wl,-rpath=. -Wl,-rpath=../lib
LDDLLFLAGS = -shared -fpic
INCS  = -Icore
LIBS  = -lm
AR    ?= ar
JIT   ?= 1
DEBUG ?= 0
EXE   =

CAT  = /bin/cat
ECHO = /bin/echo
SED  = sed
EXPR = expr
GREG = tools/greg${EXE}

STRIP != ./tools/config.sh ${CC} strip

# http://bastard.sourceforge.net/libdisasm.html
# apt-get install libdisasm-dev
LIBDIS != ./tools/config.sh ${CC} lib -ldisasm libdis.h
.if ${LIBDIS} == 1
DEFINES += -DHAVE_LIBDISASM
LIBS += -ldisasm
.endif
CLANG != ./tools/config.sh ${CC} clang
.if ${CLANG} == 1
CFLAGS += -Wno-unused-value
.endif
.if ${DEBUG} == 0
DEBUGFLAGS += -O2 -fno-stack-protector
.else
DEFINES += -DDEBUG
.if ${CLANG} != 1
DEBUGFLAGS += -g3 -fstack-protector
.else
DEBUGFLAGS += -g -fstack-protector
.endif
.endif

CFLAGS += ${DEFINES} ${DEBUGFLAGS}

# cygwin is not WIN32
WIN32 != ./tools/config.sh ${CC} mingw
.if ${WIN32} == 1
LDDLLFLAGS = -shared
EXE  = .exe
LOADEXT = .dll
DLL  = .dll
INCS += -Itools/dlfcn-win32/include
LIBS += -Ltools/dlfcn-win32/lib
RUNPOTION = potion.exe
.else
CYGWIN != ./tools/config.sh ${CC} cygwin
.if ${CYGWIN} == 1
CYGWIN = 1
LDDLLFLAGS = -shared
EXE  = .exe
LOADEXT = .dll
DLL  = .dll
RUNPOTION = ./potion
.else
APPLE != ./tools/config.sh ${CC} apple
.if ${APPLE} == 1
DLL      = .dylib
LOADEXT  = .bundle
RUNPOTION = ./potion
# in builddir: mkdir ../lib; ln -s `pwd`/libpotion.dylib ../lib/
LDDLLFLAGS = -shared -fpic -install_name "@executable_path/../lib/libpotion${DLL}"
LDEXEFLAGS = -L.
.else
RUNPOTION = ./potion
DLL  = .so
LOADEXT = .so
.if ${CC} == gcc
CFLAGS += -rdynamic
.endif
.endif
.endif
.endif

config: config.inc core/config.h

config.inc.echo:
	@${ECHO} "PREFIX  = ${PREFIX}"
	@${ECHO} "EXE  = ${EXE}"
	@${ECHO} "DLL  = ${DLL}"
	@${ECHO} "LOADEXT = ${LOADEXT}"
	@${ECHO} "ECHO    = ${ECHO}"
	@${ECHO} "CC      = ${CC}"
	@${ECHO} "CFLAGS  = ${CFLAGS}"
	@${ECHO} "LDDLLFLAGS = ${LDDLLFLAGS}"
	@${ECHO} "LDEXEFLAGS = ${LDEXEFLAGS}"
	@${ECHO} "JIT     = ${JIT}"
	@${ECHO} "LIBS    = ${LIBS}"
	@${ECHO} "DEFINES = ${DEFINES}"
	@${ECHO} "DEBUGFLAGS = ${DEBUGFLAGS}"
	@${ECHO} "STRIP   = ${STRIP}"
	@${ECHO} "APPLE   = ${APPLE}"
	@${ECHO} "WIN32   = ${WIN32}"
	@${ECHO} "CLANG   = ${CLANG}"
	@${ECHO} "DEBUG   = ${DEBUG}"
	@${ECHO} "RUNPOTION = ${RUNPOTION}"
	@${ECHO} -n "REVISION = "
	@${ECHO} $(shell git rev-list --abbrev-commit HEAD | wc -l | ${SED} "s/ //g")

config.h.echo:
	@${ECHO} "#define POTION_CC     \"${CC}\""
	@${ECHO} "#define POTION_CFLAGS \"${CFLAGS}\""
	@${ECHO} "#define POTION_JIT    ${JIT}"
	@${ECHO} "#define POTION_MAKE   \"${MAKE}\""
	@${ECHO} "#define POTION_PREFIX \"${PREFIX}\""
	@${ECHO} ${DEFINES} | ${SED} "s,-D\(\w*\),\n#define \1 1,g"
	@${ECHO}
	@./tools/config.sh ${CC}

# bootstrap config.inc via `make -f config.mk`
config.inc: tools/config.sh config.mk
	@${ECHO} MAKE $@
	@${ECHO} "# -*- makefile -*-" > config.inc
	@${ECHO} "# created by ${MAKE} -f config.mk" >> config.inc
	@${MAKE} -s -f config.mk config.inc.echo >> $@
	@${MAKE} -s -f config.mk -B core/config.h

# Force sync with config.inc
core/config.h: core/version.h tools/config.sh config.mk
	@${ECHO} MAKE $@
	@${CAT} core/version.h > core/config.h
	@${MAKE} -s -f config.mk config.h.echo >> core/config.h

core/version.h: .git/HEAD .git/$(shell git symbolic-ref HEAD)
	@${ECHO} MAKE $@
	@${ECHO} "/* created by ${MAKE} -f config.mk */" > core/version.h
	@${ECHO} -n "#define POTION_DATE   \"" >> core/version.h
	@${ECHO} -n `date +%Y-%m-%d` >> core/version.h
	@${ECHO} "\"" >> core/version.h
	@${ECHO} -n "#define POTION_COMMIT \"" >> core/version.h
	@${ECHO} -n `git rev-list HEAD -1 --abbrev=7 --abbrev-commit` >> core/version.h
	@${ECHO} "\"" >> core/version.h
	@${ECHO} -n "#define POTION_REV    " >> core/version.h
	@${ECHO} -n `git rev-list --abbrev-commit HEAD | wc -l | ${SED} "s/ //g"` >> core/version.h
	@${ECHO} >> core/version.h

.PHONY: config config.inc.echo config.h.echo
