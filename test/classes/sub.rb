BTree = class (r, l): /left = l, /right = r.
b = BTree (52, 7)

CTree = BTree class
c = CTree (81, 9)

(b /left, b /right, c /left, c /right)
# (7, 52, 9, 81)
