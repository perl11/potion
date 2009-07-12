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

static double mpbbx = 0.0, mpbdx, mpbx2, mprbx, mprdx, mprx2, mprxx;
const int mpnbt = 48, mpnpr = 16, mpmcrx = 7, mpnrow = 16, mpnsp1 = 3, mpnsp2 = 9;

double potion_num_double(PN num) {
  if (PN_IS_NUM(num))
    return (double)PN_INT(num);
  if (!PN_IS_DECIMAL(num))
    return 0.0;
  vPN(Decimal) n = (struct PNDecimal *)num;
  if (n->sign == 0)
    return 0.0;
  if (n->real[0] >= 22.0)
    return (n->sign > 0) ? HUGE_VAL : -HUGE_VAL;
  if (n->real[0] <= -24.0)
    return (n->sign > 0) ? 0.0 : -0.0;

  int na = n->sign;
  double da = PN_MANTISSA(n, 0);
  if (na == 2)
    da += PN_MANTISSA(n, 1) * mprdx;
  else if (na >= 3)
    da += (PN_MANTISSA(n, 1) * mprdx + PN_MANTISSA(n, 2) * mprx2);

  if (n->real[0] == -23.0) {
    da *= mprdx;
    da *= ldexp(1.0, -mpnbt * 22);
  } else
    da *= ldexp(1.0, mpnbt * (int)n->real[0]);
  return (n->sign > 0) ? da : -da;
}

PN potion_pow(Potion *P, PN cl, PN num, PN sup) {
  return PN_NUM((int)pow((double)PN_INT(num), (double)PN_INT(sup)));
}

static PN potion_add(Potion *P, PN closure, PN self, PN num) {
  return PN_NUM(PN_INT(self) + PN_INT(num));
}

static PN potion_sub(Potion *P, PN closure, PN self, PN num) {
  return PN_NUM(PN_INT(self) - PN_INT(num));
}

static PN potion_mult(Potion *P, PN closure, PN self, PN num) {
  return PN_NUM(PN_INT(self) * PN_INT(num));
}

static PN potion_div(Potion *P, PN closure, PN self, PN num) {
  return PN_NUM(PN_INT(self) / PN_INT(num));
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
  char ints[21];
  if (PN_IS_NUM(self))
    sprintf(ints, "%ld", PN_INT(self));
  else
    sprintf(ints, "%f", potion_num_double(self));
  return potion_byte_str(P, ints);
}

static PN potion_num_times(Potion *P, PN cl, PN self, PN block) {
  int i, j = PN_INT(self);
  for (i = 0; i < j; i++)
    PN_CLOSURE(block)->method(P, block, self, PN_NUM(i));
  return PN_NUM(i);
}

static PN potion_num_to(Potion *P, PN cl, PN self, PN end, PN block) {
  int i, s = 1, j = PN_INT(self), k = PN_INT(end);
  if (k < j) s = -1;
  for (i = j; i != k + s; i += s)
    PN_CLOSURE(block)->method(P, block, self, PN_NUM(i));
  return PN_NUM(abs(i - j));
}

PN potion_decimal(Potion *P, int len, int intg, char *str) {
  char *ptr;
  const unsigned int digits_per_step = 6;
  const int factor_per_step = 1000000;
  double v;
  int i = 0, neg = 0, rlen = ((len - 1) / digits_per_step) + 3;
  vPN(Decimal) n = PN_ALLOC_N(PN_TNUMBER, struct PNDecimal, sizeof(double) * rlen);

  if (str[0] == '-') {
    i++;
    neg = 1;
  }

  ptr = str + i + min(len - i, digits_per_step);
  n->sign = 1;
  n->real[0] = 0.0;
  n->real[1] = strtod(str + i, &ptr);
  i += digits_per_step;

  while (i < len)
  {
    int n = min(len - i, digits_per_step);
    ptr = str + i + n;
    v = strtod(str + i, &ptr);  
    if (n < digits_per_step) {
      int j, f = 1;
      for (j = 0; j < n; j++) f *= 10;
      // mpmuld(n, (double)f, 0, n, prec_words);
    } else {
      // mpmuld(n, (double)factor_per_step, 0, n, prec_words);
    }
    // mpadd(n, mp_real(v, 8), n, prec_words);
    i += digits_per_step;
  }

  if (neg) n->sign = -n->sign;
  // TODO: allow exponent in string

  return (PN)n;
}

void potion_num_init(Potion *P) {
  if (mpbbx == 0.0) {
    mpbbx = 16777216.0;
    mpbdx = mpbbx * mpbbx;
    mpbx2 = mpbdx * mpbdx; 
    mprbx = 1.0 / mpbbx;
    mprdx = 1.0 / mpbdx;
    mprx2 = mprdx * mprdx;
    mprxx = 16.0 * mprx2;
  }

  PN num_vt = PN_VTABLE(PN_TNUMBER);
  potion_method(num_vt, "+", potion_add,  "value=N");
  potion_method(num_vt, "-", potion_sub,  "value=N");
  potion_method(num_vt, "*", potion_mult, "value=N");
  potion_method(num_vt, "/", potion_div,  "value=N");
  potion_method(num_vt, "number", potion_num_number, 0);
  potion_method(num_vt, "step", potion_num_step, "end=N,step=N");
  potion_method(num_vt, "string", potion_num_string, 0);
  potion_method(num_vt, "times", potion_num_times, "block=&");
  potion_method(num_vt, "to", potion_num_to, "end=N");
}
