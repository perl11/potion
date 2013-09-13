sub fib($n) {
  if ( $n < 2 ) { $n } else { $x=$n-1; fib($x) + fib($x-1) }
}
$n = number $ARGV[0];
if (!$n) { $n = 40 }
print "fib("
print $n
print ")= "
say fib($n)
