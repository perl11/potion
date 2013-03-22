# -*- makefile -*-
# create config.inc and core/config.h
PREFIX = /usr/local
CC ?= gcc
CFLAGS = -Wall -fno-strict-aliasing -Wno-return-type -D_GNU_SOURCE
LDEXEFLAGS = -L. -Wl,-rpath=. -Wl,-rpath=../lib
LDDLLFLAGS = -shared -fpic
INCS = -Icore
LIBS = -lm
AR ?= ar
DEBUG ?= 0
WIN32 = 0
CLANG = 0
JIT = 0
EXE =
APPLE = 0

CAT  = /bin/cat
ECHO = /bin/echo
SED  = sed
EXPR = expr
GREG = tools/greg${EXE}

STRIP ?= `./tools/config.sh "${CC}" strip`
JIT_TARGET ?= `./tools/config.sh "${CC}" jit`
ifneq (${JIT_TARGET},)
  JIT = 1
endif

ifeq (${JIT},1)
#ifeq (${JIT_TARGET},X86)
ifneq (${DEBUG},0)
# http://udis86.sourceforge.net/ x86 16,32,64 bit
# port install udis86
ifeq ($(shell ./tools/config.sh "${CC}" lib -ludis86 udis86.h),1)
	DEFINES += -DHAVE_LIBUDIS86 -DJIT_DEBUG
	LIBS += -ludis86
else
ifeq ($(shell ./tools/config.sh "${CC}" lib -ludis86 udis86.h /opt/local),1)
	DEFINES += -I/opt/local/include -DHAVE_LIBUDIS86 -DJIT_DEBUG
	LIBS += -L/opt/local/lib -ludis86
else
ifeq ($(shell ./tools/config.sh "${CC}" lib -ludis86 udis86.h /usr/local),1)
	DEFINES += -I/usr/local/include -DHAVE_LIBUDIS86 -DJIT_DEBUG
	LIBS += -L/usr/local/lib -ludis86
else
# http://ragestorm.net/distorm/ x86 16,32,64 bit with all intel/amd extensions
# apt-get install libdistorm64-dev
ifeq ($(shell ./tools/config.sh "${CC}" lib -ldistorm64 stdlib.h),1)
	DEFINES += -DHAVE_LIBDISTORM64 -DJIT_DEBUG
	LIBS += -ldistorm64
else
ifeq ($(shell ./tools/config.sh "${CC}" lib -ldistorm64 stdlib.h /usr/local),1)
	DEFINES += -DHAVE_LIBDISTORM64 -DJIT_DEBUG
	LIBS += -L/usr/local/lib -ldistorm64
else
# http://bastard.sourceforge.net/libdisasm.html 386 32bit only
# apt-get install libdisasm-dev
ifeq ($(shell ./tools/config.sh "${CC}" lib -ldisasm libdis.h),1)
	DEFINES += -DHAVE_LIBDISASM -DJIT_DEBUG
	LIBS += -ldisasm
else
ifeq ($(shell ./tools/config.sh "${CC}" lib -ldisasm libdis.h /usr/local),1)
	DEFINES += -I/usr/local/include -DHAVE_LIBDISASM -DJIT_DEBUG
	LIBS += -L/usr/local/lib -ldisasm
endif
endif
endif
endif
endif
endif
endif
endif
endif

ifneq ($(shell ./tools/config.sh "${CC}" clang),0)
	CLANG = 1
	CFLAGS += -Wno-unused-value
endif
ifeq (${DEBUG},0)
	DEBUGFLAGS += -O3 -fno-omit-frame-pointer -fno-stack-protector
else
	DEFINES += -DDEBUG
	STRIP = echo
  ifneq (${CLANG},1)
	DEBUGFLAGS += -g3 -fstack-protector
  else
	DEBUGFLAGS += -g -fno-omit-frame-pointer -fstack-protector
    ifeq (${ASAN},1)
	DEBUGFLAGS += -fsanitize=address
	DEFINES += -D__SANITIZE_ADDRESS__
    endif
  endif
endif

# CFLAGS += \${DEFINES} \${DEBUGFLAGS}

ifneq ($(shell ./tools/config.sh "${CC}" bsd),1)
	LIBS += -ldl
endif
# cygwin is not WIN32
ifeq ($(shell ./tools/config.sh "${CC}" mingw),1)
	WIN32 = 1
	LDDLLFLAGS = -shared
	EXE  = .exe
	LOADEXT = .dll
	DLL  = .dll
	INCS += -Itools/dlfcn-win32/include
	LIBS += -Ltools/dlfcn-win32/lib
	RUNPOTION = potion.exe
else
ifeq ($(shell ./tools/config.sh "${CC}" cygwin),1)
	CYGWIN = 1
	LDDLLFLAGS = -shared
	EXE  = .exe
	LOADEXT = .dll
	DLL  = .dll
	RUNPOTION = ./potion
else
ifeq ($(shell ./tools/config.sh "${CC}" apple),1)
        APPLE    = 1
	DLL      = .dylib
	LOADEXT  = .bundle
	RUNPOTION = ./potion
	LDDLLFLAGS = -dynamiclib -undefined dynamic_lookup -fpic -Wl,-flat_namespace \
	             -install_name "@executable_path/../lib/libpotion${DLL}"
	LDEXEFLAGS = -L.
