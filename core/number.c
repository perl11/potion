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

static PN potion_num_inspect(Potion *P, PN closure, PN self) {
  PN str;
  if (PN_IS_NUM(self)) {
    char ints[21];
    sprintf(ints, "%ld", PN_INT(self));
    str = potion_byte_str(P, ints);
  } else {
    struct PNDecimal *n = (struct PNDecimal *)self;
    char *ints = PN_ALLOC_N(char, n->len + 2);
    int i, prec;
    for (prec = 0; prec < PN_PREC; prec++)
      if (n->digits[n->len - prec] != 0)
        break;
    prec--;
    for (i = 0; i < n->len - prec; i++) {
      int dot = (i >= n->len - PN_PREC);
      if (i == n->len - PN_PREC)
        sprintf(ints + i, ".");
      sprintf(ints + i + dot, "%d", (int)n->digits[i]);
    }
    str = potion_byte_str(P, ints);
    PN_FREE(ints);
  }
  return str;
}

PN potion_decimal(Potion *P, int len, int intg, char *str) {
  int i, rlen = intg + PN_PREC;
  struct PNDecimal *n = PN_OBJ_ALLOC(struct PNDecimal, PN_TNUMBER, sizeof(PN) * rlen);

  n->sign = (str[0] == '-');
  n->len = rlen;
  for (i = 0; i < rlen; i++) {
    int x = i;
    if (x > intg) x++;
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
  potion_method(num_vt, "inspect", potion_num_inspect, 0);
}
