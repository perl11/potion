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

static PN potion_num_string(Potion *P, PN closure, PN self) {
  PN str;
  if (PN_IS_NUM(self)) {
    char ints[21];
    sprintf(ints, "%ld", PN_INT(self));
    str = potion_byte_str(P, ints);
  } else {
    vPN(Decimal) n = (struct PNDecimal *)self;
    char ints[n->len + 2];
    int i, prec;
    for (prec = 1; prec < PN_PREC; prec++)
      if (n->digits[n->len - prec] != 0)
        break;
    if (prec > 1) prec--;
    for (i = 0; i < n->len - prec; i++) {
      int dot = (i >= n->len - PN_PREC);
      if (i == n->len - PN_PREC)
        sprintf(ints + i, ".");
      sprintf(ints + i + dot, "%d", (int)n->digits[i]);
    }
    ints[i+1] = '\0';
    str = potion_byte_str(P, ints);
  }
  return str;
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
  int i, rlen = intg + PN_PREC;
  vPN(Decimal) n = PN_ALLOC_N(PN_TNUMBER, struct PNDecimal, sizeof(PN) * rlen);

  n->sign = (str[0] == '-');
  n->len = rlen;
  for (i = 0; i < rlen; i++) {
    int x = i;
    if (x >= intg) x++;
    if (x > len || str[x] < '0' || str[x] > '9')
      n->digits[i] = 0;
    else
      n->digits[i] = str[x] - '0';
  }

  return (PN)n;
}

void potion_num_init(Potion *P) {
  PN num_vt = PN_VTABLE(PN_TNUMBER);
  potion_method(num_vt, "+", potion_add,  "value=N");
  potion_method(num_vt, "-", potion_sub,  "value=N");
  potion_method(num_vt, "*", potion_mult, "value=N");
  potion_method(num_vt, "/", potion_div,  "value=N");
  potion_method(num_vt, "number", potion_num_number, 0);
  potion_method(num_vt, "step", potion_num_step, "end=N,step=N");
  potion_method(num_vt, "string", potion_num_string, 0);
  potion_method(num_vt, "times", potion_num_times, 0);
  potion_method(num_vt, "to", potion_num_to, "end=N");
}
