///\file number.c
/// simple arithmetic.
/// PNNumber is either an immediate PN_INT integer (with one bit for a tag, range 2^30/2^62)
/// or a boxed full-precision double PNDouble.
//
// (c) 2008 why the lucky stiff, the freelance professor
// (c) 2013-2014 perel11.org
//
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include "potion.h"
#include "internal.h"

/// new PNDouble (double)
PN potion_double(Potion *P, double v) {
  vPN(Double) d = PN_ALLOC(PN_TNUMBER, struct PNDouble);
  d->value = v;
  return (PN)d;
}

/// Convert string to double
PN potion_strtod(Potion *P, char *str, int len) {
  char *ptr = str + len;
  return potion_double(P, strtod(str, &ptr));
}
/**\memberof PNNumber
  "**" method.
  not static, needed in x86 jit.
 \param sup PNNumber
 \return PNNumber (PNInteger or PNDouble) */
PN potion_num_pow(Potion *P, PN cl, PN self, PN sup) {
  double x = PN_DBL(self), y = PN_DBL(sup);
  double z = pow(x, y);
  if (PN_IS_INT(self) && PN_IS_INT(sup) && fabs(z) < INT_MAX)
    return PN_NUM((int)z);
  return potion_double(P, z);
}
/**\memberof PNNumber
  "sqrt" as double
 \return PNDouble */
static PN potion_num_sqrt(Potion *P, PN cl, PN self) {
  return potion_double(P, sqrt(PN_DBL(self)));
}

#define PN_NUM_MATH(math_op) \
  DBG_CHECK_NUM(self); \
  DBG_CHECK_NUM(num); \
  if (PN_IS_INT(self) && PN_IS_INT(num)) \
    return PN_NUM(PN_INT(self) math_op PN_INT(num)); \
  return potion_double(P, PN_DBL(self) math_op PN_DBL(num));
#define PN_INT_MATH(math_op) \
  DBG_CHECK_INT(self); \
  DBG_CHECK_INT(num); \
  return PN_NUM(PN_INT(self) math_op PN_INT(num));
#define PN_DBL_MATH(math_op) \
  DBG_CHECK_NUM(self); \
  DBG_CHECK_NUM(num); \
  return potion_double(P, PN_DBL(self) math_op PN_DBL(num));

/**\memberof PNNumber
  "+" method
 \param num PNNumber
 \return PNInteger (if both are INT) or PNDouble */
static PN potion_num_add(Potion *P, PN cl, PN self, PN num) {
  PN_NUM_MATH(+)
}

/**\memberof PNNumber
  "-" method
 \param num PNNumber
 \return PNInteger (if both are INT) or PNDouble */
static PN potion_num_sub(Potion *P, PN cl, PN self, PN num) {
  PN_NUM_MATH(-)
}

/**\memberof PNNumber
  "*" method
 \param num PN_INT or PNDouble
 \return PNInteger (if both are INT) or PNDouble */
static PN potion_num_mult(Potion *P, PN cl, PN self, PN num) {
  PN_NUM_MATH(*)
}

/**\memberof PNNumber
  "/" method
 \param num PNNumber
 \return PNInteger (if both are INT) or PNDouble */
static PN potion_num_div(Potion *P, PN cl, PN self, PN num) {
  PN_NUM_MATH(/)
}

/**\memberof PNNumber
  "%" method, Integer division, resp rounded division
 \param num PNNumber
 \return PNInteger (if both are INT) or PNDouble */
static PN potion_num_rem(Potion *P, PN cl, PN self, PN num) {
  if (PN_IS_INT(self) && PN_IS_INT(num))
    return PN_NUM(PN_INT(self) % PN_INT(num));
  double x = PN_DBL(self), y = PN_DBL(num);
  int z = (int)(x / y);
  return potion_double(P, x - (y * (double)z));
}

/**\memberof PNNumber
  "~" method, bitwise negation  \code ~ 0 #=> -1 \endcode
 \return PNInteger or 0.0 for PNDouble argument */
static PN potion_num_bitn(Potion *P, PN cl, PN self) {
  if (PN_IS_INT(self))
    return PN_NUM(~PN_INT(self));
  return (PN)potion_double(P, 0.0);
}

