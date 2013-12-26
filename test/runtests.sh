#!/bin/sh
# usage: test/runtests.sh [-q] [testfile]
#        cmd="valgrind ./potion" test/runtests.sh

cmd=${cmd:-bin/potion}
cmd2=${cmd2:-bin/p2}
ECHO=/bin/echo
SED=sed
EXPR=expr
EXT=pn
maxpass=6

# make -s $cmd
count=0; failed=0; pass=0
cmdi="$cmd -I"; cmdx="$cmdi -X"
cmdc="$cmd -c"; extc=b

QUIET=
if [ "x$1" = "x-q" ]; then QUIET=1; shift; fi

if [ "x$1" = "x-pn" ]; then shift; maxpass=3; fi
if [ "x$1" = "x-p2" ]; then 
    shift
    maxpass=3
    cmd=${cmd2}; EXT=pl
    cmdi="$cmd --inspect"; cmdx="$cmdi -J"
    cmdc="$cmd --compile"; extc=c
fi

test -f $cmd || make -s $cmd $MAKEFLAGS
test -f $cmd2 || make -s $cmd2 $MAKEFLAGS
old_LIBRARY_PATH="$LD_LIBRARY_PATH"
export LD_LIBRARY_PATH="lib:$LD_LIBRARY_PATH"

verbose() {
  if [ "x$QUIET" = "x" ]; then ${ECHO} $@; else ${ECHO} -n .; fi
}

if test -z $1; then
    make -s bin/potion-test bin/p2-test bin/gc-test $MAKEFLAGS
    ${ECHO} running potion API tests
    bin/potion-test
    if [ $EXT = pl ]; then
      ${ECHO} running p2 API tests
      bin/p2-test
    fi
    ${ECHO} running GC tests
    bin/gc-test
fi

while [ $pass -lt $maxpass ]; do 
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
            cmd=${cmd2}; t=0; EXT=pl
	    cmdi="$cmd --inspect"; cmdx="$cmdi -J";
	    cmdc="$cmd --compile"; extc=c
	    whattests="$cmd VM tests"
	fi;
    elif [ $pass -eq 3 ]; then 
        cmd=${cmd2}; t=0; EXT=pl
	cmdi="$cmd --inspect"; cmdx="$cmdi -J";
	cmdc="$cmd --compile"; extc=c
	whattests="$cmd VM tests"
    elif [ $pass -eq 4 ]; then 
        t=1; 
	whattests="$cmd compiler tests"
    elif [ $pass -eq 5 ]; then 
        t=2;
	whattests="$cmd JIT tests"
	jit=`$cmd -v | sed "/jit=1/!d"`
	if [ "$jit" = "" ]; then 
	    pass=`expr $pass + 1`
	    break
	fi
    fi

    if test -n "$1" && test -f "$1"; then
	what=$1
	if [ ${what%.pl} = $what -a $EXT = pl -a $pass -ge 3 ]; then
	    ${ECHO} skipping p2
	    pass=6
	    break
	fi
	if [ ${what%.pn} = $what -a $EXT = pn -a $pass -le 3 ]; then
	    ${ECHO} skipping potion
	    pass=3
            cmd=${cmd2}; t=0; EXT=pl
	    cmdi="$cmd --inspect"; cmdx="$cmdi -J";
	    cmdc="$cmd --compile"; extc=c
	    whattests="$cmd VM tests"
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
else
    ${ECHO} "OK ($count tests)"
fi

#if [ -z "`grep DDEBUG config.inc`" ]; then
#    ${ECHO} run examples
#    log=log.example
#    git log -1 >> $log
#    for f in example/*.pn; do
#        ${ECHO} time bin/potion $f | tee -a $log
#        time bin/potion $f 2>&1 | tee -a $log
#    done
#    for f in example/*.p[2l]; do
#        ${ECHO} time bin/p2 $f | tee -a $log
#        time bin/p2 $f 2>&1 | tee -a $log
#    done
#fi

export LD_LIBRARY_PATH="$old_LIBRARY_PATH"
