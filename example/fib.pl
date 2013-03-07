sub fib($n) {
  return ( $n < 2 ) ? $n : fib( $n - 1 ) + fib( $n - 2 );
}
#my $N = shift || 28;
my $N = 28;
say "fib($N) = ", fib($N);
