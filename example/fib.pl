sub fib {
  my $n = $_[0];
  return ( $n < 2 ) ? $n : fib( $n - 1 ) + fib( $n - 2 );
}
my $N = shift || 28;
print("fib($N) = ", fib($N), "\n");
