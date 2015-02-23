types representation
--------------------

builtin types: 1 bit
user types: short as offset (25000+n).

we also need a size, for all current types (x or y or ...)
maxsize=15 (we collect only a certain number of types until we
unify some into the next common upper type, and replace them by it)

# 1-2 words:
struct type {
  uint32_t size:4;  # 0-15 size of types[]
  uint32_t nil:1;
  uint32_t num:1;
  uint32_t bool:1;
  uint32_t dbl:1;
  uint32_t bits:24; # and 4-23 other coretype combinations as bit
  uint32_t hint:1;  # hint in last type
  uint32_t free:2;  #
  short types[0-15];#range: max 32767 user types, max length: 15
};

type of types
-------------
parsed or strictly inferred types
vs compile-time type hints (the most likely types)
vs run-time observed types (traced-based, see luajit.
  after some >hits we want to add an optimization on this type.)

bits reflect 25000 offsets in potion.h
0 nil
1 num
2 bool
3 dbl
4 int
5 str
7 closure ({sig}, result)
8 tuple  of value (int=>any)
10 file
17 table of key=>value (str=>any)
18 lick  of {str,any,any}
...
23 any
24 user++

type *unify_types(exp *eo, type *a, type *b, int a_is_upper_limit=TRUE);
type *exact_unify(exp *eo, type *a, type *b);

typeprim
typeaggr
typeobj
typetuple
typefun
subtype
param

Links
-----
http://www.typescriptlang.org/Content/TypeScript%20Language%20Specification.pdf  (microsoft's javascript with types)
https://code.facebook.com/posts/1505962329687926/flow-a-new-static-type-checker-for-javascript/ (facebook's javascript with types)
https://github.com/rwaldron/tc39-notes/blob/master/es6/2015-01/JSExperimentalDirections.pdf Soundscript, google's javascript with types
https://www.python.org/dev/peps/pep-0484/  (planned python with types)
http://www.mypy-lang.org/ (existing python with types)
https://news.ycombinator.com/item?id=8620129 (ruby 3.0 planned with types)
http://crystal-lang.org/ (a good existing ruby with types)
http://hacklang.org/ (facebook's php with types)
http://blog.pascal-martin.fr/post/in-favor-of-rfc-scalar-type-hints.html (php 7 types overview)
https://wiki.php.net/rfc/scalar_type_hints (php 7)
https://wiki.php.net/rfc/return_types (php 7)
https://github.com/Microsoft/TypeScript/issues/1265 (Comparison with Facebook Flow Type System)

