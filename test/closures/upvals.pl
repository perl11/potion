sub cl { cl }

$n = 4;
$m = 11;
sub cl2 { $m + $n }
$n = 6;

$o = 19;
sub cl3 {
  my sub cl4 { $o = 23 }
  cl4;
  $p = 45;
}

$x = 55;
sub cl4($y) {$x + $y}

$z1 = true;
$z2 = 16;
@z3 = (2);
$z4 = '(a=1)';

sub z() {
  $z4 = '(b=2)';
  $z4;
}

(cl (), cl2 (), cl3 (), cl4 (12), o, $z1, $z2, @z3, z())
#=> (sub(), 17, 45, 67, undef, undef, 16, (2), (b=2))
#TODO=> (cl, 17, 45, 67, 23, true, 16, 2, (b=2))
