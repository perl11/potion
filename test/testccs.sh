#!/bin/sh
# test some compiler configurations on this platform (linux, darwin, win)
CCS="clang clang2 clang3 gcc gcc-4.4 gcc-4.8"

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
