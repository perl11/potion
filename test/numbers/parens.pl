# ./p2 -Dv test/numbers/parens.pl
# -- parsed --
# code  (expr (times (list (expr (plus (value (9) value (1))))
#                    list (expr (minus (value (6) value (4)))))))

# TODO: (times (expr (list (plus (expr (value (9)) expr (value (1)))))
#              expr (list (minus (expr (value (6)) expr (value (4)))))))

(9 + 1) * (6 - 4)  #=> 20
