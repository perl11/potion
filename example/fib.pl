sub fib($n) {
  if ( $n < 2 ) { $n } else { fib($n-1) + fib($n-2) }
}
my $N = 40;
print "fib("
print $N
print ") = "
print fib($N)
""
