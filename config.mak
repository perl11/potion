# -*- makefile -*-
# create config.inc and core/config.h
POTION_MAJOR = 0
POTION_MINOR = 3
PREFIX = /usr/local
CC     = $(shell tools/config.sh compiler)
PWD    = $(shell pwd)
# -pedantic not yet
WARNINGS = -Wall -Wno-variadic-macros -Wno-pointer-arith -Wno-return-type
CFLAGS = -D_GNU_SOURCE -fno-strict-aliasing
INCS   = -I${PWD}/core
LIBPTH = -L${PWD}/lib
RPATH         = -Wl,-rpath=${PWD}/lib
RPATH_INSTALL = -Wl,-rpath=\$${PREFIX}/lib
LIBS   = -lm
LDFLAGS ?=
LDDLLFLAGS = -shared -fpic
DEBUG ?= 0
SANDBOX ?= 0
WIN32  = 0
CLANG  = 0
JIT    = 0
ICC    = 0
GCC    = 0
EXE    =
APPLE  = 0
CYGWIN = 0

CAT  = /bin/cat
ECHO = /bin/echo
RANLIB ?= ranlib
AR    ?= ar
SED  = sed
EXPR = expr

STRIP ?= $(shell tools/config.sh "${CC}" strip)
JIT_TARGET ?= $(shell tools/config.sh "${CC}" jit)
ifneq (${JIT_TARGET},)
  JIT = 1
endif

ifeq (${JIT},1)
ifeq (${JIT_TARGET},X86)
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
endif

ifeq ($(shell tools/config.sh "${CC}" lib -luv uv.h /usr/local),1)
	HAVE_LIBUV = 1
	DEFINES += -DHAVE_LIBUV
	INCS += -I/usr/local/include
	LIBS += -L/usr/local/lib
else
	HAVE_LIBUV = 0
	INCS += -I${PWD}/3rd/libuv/include
endif

#yet disabled
ifeq (0,1)
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
endif

ifeq (${DEBUG},0)
       DEBUGFLAGS += -O3 -DNDEBUG
       CFLAGS += -D_FORTIFY_SOURCE=2
endif
ifneq (${SANDBOX},0)
	DEFINES += -DSANDBOX
endif

ifneq (,$(findstring ccache,${CC}))
	WARNINGS = -Wall -Wno-variadic-macros -Wno-pointer-arith -Wno-return-type
	CFLAGS += -Qunused-arguments
endif

ifneq ($(shell tools/config.sh "${CC}" clang),0)
	CLANG = 1
	WARNINGS += -Wno-unused-value -Wno-switch -Wno-unused-label
  #todo: 64bit => -fPIE and -fpie for the linker
  ifeq (${DEBUG},0)
        DEFINES += -DCGOTO
	DEBUGFLAGS += -finline
  endif
else
ifneq ($(shell ./tools/config.sh "${CC}" icc),0)
	ICC = 1
        #DEFINES += -DCGOTO
	DEBUGFLAGS += -falign-functions=16
# 177: label "l414" was declared but never referenced in syntax.c sets fail case
# 186: pointless comparison of unsigned integer with zero in PN_TYPECHECK
# 188: enumerated type mixed with another type (treating P->flags as int)
	WARNINGS += -Wno-sign-compare -Wno-pointer-arith -diag-remark 177,186,188
  ifeq (${DEBUG},0)
                    # -Ofast
	DEBUGFLAGS += -finline
  else
        DEBUGFLAGS += -g3 -gdwarf-3
  endif
else
ifneq ($(shell ./tools/config.sh "${CC}" gcc),0)
	GCC = 1
	WARNINGS += -Wno-switch -Wno-unused-label
	DEBUGFLAGS += --param ssp-buffer-size=1
  ifeq (${DEBUG},0)
	DEBUGFLAGS += -finline -falign-functions
        DEFINES += -DCGOTO
  endif
endif
endif
endif

ifeq (${DEBUG},0)
	DEBUGFLAGS += -fno-stack-protector
