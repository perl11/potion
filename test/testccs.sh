#!/bin/sh
# test some compiler configurations on this platform (linux, darwin, win)
CCS="clang clang-mp-3.3 gcc gcc-mp-4.3 gcc-mp-4.8 gcc-mp-4.9"

testdebug() {
    make -s realclean
    echo make CC="$1" DEBUG=$2
    make -s CC="$1" DEBUG=$2 >/dev/null 2>/dev/null
    make -s bin/potion bin/potion-test >/dev/null 2>/dev/null
    make test.pn; make test.p2
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
