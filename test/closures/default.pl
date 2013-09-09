sub min ($x=0, $y=1) { $y - $x }
(min(), min(1), min(0,1), min($y=0), min, min->arity, min->minargs)
#=> (1, 0, 1, 1, function(x:=0,y:=1), 2, 0)