else
	DEFINES += -DDEBUG
	STRIP = echo
  ifneq (${CLANG},1)
	DEBUGFLAGS += -g3 -fno-stack-protector
  else
	DEBUGFLAGS += -g -fno-stack-protector
  endif
endif
ifeq (${ASAN},1)
	DEBUGFLAGS += -fsanitize=address -fno-omit-frame-pointer
	DEFINES += -D__SANITIZE_ADDRESS__
endif

# CFLAGS += \${DEFINES} \${DEBUGFLAGS}

CROSS = $(shell tools/config.sh "${CC}" cross)
# cygwin is not WIN32. detect mingw target on cross
# maybe parse versions from core/potion.h
ifeq ($(shell tools/config.sh "${CC}" mingw),1)
	WIN32 = 1
	LDFLAGS += -Wl,--major-image-version,${POTION_MAJOR},--minor-image-version,${POTION_MINOR}
	LDDLLFLAGS = -shared
	EXE  = .exe
	DLL  = .dll
	LOADEXT = .dll
	INCS += -I${PWD}/tools/dlfcn-win32/include
	LIBPTH += -L${PWD}/tools/dlfcn-win32/lib
	LIBS += -Llib -luv
    ifneq (,$(findstring i386-mingw32-gcc,${CC}))
	LIBS += -lws2_32 -lpsapi
    else
	LIBS += -lpthread -lws2_32 -lpsapi
    endif
	RPATH =
	RPATH_INSTALL =
    ifneq (${CROSS},1)
# mingw32 shell, not cmd.exe
	ECHO = echo
	CAT = cat
    else
# mingw32 cross needs native echo -n
	ECHO = /bin/echo
	CAT = /bin/cat
	W := $(WARNINGS)
	override WARNINGS = $(subst -Werror,,$(W))
	W := $(WARNINGS)
	override WARNINGS = $(subst -Wno-variadic-macros,,$(W))
	D := $(DEBUGFLAGS)
	override DEBUGFLAGS = $(subst --param ssp-buffer-size=1,,$(D))
	D := $(DEBUGFLAGS)
	override DEBUGFLAGS = $(subst -fno-stack-protector,,$(D))
        RANLIB = $(shell echo "${CC}" | sed -e "s,-gcc,-ranlib,")
    endif
else
ifeq ($(shell tools/config.sh "${CC}" cygwin),1)
	CYGWIN = 1
	LDFLAGS += -Wl,--major-image-version,${MAJOR_IMAGE_VERSION},--minor-image-version,${MINOR_IMAGE_VERSION}
	LDDLLFLAGS = -shared
	LIBS += -Llib -luv
	LOADEXT = .dll
	EXE  = .exe
	DLL  = .dll
else
ifeq ($(shell tools/config.sh "${CC}" apple),1)
        APPLE    = 1
	DLL      = .dylib
	LOADEXT  = .bundle
	LDDLLFLAGS = -dynamiclib -undefined dynamic_lookup -fpic -Wl,-flat_namespace
	LDDLLFLAGS += -install_name "@executable_path/../lib/libpotion${DLL}"
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
ifneq ($(shell tools/config.sh "${CC}" bsd),1)
	LIBS += -ldl
endif

ifneq ($(APPLE),1)
ifneq ($(CROSS),1)
ifneq ($(WIN32),1)
  # only in very new compilers
  #ifneq ($(ICC),1)
  #	WARNINGS += -Wno-zero-length-array -Wno-gnu
  #endif
  ifneq ($(CYGWIN),1)
	LDFLAGS += -Wl,--as-needed -Wl,-z,relro -Wl,-z,now
	LDDLLFLAGS += $(LDFLAGS)
  endif
endif
endif
endif
ifeq (1,$(CROSS))
	RANLIB = $(subst -gcc,-ranlib,${CC})
	AR = $(subst -gcc,-ar,${CC})
endif

# let an existing config.inc overwrite everything
include config.inc

config: config.inc core/config.h