/**\memberof PNNumber
  "<<" method, left shift by num. \code 2 << 1 #=> 4 \endcode
 \param num PN_INT or PNDouble
 \return PNInteger or 0.0 for a PNDouble argument */
static PN potion_num_bitl(Potion *P, PN cl, PN self, PN num) {
  if (PN_IS_INT(self) && PN_IS_INT(num))
    return PN_NUM(PN_INT(self) << PN_INT(num));
  return (PN)potion_double(P, 0.0);
}

/**\memberof PNNumber
  ">>" method, right shift by num. \code 4 >> 1 #=> 2 \endcode
 \param num PN_INT or PNDouble
 \return PNInteger or 0.0 for a PNDouble argument */
static PN potion_num_bitr(Potion *P, PN cl, PN self, PN num) {
  if (PN_IS_INT(self) && PN_IS_INT(num))
    return PN_NUM(PN_INT(self) >> PN_INT(num));
  return (PN)potion_double(P, 0.0);
}

/**\memberof PNInteger
  "+" method
 \param num PNInteger
 \return PNInteger */
static PN potion_int_add(Potion *P, PN cl, PN self, PN num) { PN_INT_MATH(+) }
/**\memberof PNInteger
  "-" method
 \param num PNInteger
 \return PNInteger */
static PN potion_int_sub(Potion *P, PN cl, PN self, PN num) { PN_INT_MATH(-) }
/**\memberof PNInteger
  "*" method
 \param num PNInteger
 \return PNInteger */
static PN potion_int_mult(Potion *P, PN cl, PN self, PN num) { PN_INT_MATH(*) }
/**\memberof PNInteger
  "/" method
 \param num PNInteger
 \return PNInteger */
static PN potion_int_div(Potion *P, PN cl, PN self, PN num) { PN_INT_MATH(/) }

/**\memberof PNNumber
  "%" method, Integer division
 \param num PNInteger
 \return PNInteger */
static PN potion_int_rem(Potion *P, PN cl, PN self, PN num) { PN_INT_MATH(%) }

/**\memberof PNInteger
  "~" method, bitwise negation  \code ~ 0 #=> -1 \endcode
 \return PNInteger */
static PN potion_int_bitn(Potion *P, PN cl, PN self) {
  return PN_NUM(~PN_INT(self));
}

/**\memberof PNInteger
  "<<" method, left shift by num. \code 2 << 1 #=> 4 \endcode
 \param num PNInteger
 \return PNInteger */
static PN potion_int_bitl(Potion *P, PN cl, PN self, PN num) {
  return PN_NUM(PN_INT(self) << PN_INT(num));
}

/**\memberof PNInteger
  ">>" method, right shift by num. \code 4 >> 1 #=> 2 \endcode
 \param num PNInteger
 \return PNInteger */
static PN potion_int_bitr(Potion *P, PN cl, PN self, PN num) {
  return PN_NUM(PN_INT(self) >> PN_INT(num));
}
/**\memberof PNInteger
  "string" method, stringify
 \return PNString */
PN potion_int_string(Potion *P, PN cl, PN self) {
  char ints[40];
  sprintf(ints, "%ld", PN_INT(self));
  return potion_str(P, ints);
}

/**\memberof PNNumber
  "number" method, identity. \code 4 number #=> 4 \endcode
 \return PNInteger or PNDouble */
static PN potion_num_number(Potion *P, PN cl, PN self) {
  return self;
}
/**\memberof PNNumber
  "double" cast
 \return PNDouble */
static PN potion_num_double(Potion *P, PN cl, PN self) {
  if (PN_IS_INT(self))
    return potion_double(P, (double)PN_INT(self));
  else
    return self;
}
/**\memberof PNNumber
  "integer" cast
 \return floor rounded PNInteger */
static PN potion_num_integer(Potion *P, PN cl, PN self) {
  if (PN_IS_INT(self))
    return self;
  else
    return PN_NUM(floor(((struct PNDouble *)self)->value));
}
/**\memberof PNDouble
  "+" method
 \param num PNDouble
 \return PNDouble */
static PN potion_dbl_add(Potion *P, PN cl, PN self, PN num) { PN_DBL_MATH(+) }
/**\memberof PNDouble
  "-" method
 \param num PNDouble
 \return PNDouble */
