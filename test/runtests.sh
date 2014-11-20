#!/bin/sh
# usage: test/runtests.sh [-q] [testfile]
#        cmd="valgrind -q bin/potion" test/runtests.sh

cmd=${cmd:-bin/potion}
ECHO=/bin/echo
SED=sed
EXPR=expr

# make -s $cmd
count=0; failed=0; pass=0
EXT=pn;
cmdi="$cmd -I"; cmdx="$cmdi -X"; 
cmdc="$cmd -c"; extc=b
QUIET=
if [ "x$1" = "x-q" ]; then QUIET=1; shift; fi

verbose() {
  if [ "x$QUIET" = "x" ]; then ${ECHO} $@; else ${ECHO} -n .; fi
}

if test -z $1; then
    # make -s test/api/potion-test test/api/gc-test
    ${ECHO} running potion API tests; 
    test/api/potion-test; 
    ${ECHO} running GC tests; 
    test/api/gc-test;
fi

while [ $pass -lt 3 ]; do 
    ${ECHO}; 
    if [ $pass -eq 0 ]; then 
	t=0; 
	whattests="$cmd VM tests"
    elif [ $pass -eq 1 ]; then 
        t=1; 
	whattests="$cmd compiler tests"
    elif [ $pass -eq 2 ]; then 
        t=2; 
	whattests="$cmd JIT tests"
	jit=`$cmd -v | sed "/jit=1/!d"`; 
	if [ "$jit" = "" ]; then 
	    pass=`expr $pass + 1`
	    break
	fi;
    fi

    if test -n "$1" && test -f "$1"; then
	what=$1
	if [ ${what%.pn} = $what -a $EXT = pn -a $pass -le 3 ]; then
	    ${ECHO} skipping potion
	    break
	fi
    else
        if $(grep "SANDBOX = 1" config.inc >/dev/null); then
	    what=`ls test/**/*.$EXT|grep -Ev "test/misc/(buffile|load)\.$EXT"`
        else
	    what=test/**/*.$EXT
        fi
    fi

    ${ECHO} running $whattests

    for f in $what; do 
	look=`cat $f | sed "/\#=>/!d; s/.*\#=> //"`
	#echo look=$look
	if [ $t -eq 0 ]; then
	    verbose $cmdi -B $f
	    for=`$cmdi -B $f | sed "s/\n$//"`
	elif [ $t -eq 1 ]; then
	    $cmdc $f > /dev/null
	    fb=$f$extc
	    verbose $cmdi -B $fb
	    for=`$cmdi -B $fb | sed "s/\n$//"`
	    rm -rf $fb
	else
	    verbose $cmdx $f
	    for=`$cmdx $f | sed "s/\n$//"`
	fi;
	if [ "$look" != "$for" ]; then
	    ${ECHO}
	    ${ECHO} "** $f: expected <$look>, but got <$for>"
	    failed=`expr $failed + 1`
	else
	    # ${ECHO} -n .
	    jit=`$cmd -v | ${SED} "/jit=1/!d"`
	    if [ "$jit" = "" ]; then
		${ECHO} "* skipping"
		break
	    fi
	fi
	count=`expr $count + 1`
    done
    pass=`expr $pass + 1`
done

${ECHO}
if [ $failed -gt 0 ]; then
    ${ECHO} "$failed FAILS ($count tests)"
    exit 1
else
    ${ECHO} "OK ($count tests)"
fi
  
