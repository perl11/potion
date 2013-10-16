#!/bin/sh
# OPTIONS: -ci686-w64-mingw32-gcc ...
if [ -z "$1" ]; then
    case `uname -s` in
        *Linux) # native to x86_64, cross to i686 via -m32 and win32 via i686-w64-mingw32-gcc
            CC="gcc-4.8" #clang-3.3"
            CROSS="i686-w64-mingw32-gcc" ;; # x86_64-w64-mingw32-gcc win64 not yet, w64 not used for dist
        Darwin)
            CC="clang-mp-3.3" #gcc-mp-4.8"
            CROSS="i386-mingw32-gcc" ;;
        CYGWIN*) # native via gcc4
            CC="gcc-4"
            if [ `uname -m` = x86_64 ]; then #Cygwin64
                CC="gcc"
            fi
            ;;
    esac
    #LATER evtl.: ppc, arm, darwin pkg, win64 port
else
    while getopts "c:" opt
    do
        if [ "$opt" = "c" ]; then CROSS="$CROSS ${OPTARG}"; fi
        shift
    done
    OPTS=1
    CC=$@
fi

dorelease() {
    echo
    echo RELEASE $1
    make realclean
    echo make CC="$1"
    make CC="$1" DEBUG=0
    make test
    make static
    make docall dist
}

docross() {
    echo CROSS $1
    make clean
    make clean -C 3rd/libuv
    rm config.inc lib/lib* 3rd/libuv/Makefile
    echo make CC="$1" DEBUG=0 CROSS=1
    make -s -f config.mak CC="$1" DEBUG=0 CROSS=1
    touch bin/greg core/syntax.c
    make CC="$1" DEBUG=0 CROSS=1
    make static
    make dist
}

for c in $CC; do
    dorelease "$c"
done

if [ -n "$CROSS" ]; then
    # build greg and syntax.c native
    make clean
    make core/syntax.c

    for c in $CROSS; do
        docross "$c"
    done
fi

if [ -z "$OPTS" ]; then
    case `uname -s` in
        *Linux) rm 3rd/libuv/Makefile lib/lib*
                dorelease "gcc -m32" ;;
    esac
fi