else
	RUNPOTION = ./potion
	DLL  = .so
	LOADEXT = .so
    ifeq (${CC},gcc)
	CFLAGS += -rdynamic
    endif
endif
endif
endif

# let an existing config.inc overwrite everything
include config.inc

config: config.inc

config.inc.echo:
	@${ECHO} "PREFIX  = ${PREFIX}"
	@${ECHO} "ECHO    = ${ECHO}"
	@${ECHO} "EXE     = ${EXE}"
	@${ECHO} "DLL     = ${DLL}"
	@${ECHO} "LOADEXT = ${LOADEXT}"
	@${ECHO} "CC      = ${CC}"
	@${ECHO} "DEFINES = ${DEFINES}"
	@${ECHO} "DEBUGFLAGS = ${DEBUGFLAGS}"
	@${ECHO} "CFLAGS  = ${CFLAGS} " "\$$"{DEFINES} "\$$"{DEBUGFLAGS}
	@${ECHO} "LDDLLFLAGS = ${LDDLLFLAGS}"
	@${ECHO} "LDEXEFLAGS = ${LDEXEFLAGS}"
	@${ECHO} "LIBS    = ${LIBS}"
	@${ECHO} "STRIP   = ${STRIP}"
	@${ECHO} "APPLE   = ${APPLE}"
	@${ECHO} "WIN32   = ${WIN32}"
	@${ECHO} "CLANG   = ${CLANG}"
	@${ECHO} "JIT     = ${JIT}"
	@test -n ${JIT_TARGET} && ${ECHO} "JIT_${JIT_TARGET} = 1"
	@${ECHO} "DEBUG   = ${DEBUG}"
	@${ECHO} "RUNPOTION = ${RUNPOTION}"
	@${ECHO} "REVISION  = " $(shell git rev-list --abbrev-commit HEAD | wc -l | ${SED} "s/ //g")

config.h.echo:
	@${ECHO} "#define POTION_CC     \"${CC}\""
	@${ECHO} "#define POTION_CFLAGS \"${CFLAGS}\""
	@${ECHO} "#define POTION_JIT    ${JIT}"
	@${ECHO} "#define POTION_MAKE   \"${MAKE}\""
	@${ECHO} "#define POTION_PREFIX \"${PREFIX}\""
	@${ECHO} "#define POTION_WIN32  ${WIN32}"
	@${ECHO} "#define POTION_EXE    \"${EXE}\""
	@${ECHO} "#define POTION_DLL    \"${DLL}\""
	@${ECHO} "#define POTION_LOADEXT \"${LOADEXT}\""
	@test -n ${JIT_TARGET} && ${ECHO} "#define POTION_JIT_TARGET POTION_${JIT_TARGET}"
	@test -n ${JIT_TARGET} && ${ECHO} "#define POTION_JIT_NAME " $(shell echo ${JIT_TARGET} | tr A-Z a-z)
	@${ECHO} ${DEFINES} | ${SED} "s,-D\(\w*\),\n#define \1 1,g; s,-I[a-z/:]* ,,g"
	@${ECHO}
	@./tools/config.sh ${CC}

# bootstrap config.inc via `make -f config.mak`
config.inc: tools/config.sh config.mak
	@${ECHO} MAKE $@
	@${ECHO} "# -*- makefile -*-" > config.inc
	@${ECHO} "# created by ${MAKE} -f config.mak" >> config.inc
	@${MAKE} -s -f config.mak config.inc.echo >> $@
#	@${MAKE} -s -f config.mak core/config.h
#	@${CAT} core/config.h | ${SED} "/POTION_JIT_TARGET /!d;" | \
#	  ${SED} "s,\(.*JIT_TARGET \)POTION_\(.*\),JIT_\2 = 1," >> $@

# Force sync with config.inc
core/config.h: config.inc core/version.h tools/config.sh config.mak
	@${ECHO} MAKE $@
	@${CAT} core/version.h > core/config.h
	@${MAKE} -s -f config.mak config.h.echo >> core/config.h

core/version.h: $(shell git show-ref HEAD | ${SED} "s,^.* ,.git/,g")
	@${ECHO} MAKE $@
	@${ECHO} "/* created by ${MAKE} -f config.mak */" > core/version.h
	@${ECHO} -n "#define POTION_DATE   \"" >> core/version.h
	@${ECHO} -n $(shell date +%Y-%m-%d) >> core/version.h
	@${ECHO} "\"" >> core/version.h
	@${ECHO} -n "#define POTION_COMMIT \"" >> core/version.h
	@${ECHO} -n $(shell git rev-list HEAD -1 --abbrev=7 --abbrev-commit) >> core/version.h
	@${ECHO} "\"" >> core/version.h
	@${ECHO} -n "#define POTION_REV    " >> core/version.h
	@${ECHO} -n $(shell git rev-list --abbrev-commit HEAD | wc -l | ${SED} "s/ //g") >> core/version.h
	@${ECHO} >> core/version.h

.PHONY: config config.inc.echo config.h.echo
