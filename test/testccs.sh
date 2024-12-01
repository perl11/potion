#!/bin/sh
# test some compiler configurations on this platform (linux, darwin, win)
# cannot test cross with this yet
if [ -z $CCS ]; then
case `uname -o` in
*Linux) CCS="clang clang-3.3 gcc gcc-4.4 gcc-4.5 gcc-4.6 gcc-4.7 gcc-4.8 gcc-5" ;;
Darwin) CCS="clang clang-3.2 clang-3.4 clang-mp-3.3 clang-mp-3.5 clang-mp-3.6 clang-mp-3.7
             gcc gcc-mp-4.3 gcc-mp-4.8 gcc-mp-4.9 gcc-mp-5" ;;
Cygwin) CCS="clang gcc gcc-3" ;;
esac
fi

cp config.inc config.inc.test
testdebug() {
    make -s realclean >/dev/null 2>/dev/null
    echo make CC="$1" DEBUG=$2
    make -s CC="$1" DEBUG=$2 >/dev/null 2>/dev/null
    make -s pn test/api/potion-test >/dev/null 2>/dev/null
    make test
    echo make CC="$1" DEBUG=$2
    echo ---------------------
}

dotest() {
    testdebug "$1" 0
    testdebug "$1" 1
}

for c in $CCS
do
    dotest "$c"
done

rm 3rd/libuv/Makefile
testdebug "gcc -m32" 0
if [ `uname -o` = Darwin ]; then
    DYLD_LIBRARY_PATH=/usr/local/lib32 testdebug "gcc -m32" 1
fi

if test -f /opt/intel/bin/icc; then
    rm 3rd/libuv/Makefile
    case `uname -m` in
        x86_64) /opt/intel/bin/compilervars.sh intel64
            ;;
        i386)   /opt/intel/bin/compilervars.sh ia32
            ;;
    esac
    dotest icc
fi

rm 3rd/libuv/Makefile
mv config.inc.test config.inc
make
