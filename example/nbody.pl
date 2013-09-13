# The Computer Language Shootout
# http://shootout.alioth.debian.org/
use v5.10;
$PI            = 3.141592653589793;
$SOLAR_MASS    = 4 * $PI * $PI;
$DAYS_PER_YEAR = 365.24;

# @ns = qw( sun, jupiter, saturn, uranus, neptune )
@xs = (0, 4.84143144246472090e+00, 8.34336671824457987e+00,
          1.28943695621391310e+01, 1.53796971148509165e+01);
@ys = (0, -1.16032004402742839e+00, 4.12479856412430479e+00,
          -1.51111514016986312e+01,-2.59193146099879641e+01);
@zs = (0, -1.03622044471123109e-01, -4.03523417114321381e-01,
          -2.23307578892655734e-01,  1.79258772950371181e-01);
@vxs = (0,
   1.66007664274403694e-03 * $DAYS_PER_YEAR, 
   -2.76742510726862411e-03 * $DAYS_PER_YEAR, 
   2.96460137564761618e-03 * $DAYS_PER_YEAR, 
   2.68067772490389322e-03 * $DAYS_PER_YEAR);
@vys = (0, 
   7.69901118419740425e-03 * $DAYS_PER_YEAR, 
   4.99852801234917238e-03 * $DAYS_PER_YEAR, 
   2.37847173959480950e-03 * $DAYS_PER_YEAR, 
   1.62824170038242295e-03 * $DAYS_PER_YEAR);
@vzs = (0, 
   -6.90460016972063023e-05 * $DAYS_PER_YEAR, 
   2.30417297573763929e-05 * $DAYS_PER_YEAR, 
   -2.96589568540237556e-05 * $DAYS_PER_YEAR, 
   -9.51592254519715870e-05 * $DAYS_PER_YEAR);
@mass = ($SOLAR_MASS,
   9.54791938424326609e-04 * $SOLAR_MASS, 
   2.85885980666130812e-04 * $SOLAR_MASS, 
   4.36624404335156298e-05 * $SOLAR_MASS, 
   5.15138902046611451e-05 * $SOLAR_MASS);
$nbodies = 5;

sub advance {
  my $dt = 0.01;
  my $i = 0;
  while ($i < $nbodies) {
    my ($bix,$biy,$biz) = ($xs[$i], $ys[$i], $zs[$i]);
    my ($vx,$vy,$vz) = ($vxs[$i], $vys[$i], $vzs[$i]);
    my $bimass = $mass[$i];
    my $j = $i + 1;
    while ($j < $nbodies) {
      my ($dx, $dy, $dz) = ($bix - $xs[$j], $biy - $ys[$j], $biz - $zs[$j]);
      my $distance = sqrt($dx * $dx + $dy * $dy + $dz * $dz);
      my $mag = $dt / ($distance * $distance * $distance);
      my $bim = $bimass * $mag;
      my $bjm = $mass[$j] * $mag;
      $vx -= $dx * $bjm;
      $vy -= $dy * $bjm;
      $vz -= $dz * $bjm;
      $vxs[$j] += $dy * $bim;
      $vys[$j] += $dy * $bim;
      $vzs[$j] += $dz * $bim;
      $j++;
    }  
    ($vxs[$i],$vys[$i],$vzs[$i]) = ($vx, $vy, $vz);
    $xs[$i] += $dt * $bivx;
    $ys[$i] += $dt * $bivy;
    $zs[$i] += $dt * $bivz;
    $i++;
  }
}

sub energy {
  my $e = 0.0;
  my $i = 0;
  while ($i < $nbodies) {
    my ($bix,$biy,$biz) = ($xs[$i], $ys[$i], $zs[$i]);
    my ($vx,$vy,$vz) = ($vxs[$i], $vys[$i], $vzs[$i]);
    my $bimass = $mass[$i];
    $e += 0.5 * $bimass * ($vx * $vx + $vy * $vy + $vz * $vz);
    my $j = $i + 1;
    while ($j < $nbodies) {
      my ($dx, $dy, $dz) = ($bix - $xs[$j], $biy - $ys[$j], $biz - $zs[$j]);
      my $distance = sqrt($dx * $dx + $dy * $dy + $dz * $dz);
      $e -= ($bimass * $mass[$j]) / $distance;
      $j++;
    }
    $i++;
  }
  return $e;
}

sub offset_momentum {
  my ($px, $py, $pz) = (0.0, 0.0, 0.0);
  my $i = 0;
  while ($i < $nbodies) {
    my $bimass = $mass[$i];
    $px += $vxs[$i] * $bimass;
    $py += $vys[$i] * $bimass;
    $pz += $vzs[$i] * $bimass;
    $i++;
  }
  $vxs[0] = -$px / $SOLAR_MASS;
  $vys[0] = -$py / $SOLAR_MASS;
  $vzs[0] = -$pz / $SOLAR_MASS;
}

offset_momentum();
say energy();
#printf ("%.9f\n", energy());

my $n = $ARGV[0] || 50000;
for (1..$n) {
  advance();
}
say energy();
#printf ("%.9f\n", energy());