static PN potion_dbl_sub(Potion *P, PN cl, PN self, PN num) { PN_DBL_MATH(-) }
/**\memberof PNDouble
  "*" method
 \param num PNDouble
 \return PNDouble */
static PN potion_dbl_mult(Potion *P, PN cl, PN self, PN num) { PN_DBL_MATH(*) }
/**\memberof PNDouble
  "/" method
 \param num PNDouble
 \return PNDouble */
static PN potion_dbl_div(Potion *P, PN cl, PN self, PN num) { PN_DBL_MATH(/) }

/**\memberof PNDouble
  "string" method, stringify
 \return PNString */
PN potion_dbl_string(Potion *P, PN cl, PN self) {
  char ints[40];
  int len = sprintf(ints, "%.16f", ((struct PNDouble *)self)->value);
  while (len > 0 && ints[len - 1] == '0') len--;
  if (ints[len - 1] == '.') len++;
  ints[len] = '\0';
  return potion_str(P, ints);
}
/**\memberof PNNumber
  "string" method, stringify
 \return PNString */
PN potion_num_string(Potion *P, PN cl, PN self) {
  char ints[40];
  if (PN_IS_INT(self)) {
    sprintf(ints, "%ld", PN_INT(self));
  } else {
    int len = sprintf(ints, "%.16f", ((struct PNDouble *)self)->value);
    while (len > 0 && ints[len - 1] == '0') len--;
    if (ints[len - 1] == '.') len++;
    ints[len] = '\0';
  }
  return potion_str(P, ints);
}
/**\memberof PNNumber
  "times" call block times (int only). argument starting with 0.
          \code 2 times: "x" print.         #=> xx \endcode
          \code 2 times(i): i string print. #=> 01 \endcode

          Note: Do not mix it up with the 'mult' opcode which is parsed as AST_TIMES.
 \verbatim $ potion -Div -e'4 * 2'
        -- parsed --
        code (times (expr (value (4 )) expr (value (2 ))))
        -- compiled --
        [1] loadpn   0 9    ; 4
        [2] loadpn   1 5    ; 2
        [3] mult     0 1
        [4] return   0
 \endverbatim
 \param block PNClosure
 \return self PNInteger (normally unused)
 \sa potion_num_to */
static PN potion_int_times(Potion *P, PN cl, PN self, PN block) {
  long i, j = PN_INT(self);
  PN_CHECK_INT(self);
  //PN_CHECK_CLOSURE(block);
  if (PN_TYPE(block) != PN_TCLOSURE)
    potion_fatal("block argument for times is not a closure");
  for (i = 0; i < j; i++)
    PN_CLOSURE(block)->method(P, block, P->lobby, PN_NUM(i));
  return PN_NUM(i);
}
/**\memberof PNNumber
  from "to" end: block. call block on each number from self to end
                 \code 1 to 2: "x" print. #=> "xx" \endcode
 \param end int
 \param block PNClosure
 \return self PNInteger (normally unused)
 \sa potion_num_times, potion_num_step */
static PN potion_int_to(Potion *P, PN cl, PN self, PN end, PN block) {
  long i, s = 1, j = PN_INT(self), k = PN_INT(end);
  PN_CHECK_INT(self);
  PN_CHECK_INT(end);
  if (k < j) s = -1;
  if (PN_TYPE(block) != PN_TCLOSURE)
    potion_fatal("block argument for to is not a closure");
  for (i = j; i != k + s; i += s)
    PN_CLOSURE(block)->method(P, block, P->lobby, PN_NUM(i));
  return PN_NUM(labs(i - j));
}
/**\memberof PNNumber
  from "step" end step: block. call block on each number from self to end
                 \code 1 step 5 2(i): i string print. #=> 1 3 5 \endcode
 \param end  PNInteger (int only)
 \param step PNInteger (int only)
 \param block PNClosure
 \sa potion_num_to. */
static PN potion_int_step(Potion *P, PN cl, PN self, PN end, PN step, PN block) {
  long i, j = PN_INT(end), k = PN_INT(step);
  PN_CHECK_INT(self);
  PN_CHECK_INT(end);
  PN_CHECK_INT(step);
  if (PN_TYPE(block) != PN_TCLOSURE)
    potion_fatal("block argument for step is not a closure");
  for (i = PN_INT(self); i <= j; i += k) {
    PN_CLOSURE(block)->method(P, block, P->lobby, PN_NUM(i));
  }
  return PN_NUM(labs(i - j) / k);
}

