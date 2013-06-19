///\file number.c
/// simple math. PNNumber is either a immediate PN_NUM integer or double PNDecimal
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include "p2.h"
#include "internal.h"

/// new PNDecimal (double)
PN potion_real(Potion *P, double v) {
  vPN(Decimal) d = PN_ALLOC_N(PN_TNUMBER, struct PNDecimal, 0);
  d->value = v;
  return (PN)d;
}

/// strtod
PN potion_decimal(Potion *P, char *str, int len) {
  char *ptr = str + len;
  return potion_real(P, strtod(str, &ptr));
}
/**\memberof PNNumber
  "**" method.
  not static, needed in x86 jit.
 \param sup PNNumber
 \return PNNumber or PNDecimal */
PN potion_pow(Potion *P, PN cl, PN self, PN sup) {
  double x = PN_DBL(self), y = PN_DBL(sup);
  double z = pow(x, y);
  if (PN_IS_NUM(self) && PN_IS_NUM(sup) && abs(z) < INT_MAX)
    return PN_NUM((int)z);
  return potion_real(P, z);
}
/**\memberof PNNumber
  "sqrt" as double
 \return PNDecimal */
static PN potion_sqrt(Potion *P, PN cl, PN self) {
  return potion_real(P, sqrt(PN_DBL(self)));
}

#define PN_NUM_MATH(int_math) \
  if (PN_IS_NUM(self) && PN_IS_NUM(num)) \
    return PN_NUM(PN_INT(self) int_math PN_INT(num)); \
  return potion_real(P, PN_DBL(self) int_math PN_DBL(num));

/**\memberof PNNumber
  "+" method
 \param num PN_INT or PNDecimal
 \return PNNumber (if both are INT) or PNDecimal */
static PN potion_add(Potion *P, PN cl, PN self, PN num) {
  PN_NUM_MATH(+)
}

/**\memberof PNNumber
  "-" method
 \param num PN_INT or PNDecimal
 \return PNNumber (if both are INT) or PNDecimal */
static PN potion_sub(Potion *P, PN cl, PN self, PN num) {
  PN_NUM_MATH(-)
}

/**\memberof PNNumber
  "*" method
 \param num PN_INT or PNDecimal
 \return PNNumber (if both are INT) or PNDecimal */
static PN potion_mult(Potion *P, PN cl, PN self, PN num) {
  PN_NUM_MATH(*)
}

/**\memberof PNNumber
  "/" method
 \param num PN_INT or PNDecimal
 \return PNNumber (if both are INT) or PNDecimal */
static PN potion_div(Potion *P, PN cl, PN self, PN num) {
  PN_NUM_MATH(/)
}

/**\memberof PNNumber
  "%" method, Integer division, resp rounded division
 \param num PN_INT or PNDecimal
 \return PNNumber (if both are INT) or PNDecimal */
static PN potion_rem(Potion *P, PN cl, PN self, PN num) {
  if (PN_IS_NUM(self) && PN_IS_NUM(num))
    return PN_NUM(PN_INT(self) % PN_INT(num));
  double x = PN_DBL(self), y = PN_DBL(num);
  int z = (int)(x / y);
  return potion_real(P, x - (y * (double)z));
}

/**\memberof PNNumber
  "~" method, bitwise negation  \code ~ 0 #=> -1 \endcode
 \return PNNumber or 0.0 for PNDecimal argument */
static PN potion_bitn(Potion *P, PN cl, PN self) {
  if (PN_IS_NUM(self))
    return PN_NUM(~PN_INT(self));
  return (PN)potion_real(P, 0.0);
}

/**\memberof PNNumber
  "<<" method, left shift by num. \code 2 << 1 #=> 4 \endcode
 \param num PN_INT or PNDecimal
 \return PNNumber or 0.0 for a PNDecimal argument */
static PN potion_bitl(Potion *P, PN cl, PN self, PN num) {
  if (PN_IS_NUM(self) && PN_IS_NUM(num))
    return PN_NUM(PN_INT(self) << PN_INT(num));
  return (PN)potion_real(P, 0.0);
}

/**\memberof PNNumber
  ">>" method, right shift by num. \code 4 >> 1 #=> 2 \endcode
 \param num PN_INT or PNDecimal
 \return PNNumber or 0.0 for a PNDecimal argument */
static PN potion_bitr(Potion *P, PN cl, PN self, PN num) {
  if (PN_IS_NUM(self) && PN_IS_NUM(num))
    return PN_NUM(PN_INT(self) >> PN_INT(num));
  return (PN)potion_real(P, 0.0);
}
/**\memberof PNNumber
  "number" method, identity. \code 4 number #=> 4 \endcode
 \return PNNumber or PNDecimal */
static PN potion_num_number(Potion *P, PN cl, PN self) {
  return self;
}
/**\memberof PNNumber
  "string" method, stringify
 \return PNString */
