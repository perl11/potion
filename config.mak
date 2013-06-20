# -*- makefile -*-
# create config.inc and core/config.h
PREFIX = /usr/local
CC    ?= gcc
CFLAGS = -Wall -Werror -fno-strict-aliasing -Wno-switch -Wno-return-type -Wno-unused-label -D_GNU_SOURCE
INCS   = -Icore
LIBPTH = -Llib
RPATH         = -Wl,-rpath=$(shell pwd)/lib
RPATH_INSTALL = -Wl,-rpath=\$${PREFIX}/lib
LIBS   = -lm
RDLLFLAGS  =
LDDLLFLAGS = -shared -fpic
AR    ?= ar
DEBUG ?= 0
WIN32  = 0
CLANG  = 0
JIT    = 0
EXE    =
APPLE  = 0
CYGWIN = 0
RUNPRE = ./

CAT  = /bin/cat
ECHO = /bin/echo
RANLIB = ranlib
SED  = sed
EXPR = expr

STRIP ?= `tools/config.sh "${CC}" strip`
JIT_TARGET ?= `tools/config.sh "${CC}" jit`
ifneq (${JIT_TARGET},)
  JIT = 1
endif

ifeq (${JIT},1)
#ifeq (${JIT_TARGET},X86)
ifneq (${DEBUG},0)
# http://udis86.sourceforge.net/ x86 16,32,64 bit
# port install udis86
ifeq ($(shell tools/config.sh "${CC}" lib -ludis86 udis86.h),1)
	DEFINES += -DHAVE_LIBUDIS86 -DJIT_DEBUG
	LIBS += -ludis86
else
ifeq ($(shell tools/config.sh "${CC}" lib -ludis86 udis86.h /opt/local),1)
	DEFINES += -DHAVE_LIBUDIS86 -DJIT_DEBUG
	INCS += -I/opt/local/include
	LIBS += -L/opt/local/lib -ludis86
else
ifeq ($(shell tools/config.sh "${CC}" lib -ludis86 udis86.h /usr/local),1)
	DEFINES += -DHAVE_LIBUDIS86 -DJIT_DEBUG
	INCS += -I/usr/local/include
	LIBS += -L/usr/local/lib -ludis86
else
# http://ragestorm.net/distorm/ x86 16,32,64 bit with all intel/amd extensions
# apt-get install libdistorm64-dev
ifeq ($(shell tools/config.sh "${CC}" lib -ldistorm64 stdlib.h),1)
	DEFINES += -DHAVE_LIBDISTORM64 -DJIT_DEBUG
	LIBS += -ldistorm64
else
ifeq ($(shell tools/config.sh "${CC}" lib -ldistorm64 stdlib.h /usr/local),1)
	DEFINES += -DHAVE_LIBDISTORM64 -DJIT_DEBUG
	LIBS += -L/usr/local/lib -ldistorm64
else
# http://bastard.sourceforge.net/libdisasm.html 386 32bit only
# apt-get install libdisasm-dev
ifeq ($(shell tools/config.sh "${CC}" lib -ldisasm libdis.h),1)
	DEFINES += -DHAVE_LIBDISASM -DJIT_DEBUG
	LIBS += -ldisasm
else
ifeq ($(shell tools/config.sh "${CC}" lib -ldisasm libdis.h /usr/local),1)
	DEFINES += -DHAVE_LIBDISASM -DJIT_DEBUG
	INCS += -I/usr/local/include
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

ifeq ($(shell tools/config.sh "${CC}" lib -luv uv.h /usr/local),1)
	HAVE_LIBUV = 1
	DEFINES += -DHAVE_LIBUV
	INCS += -I/usr/local/include
	LIBS += -L/usr/local/lib
else
	HAVE_LIBUV = 0
	INCS += -I3rd/libuv/include
endif

ifeq ($(shell tools/config.sh "${CC}" lib -lpcre pcre.h /usr/local),1)
	HAVE_PCRE = 1
	DEFINES += -DHAVE_PCRE
	INCS += -I/usr/local/include
	LIBS += -L/usr/local/lib
else
ifeq ($(shell tools/config.sh "${CC}" lib -lpcre pcre.h /usr),1)
	HAVE_PCRE = 1
	DEFINES += -DHAVE_PCRE
else
	HAVE_PCRE = 0
endif
endif

# JIT with -O still fails some tests
ifneq (${JIT},1)
       DEBUGFLAGS += -O3
endif
ifneq ($(shell ./tools/config.sh "${CC}" clang),0)
	CLANG = 1
	CFLAGS += -Wno-unused-value
endif
ifeq (${DEBUG},0)
	DEBUGFLAGS += -fno-stack-protector
else
	DEFINES += -DDEBUG
	STRIP = echo
  ifneq (${CLANG},1)
	DEBUGFLAGS += -g3 -fstack-protector
  else
	DEBUGFLAGS += -g -fstack-protector
  endif