/**\memberof PNNumber
  "chr" of int only, no UTF-8 multi-byte sequence yet.
 \return PNString one char <255 */
static PN potion_int_chr(Potion *P, PN cl, PN self) {
  char c = PN_INT(self);
  DBG_CHECK_INT(self);
  return PN_STRN(&c, 1);
}
/**\memberof PNNumber
  "number?"
 \return PNBoolean true or false */
static PN potion_num_is_number(Potion *P, PN cl, PN self) {
  return (PN_IS_INT(self) || PN_IS_DBL(self)) ? PN_TRUE : PN_FALSE;
}
/**\memberof PNNumber
  "integer?"
 \return PNBoolean true or false */
static PN potion_num_is_integer(Potion *P, PN cl, PN self) {
  return PN_IS_INT(self) ? PN_TRUE : PN_FALSE;
}
/**\memberof PNNumber
  "double?"
 \return PNBoolean true or false */
static PN potion_num_is_double(Potion *P, PN cl, PN self) {
  return PN_IS_DBL(self) ? PN_TRUE : PN_FALSE;
}
/**\memberof PNNumber
  "abs"
 \return PNInteger or PNDouble */
static PN potion_num_abs(Potion *P, PN cl, PN self) {
  DBG_CHECK_NUM(self);
  if (PN_IS_DBL(self)) {
    double d = PN_DBL(self);
    if (d < 0.0)
      return (PN) potion_double(P, -d);
    else
      return self;
  }
  return PN_NUM(labs(PN_INT(self)));
}
/**\memberof PNInteger
  "abs"
 \return PNInteger */
static PN potion_int_abs(Potion *P, PN cl, PN self) {
  PN_CHECK_INT(self);
  return PN_NUM(labs(PN_INT(self)));
}
/**\memberof PNDouble
  "abs"
 \return PNDouble */
static PN potion_dbl_abs(Potion *P, PN cl, PN self) {
  double d = PN_DBL(self);
  DBG_CHECK_NUM(self);
  if (d < 0.0)
    return (PN) potion_double(P, -d);
  else
    return self;
}
/**\memberof PNNumber
  "cmp" a number to a value. casts argument n to a number
                     \code 1 cmp 2 #=> -1 \endcode
                     \code 1 cmp 1 #=>  0 \endcode
                     \code 1 cmp 0 #=>  1 \endcode
 \param n PN number compared to
 \return PNInteger -1, 0 or 1
 \sa potion_tuple_sort. */
static PN potion_num_cmp(Potion *P, PN cl, PN self, PN n) {
  if (PN_IS_DBL(self)) {
    double d1 = ((struct PNDouble *)self)->value;
    double d2 = PN_DBL(potion_send(PN_number, n));
    return d1 < d2 ? PN_NUM(-1) : d1 == d2 ? PN_ZERO : PN_NUM(1);
  } else {
    long n1, n2;
    n1 = PN_INT(self);
    n2 = PN_IS_INT(n) ? PN_INT(n) : PN_INT(potion_send(PN_number, n));
    return n1 < n2 ? PN_NUM(-1) : n1 == n2 ? PN_ZERO : PN_NUM(1);
  }
}
/**\memberof PNDouble
  "cmp" a double to a value. casts argument n to a number
                     \code 1 cmp 2 #=> -1 \endcode
                     \code 1 cmp 1 #=>  0 \endcode
                     \code 1 cmp 0 #=>  1 \endcode
 \param n PN number compared to
 \return PNInteger -1, 0 or 1 */
static PN potion_dbl_cmp(Potion *P, PN cl, PN self, PN n) {
  double d1 = ((struct PNDouble *)self)->value;
  double d2 = PN_DBL(n);
  DBG_CHECK_DBL(self);
  PN_CHECK_NUM(n);
  return d1 < d2 ? PN_NUM(-1) : d1 == d2 ? PN_ZERO : PN_NUM(1);
}
/**\memberof PNInteger
  "cmp" a number to a value. casts argument n to a number
                     \code 1 cmp 2 #=> -1 \endcode
                     \code 1 cmp 1 #=>  0 \endcode
                     \code 1 cmp 0 #=>  1 \endcode
 \param n PN number compared to
 \return PNInteger -1, 0 or 1
 \sa potion_tuple_sort. */
