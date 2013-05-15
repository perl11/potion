$a = if (0) { 12 }
  elsif (1) { 14 }
  else { 16 }

$b = if (0) { 12 }
  elsif (undef) { 14 }
  else { 16 }

$c = if (1) { 12 }
  elsif (1) { 14 }
  else { 16 }

$d = if (1) { 'P2' } else { 10 }
$e = if (1) { 10 }

($a, $b, $c, $d, $e) #=> 14
# not all statements parsed yet, only the first
#TODO =>(14, 16, 12, P2, 10)
