sub fib($n) {
  if ( $n < 2 ) { $n } else { fib($n-1) + fib($n-2) }
}

my $N = 28;
#print("fib($N) = ", fib($N), "\n");
fib($N)