static PN potion_int_cmp(Potion *P, PN cl, PN self, PN n) {
  long n1, n2;
  DBG_CHECK_INT(self);
  DBG_CHECK_INT(n);
  n1 = PN_INT(self);
  n2 = PN_INT(n);
  return n1 < n2 ? PN_NUM(-1) : n1 == n2 ? PN_ZERO : PN_NUM(1);
}

void potion_num_init(Potion *P) {
  PN num_vt = PN_VTABLE(PN_TNUMBER);
  PN dbl_vt = PN_VTABLE(PN_TDOUBLE);
  PN int_vt = PN_VTABLE(PN_TINTEGER);
  potion_method(num_vt, "+", potion_num_add, "value=N");
  potion_method(num_vt, "-", potion_num_sub, "value=N");
  potion_method(num_vt, "*", potion_num_mult, "value=N");
  potion_method(num_vt, "/", potion_num_div, "value=N");
  potion_method(num_vt, "%", potion_num_rem, "value=N");
  potion_method(num_vt, "~", potion_num_bitn, 0);
  potion_method(num_vt, "<<", potion_num_bitl, "value=N");
  potion_method(num_vt, ">>", potion_num_bitr, "value=N");
  potion_method(num_vt, "**", potion_num_pow, "value=N");
  potion_method(num_vt, "abs", potion_num_abs, 0);
  potion_method(num_vt, "sqrt", potion_num_sqrt, 0);
  potion_method(num_vt, "cmp", potion_num_cmp, "value=o");
  potion_method(num_vt, "chr", potion_int_chr, 0);
  potion_method(num_vt, "string", potion_num_string, 0);
  potion_method(num_vt, "number", potion_num_number, 0);
  potion_method(num_vt, "integer", potion_num_integer, 0);
  potion_method(num_vt, "double", potion_num_double, 0);
  potion_method(num_vt, "number?", potion_num_is_number, 0);
  potion_method(num_vt, "integer?", potion_num_is_integer, 0);
  potion_method(num_vt, "double?", potion_num_is_double, 0);
  potion_method(num_vt, "rand", potion_num_rand, 0);
  // optimized double-only methods, for both operands
  potion_method(dbl_vt, "string", potion_dbl_string, 0);
  potion_method(dbl_vt, "+", potion_dbl_add, "value=D");
  potion_method(dbl_vt, "-", potion_dbl_sub, "value=D");
  potion_method(dbl_vt, "*", potion_dbl_mult, "value=D");
  potion_method(dbl_vt, "/", potion_dbl_div, "value=D");
  potion_method(dbl_vt, "abs", potion_dbl_abs, 0);
  potion_method(dbl_vt, "cmp", potion_dbl_cmp, "value=D");
  //potion_method(dbl_vt, "rand", potion_dbl_rand, "|bound=D");
  // optimized integer-only methods, for both operands
  potion_method(int_vt, "+", potion_int_add, "value=I");
  potion_method(int_vt, "-", potion_int_sub, "value=I");
  potion_method(int_vt, "*", potion_int_mult, "value=I");
  potion_method(int_vt, "/", potion_int_div, "value=I");
  potion_method(int_vt, "%", potion_int_rem, "value=I");
  potion_method(int_vt, "<<", potion_int_bitl, "value=I");
  potion_method(int_vt, ">>", potion_int_bitr, "value=I");
  potion_method(int_vt, "~", potion_int_bitn, 0);
  potion_method(int_vt, "chr", potion_int_chr, 0);
  potion_method(int_vt, "abs", potion_int_abs, 0);
  potion_method(int_vt, "cmp", potion_int_cmp, "value=I");
  //potion_method(int_vt, "rand", potion_int_rand, "|bound=I");
  potion_method(num_vt, "step", potion_int_step, "end=N,step=N,block=&");
  potion_method(num_vt, "times", potion_int_times, "block=&");
  potion_method(num_vt, "to",   potion_int_to, "end=N,block=&");
}
