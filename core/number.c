//
// number.c
// simple math
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include "potion.h"
#include "internal.h"

#define PN_DBL(num) (PN_IS_NUM(num) ? (double)PN_INT(num) : ((struct PNDecimal *)num)->value)

PN potion_real(Potion *P, double v) {
  vPN(Decimal) d = PN_ALLOC_N(PN_TNUMBER, struct PNDecimal, 0);
  d->value = v;
  return (PN)d;
}

PN potion_decimal(Potion *P, char *str, int len) {
  char *ptr = str + len;
  return potion_real(P, strtod(str, &ptr));
}

PN potion_pow(Potion *P, PN cl, PN num, PN sup) {
  double x = PN_DBL(num), y = PN_DBL(sup);
  double z = pow(x, y);
  if (PN_IS_NUM(num) && PN_IS_NUM(sup))
    return PN_NUM((int)z);
  return potion_real(P, z);
}

PN potion_sqrt(Potion *P, PN cl, PN num) {
  return potion_real(P, sqrt(PN_DBL(num)));
}

#define PN_NUM_MATH(int_math) \
  if (PN_IS_NUM(self) && PN_IS_NUM(num)) \
    return PN_NUM(PN_INT(self) int_math PN_INT(num)); \
  return potion_real(P, PN_DBL(self) int_math PN_DBL(num));

static PN potion_add(Potion *P, PN closure, PN self, PN num) {
  PN_NUM_MATH(+)
}

static PN potion_sub(Potion *P, PN closure, PN self, PN num) {
  PN_NUM_MATH(-)
}

static PN potion_mult(Potion *P, PN closure, PN self, PN num) {
  PN_NUM_MATH(*)
}

static PN potion_div(Potion *P, PN closure, PN self, PN num) {
  PN_NUM_MATH(/)
}

static PN potion_rem(Potion *P, PN closure, PN self, PN num) {
  if (PN_IS_NUM(self) && PN_IS_NUM(num))
    return PN_NUM(PN_INT(self) % PN_INT(num));
  double x = PN_DBL(self), y = PN_DBL(num);
  int z = (int)(x / y);
  return potion_real(P, x - (y * (double)z));
}

static PN potion_bitn(Potion *P, PN closure, PN self) {
  if (PN_IS_NUM(self))
    return PN_NUM(~PN_INT(self));
  return (PN)potion_real(P, 0.0);
}

static PN potion_bitl(Potion *P, PN closure, PN self, PN num) {
  if (PN_IS_NUM(self) && PN_IS_NUM(num))
    return PN_NUM(PN_INT(self) << PN_INT(num));
  return (PN)potion_real(P, 0.0);
}

static PN potion_bitr(Potion *P, PN closure, PN self, PN num) {
  if (PN_IS_NUM(self) && PN_IS_NUM(num))
    return PN_NUM(PN_INT(self) >> PN_INT(num));
  return (PN)potion_real(P, 0.0);
}

static PN potion_num_number(Potion *P, PN closure, PN self) {
  return self;
}

static PN potion_num_step(Potion *P, PN cl, PN self, PN end, PN step, PN block) {
  int i, j = PN_INT(end), k = PN_INT(step);
  for (i = PN_INT(self); i <= j; i += k) {
    PN_CLOSURE(block)->method(P, block, self, PN_NUM(i));
  }
}

PN potion_num_string(Potion *P, PN closure, PN self) {
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

static PN potion_num_times(Potion *P, PN cl, PN self, PN block) {
  int i, j = PN_INT(self);
  for (i = 0; i < j; i++)
    PN_CLOSURE(block)->method(P, block, self, PN_NUM(i));
  return PN_NUM(i);
}

PN potion_num_to(Potion *P, PN cl, PN self, PN end, PN block) {
  int i, s = 1, j = PN_INT(self), k = PN_INT(end);
  if (k < j) s = -1;
  for (i = j; i != k + s; i += s)
    PN_CLOSURE(block)->method(P, block, self, PN_NUM(i));
  return PN_NUM(abs(i - j));
}

void potion_num_init(Potion *P) {
  PN num_vt = PN_VTABLE(PN_TNUMBER);
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
  potion_method(num_vt, "step", potion_num_step, "end=N,step=N");
  potion_method(num_vt, "string", potion_num_string, 0);
  potion_method(num_vt, "times", potion_num_times, "block=&");
  potion_method(num_vt, "to", potion_num_to, "end=N");
}
