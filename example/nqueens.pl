my ($board_size, @occupied, @past, @solutions);
 
sub try_column {
  my ($depth, @diag) = shift;
  if ($depth == $board_size) {
    push @solutions, "@past\n";
    return;
  }
  local $" = ",";
 
  # @diag: marks cells diagonally attackable by any previous queens.
  #        Here it's pre-allocated to double size just so we don't need
  #        to worry about negative indices.
  $#diag = 2 * $board_size;
  for (0 .. $#past) {
    $diag[ $past[$_] + $depth - $_ ] = 1;
    $diag[ $past[$_] - $depth + $_ ] = 1;
  }
  print "depth:$depth, diag*2: (".join(",",@diag)."), past:(@past)\n" if @ARGV;
 
  for my $row (0 .. $board_size - 1) {
    next if $occupied[$row] || $diag[$row];
 
    # @past:     row numbers of previous queens
    # @occupied: rows already used. This gets inherited by each
    #            recursion so we don't need to repeatedly look them up
    push @past, $row;
    $occupied[$row] = 1;
    print "row:$row, past:(".join(",",@past)."), occupied:(".join(",",@occupied).")\n" if @ARGV;
 
    try_column($depth + 1);
 
    # clean up, for next recursion
    $occupied[$row] = 0;
    pop @past;
  }
}
 
$board_size = shift || 8;  # 12: 14,200 solutions
try_column(0);
 
local $" = "\n";
print @solutions if @ARGV;
print "board $board_size: ", scalar(@solutions), " solutions\n";