endif
ifeq (${ASAN},1)
	DEBUGFLAGS += -fsanitize=address -fno-omit-frame-pointer
	DEFINES += -D__SANITIZE_ADDRESS__
endif

# CFLAGS += \${DEFINES} \${DEBUGFLAGS}

ifneq ($(shell tools/config.sh "${CC}" bsd),1)
	LIBS += -ldl
endif
CROSS = $(shell tools/config.sh "${CC}" cross)
# cygwin is not WIN32. detect mingw target on cross
ifeq ($(shell tools/config.sh "${CC}" mingw),1)
	WIN32 = 1
	LDDLLFLAGS = -shared
	EXE  = .exe
	DLL  = .dll
	LOADEXT = .dll
	INCS += -Itools/dlfcn-win32/include
	LIBS += -Ltools/dlfcn-win32/lib
	RPATH =
	RPATH_INSTALL =
    ifneq (${CROSS},1)
	ECHO = echo
	CAT = type
	RUNPRE =
    else
        RANLIB = $(shell echo "${CC}" | sed -e "s,-gcc,-ranlib,")
    endif
else
ifeq ($(shell tools/config.sh "${CC}" cygwin),1)
	CYGWIN = 1
	LDDLLFLAGS = -shared
	LOADEXT = .dll
	EXE  = .exe
	DLL  = .dll
else
ifeq ($(shell tools/config.sh "${CC}" apple),1)
        APPLE    = 1
	DLL      = .dylib
	LOADEXT  = .bundle
	LDDLLFLAGS = -dynamiclib -undefined dynamic_lookup -fpic -Wl,-flat_namespace
	RDLLFLAGS  = -install_name "@executable_path/../lib/libpotion${DLL}"
	RPATH =
	RPATH_INSTALL =
else
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
	@${ECHO} "INCS    = ${INCS}"
	@${ECHO} "LIBPTH  = ${LIBPTH}"
	@${ECHO} "RPATH   = ${RPATH}"
	@${ECHO} "RPATH_INSTALL = " ${RPATH_INSTALL}
	@${ECHO} "LIBS    = ${LIBS}"
	@${ECHO} "LDDLLFLAGS = ${LDDLLFLAGS}"
	@${ECHO} "RDLLFLAGS  = ${RDLLFLAGS}"
	@${ECHO} "HAVE_LIBUV = ${HAVE_LIBUV}"
	@${ECHO} "HAVE_PCRE  = ${HAVE_PCRE}"
	@${ECHO} "STRIP   = ${STRIP}"
	@${ECHO} "RUNPRE  = ${RUNPRE}"
	@${ECHO} "CROSS   = ${CROSS}"
	@${ECHO} "APPLE   = ${APPLE}"
	@${ECHO} "WIN32   = ${WIN32}"
	@${ECHO} "CYGWIN  = ${CYGWIN}"
	@${ECHO} "CLANG   = ${CLANG}"
	@${ECHO} "JIT     = ${JIT}"
	@test -n ${JIT_TARGET} && ${ECHO} "JIT_${JIT_TARGET} = 1"
	@${ECHO} "DEBUG   = ${DEBUG}"
	@${ECHO} "REVISION  = " $(shell git rev-list --abbrev-commit HEAD | wc -l | ${SED} "s/ //g")

config.h.echo:
	@${ECHO} "#define POTION_CC     \"${CC}\""
	@${ECHO} "#define POTION_CFLAGS \"${CFLAGS}\""
	@${ECHO} "#define POTION_MAKE   \"${MAKE}\""
	@${ECHO} "#define POTION_PREFIX \"${PREFIX}\""
	@${ECHO} "#define POTION_EXE    \"${EXE}\""
	@${ECHO} "#define POTION_DLL    \"${DLL}\""
	@${ECHO} "#define POTION_LOADEXT \"${LOADEXT}\""
	@${ECHO} "#define POTION_WIN32  ${WIN32}"
	@${ECHO} "#define POTION_JIT    ${JIT}"
	@test -n ${JIT_TARGET} && ${ECHO} "#define POTION_JIT_TARGET POTION_${JIT_TARGET}"
	@test -n ${JIT_TARGET} && ${ECHO} "#define POTION_JIT_NAME " $(shell echo ${JIT_TARGET} | tr A-Z a-z)
	@${ECHO} ${DEFINES} | perl -lpe's/-D(\w+)/\n#define \1 1/g; s/=/ /g; s{-I[a-z/:]* }{}g;'
	@${ECHO}
	@tools/config.sh "${CC}"

# bootstrap config.inc via `make -f config.mak`
config.inc: tools/config.sh config.mak
	@${ECHO} MAKE $@
	@${ECHO} "# -*- makefile -*-" > config.inc
	@${ECHO} "# created by ${MAKE} -f config.mak" >> config.inc
	@${MAKE} -s -f config.mak config.inc.echo >> $@

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