config.inc.echo:
	@${ECHO} "PREFIX  = ${PREFIX}"
	@${ECHO} "ECHO    = ${ECHO}"
	@${ECHO} "EXE     = ${EXE}"
	@${ECHO} "DLL     = ${DLL}"
	@${ECHO} "LOADEXT = ${LOADEXT}"
	@${ECHO} "CC      = ${CC}"
	@${ECHO} "DEFINES = ${DEFINES}"
	@${ECHO} "DEBUGFLAGS = ${DEBUGFLAGS}"
	@${ECHO} "WARNINGS   = ${WARNINGS}"
	@${ECHO} "CFLAGS  = ${CFLAGS} " "\$$"{DEFINES} "\$$"{DEBUGFLAGS} "\$$"{WARNINGS}
	@${ECHO} "INCS    = ${INCS}"
	@${ECHO} "LIBPTH  = ${LIBPTH}"
	@${ECHO} "RPATH   = ${RPATH}"
	@${ECHO} "RPATH_INSTALL = " ${RPATH_INSTALL}
	@${ECHO} "LIBS    = ${LIBS}"
	@${ECHO} "LDFLAGS = ${LDFLAGS}"
	@${ECHO} "LDDLLFLAGS = ${LDDLLFLAGS}"
	@${ECHO} "HAVE_LIBUV = ${HAVE_LIBUV}"
	@${ECHO} "HAVE_PCRE  = ${HAVE_PCRE}"
	@${ECHO} "STRIP   = ${STRIP}"
	@${ECHO} "AR      = ${AR}"
	@${ECHO} "RANLIB  = ${RANLIB}"
	@${ECHO} "CROSS   = ${CROSS}"
	@${ECHO} "APPLE   = ${APPLE}"
	@${ECHO} "WIN32   = ${WIN32}"
	@${ECHO} "CYGWIN  = ${CYGWIN}"
	@${ECHO} "CLANG   = ${CLANG}"
	@${ECHO} "ICC     = ${ICC}"
	@${ECHO} "GCC     = ${GCC}"
	@${ECHO} "SANDBOX = ${SANDBOX}"
	@${ECHO} "JIT     = ${JIT}"
	@test -n ${JIT_TARGET} && ${ECHO} "JIT_${JIT_TARGET} = 1"
	@${ECHO} "DEBUG   = ${DEBUG}"
#TODO get rid of git here, read from POTION_REV in core/version.h
	@${ECHO} "REVISION  = " $(shell git rev-list --abbrev-commit HEAD | wc -l | ${SED} "s/ //g")

config.h.echo:
	@${ECHO} "#define POTION_CC     \"${CC}\""
	@${ECHO} "#define POTION_CFLAGS \"${CFLAGS}\""
	@${ECHO} "#define POTION_LDFLAGS \"${LDFLAGS}\""
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
	-test -d bin || mkdir bin
	-test -d lib/p2 || mkdir lib/p2
	@${ECHO} "# -*- makefile -*-" > config.inc
	@${ECHO} "# created by ${MAKE} -f config.mak" >> config.inc
	@${MAKE} -s -f config.mak config.inc.echo >> $@

# Force sync with config.inc
core/config.h: config.inc core/version.h tools/config.sh config.mak
	@${ECHO} MAKE $@
	@${CAT} core/version.h > core/config.h
	@${MAKE} -s -f config.mak config.h.echo >> core/config.h

# TODO: fix when git is not available
core/version.h: $(shell git show-ref HEAD | ${SED} "s,^.* ,.git/,g")
	@${ECHO} MAKE $@
	@${ECHO} "/* created by ${MAKE} -f config.mak */" > core/version.h
	@${ECHO} "#define POTION_MAJOR ${POTION_MAJOR}" >> core/version.h
	@${ECHO} "#define POTION_MINOR ${POTION_MINOR}" >> core/version.h
	@${ECHO} "#define POTION_VERSION \"${POTION_MAJOR}.${POTION_MINOR}\"" >> core/version.h
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
