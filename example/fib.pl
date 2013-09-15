sub fib {
  my $n = shift;
  ( $n < 2 ) ? $n : fib( $n - 1 ) + fib( $n - 2 );
}
$n = $ARGV[0] || 40;
print("fib($N) = ", fib($N), "\n");
