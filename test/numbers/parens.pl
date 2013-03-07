# ./p2 -Dv test/numbers/parens.pl
# -- parsed --
# code  (expr (times (list (expr (plus (value (9 undef undef) value (1 undef undef))))
#                    list (expr (minus (value (6 undef undef) value (4 undef undef)))))))

# TODO: (times (expr (list (plus (expr (value (9 nil nil)) expr (value (1 nil nil))))) 
#              expr (list (minus (expr (value (6 nil nil)) expr (value (4 nil nil)))))))

(9 + 1) * (6 - 4)  #=> 20