PN potion_num_string(Potion *P, PN cl, PN self) {
  char ints[40];
  if (PN_IS_NUM(self)) {
    sprintf(ints, "%ld", PN_INT(self));
  } else {
    int len = sprintf(ints, "%.16f", ((struct PNDecimal *)self)->value);
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
 \return self PNNumber (normally unused)
 \sa potion_num_to */
static PN potion_num_times(Potion *P, PN cl, PN self, PN block) {
  long i, j = PN_INT(self);
  for (i = 0; i < j; i++)
    PN_CLOSURE(block)->method(P, block, P->lobby, PN_NUM(i));
  return PN_NUM(i);
}
/**\memberof PNNumber
  from "to" end: block. call block on each number from self to end
                 \code 1 to 2: "x" print. #=> "xx" \endcode
 \param end int
 \param block PNClosure
 \return self PNNumber (normally unused)
 \sa potion_num_times, potion_num_step */
static PN potion_num_to(Potion *P, PN cl, PN self, PN end, PN block) {
  long i, s = 1, j = PN_INT(self), k = PN_INT(end);
  if (k < j) s = -1;
  for (i = j; i != k + s; i += s)
    PN_CLOSURE(block)->method(P, block, P->lobby, PN_NUM(i));
  return PN_NUM(abs(i - j));
}
/**\memberof PNNumber
  from "step" end step: block. call block on each number from self to end
                 \code 1 step 5 2(i): i string print. #=> 1 3 5 \endcode
 \param end  PNNumber (int only)
 \param step PNNumber (int only)
 \param block PNClosure
 \sa potion_num_to. */
static PN potion_num_step(Potion *P, PN cl, PN self, PN end, PN step, PN block) {
  long i, j = PN_INT(end), k = PN_INT(step);
  for (i = PN_INT(self); i <= j; i += k) {
    PN_CLOSURE(block)->method(P, block, P->lobby, PN_NUM(i));
  }
}
/**\memberof PNNumber
  "chr" of int only, no UTF-8 multi-byte sequence
 \return PNString one char <255 */
static PN potion_num_chr(Potion *P, PN cl, PN self) {
  char c = PN_INT(self);
  return PN_STRN(&c, 1);
}
/**\memberof PNNumber
  "integer?"
 \return PNBoolean true or false */
static PN potion_num_is_integer(Potion *P, PN cl, PN self) {
  return PN_IS_NUM(self) ? PN_TRUE : PN_FALSE;
}
/**\memberof PNNumber
  "float?"
 \return PNBoolean true or false */
static PN potion_num_is_float(Potion *P, PN cl, PN self) {
  return PN_IS_DECIMAL(self) ? PN_TRUE : PN_FALSE;
}
/**\memberof PNNumber
  "integer" cast
 \return floor rounded PNNumber */
static PN potion_num_integer(Potion *P, PN cl, PN self) {
  if (PN_IS_NUM(self))
    return self;
  else
    return PN_NUM(floor(((struct PNDecimal *)self)->value));
}
/**\memberof PNNumber
  "abs"
 \return PNNumber or PNDecimal */
static PN potion_abs(Potion *P, PN cl, PN self) {
  if (PN_IS_DECIMAL(self)) {
    double d = PN_DBL(self);
    if (d < 0.0)
      return (PN) potion_real(P, -d);
    else
      return self;
  }
  return PN_NUM(labs(PN_INT(self)));
}
/**\memberof PNNumber
  "cmp" a number to a value. casts argument n to a number
                     \code 1 cmp 2 #=> -1 \endcode
                     \code 1 cmp 1 #=>  0 \endcode
                     \code 1 cmp 0 #=>  1 \endcode
 \param n PN number compared to
 \return PNNumber -1, 0 or 1
 \sa potion_tuple_sort. */
static PN potion_num_cmp(Potion *P, PN cl, PN self, PN n) {
  if (PN_IS_DECIMAL(self)) {
    double d1 = PN_DBL(self);
    double d2 = PN_DBL(potion_send(PN_number, n));
    return d1 < d2 ? PN_NUM(-1) : d1 == d2 ? PN_NUM(0) : PN_NUM(1);
  } else {
    long n1, n2;
    n1 = PN_INT(self);
    n2 = PN_IS_NUM(n) ? PN_INT(n) : PN_INT(potion_send(PN_number, n));
    return n1 < n2 ? PN_NUM(-1) : n1 == n2 ? PN_NUM(0) : PN_NUM(1);
  }
}

void potion_num_init(Potion *P) {
  PN num_vt = PN_VTABLE(PN_TNUMBER);
#if defined(P2)
  PN dbl_vt = PN_VTABLE(PN_TDECIMAL);
#endif
  potion_method(num_vt, "+", potion_add, "value=N");
  potion_method(num_vt, "-", potion_sub, "value=N");
  potion_method(num_vt, "*", potion_mult, "value=N");
  potion_method(num_vt, "/", potion_div, "value=N");
  potion_method(num_vt, "%", potion_rem, "value=N");
  potion_method(num_vt, "~", potion_bitn, 0);
  potion_method(num_vt, "<<", potion_bitl, "value=N");
  potion_method(num_vt, ">>", potion_bitr, "value=N");
  potion_method(num_vt, "**", potion_pow, "value=N");
  potion_method(num_vt, "number", potion_num_number, 0);
  potion_method(num_vt, "sqrt", potion_sqrt, 0);
  potion_method(num_vt, "step", potion_num_step, "end=N,step=N,block=&");
  potion_method(num_vt, "string", potion_num_string, 0);
  potion_method(num_vt, "times", potion_num_times, "block=&");
  potion_method(num_vt, "to", potion_num_to, "end=N,block=&");
  potion_method(num_vt, "chr", potion_num_chr, 0);
  potion_method(num_vt, "integer?", potion_num_is_integer, 0);
  potion_method(num_vt, "float?", potion_num_is_float, 0);
  potion_method(num_vt, "integer", potion_num_integer, 0);
  potion_method(num_vt, "abs", potion_abs, 0);
  potion_method(num_vt, "cmp", potion_num_cmp, "value=o");
  potion_method(num_vt, "rand", potion_num_rand, 0);
#if defined(P2)
  potion_method(dbl_vt, "string", potion_num_string, 0);
#endif
}
