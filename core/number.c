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

#define TWO_TO_THE_52 4503599627370496.0

inline double FLOOR_POSITIVE(double a) {
  double b = (a + TWO_TO_THE_52) - TWO_TO_THE_52;
  if (b>a) return b-1.0; else return b;
}

inline double AINT(double a) {
  double b = a;
  if (a>=0) {
    b = (b + TWO_TO_THE_52) - TWO_TO_THE_52;
    if(b>a) return b-1.0; else return b;
  }
  else {
    b = (b - TWO_TO_THE_52) + TWO_TO_THE_52;
    if(b<a) return b+1.0; else return b;
  }
}

inline double mp_two_prod_positive(double a, double b, double *err) {
  double p = a * b;
  *err = a*b-p;
  return p;
}

static inline vPN(Decimal) potion_dec_alloc(Potion *P, int rlen) {
  vPN(Decimal) dec = PN_ALLOC_N(PN_TNUMBER, struct PNDecimal, sizeof(double) * rlen);
  dec->len = rlen;
  return dec;
}

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

vPN(Decimal) potion_real(Potion *P, double num) {
  int i, k, n1, n2;
  double aa;
  vPN(Decimal) a;
  
  if (num == 0.0)
    return potion_dec_alloc(P, 0);
  
  a = potion_dec_alloc(P, 6);
  n1 = n2 = 0;
  aa = fabs(num);

  if (aa >= mpbdx) {
    for (k = 1; k <= 100; ++k) {
      aa = mprdx * aa;
      if (aa < mpbdx) {
        n1 = n1 + k;
        break;
      }
    }
  } else if (aa < 1.0) {
    for (k = 1; k <= 100; ++k) {
      aa = mpbdx * aa;
      if (aa >= 1.0) {
        n1 = n1 - k;
        break;
      }
    } 
  }
  
  a->real[0] = n1;
  PN_MANTISSA(a, 0) = FLOOR_POSITIVE(aa);
  aa = mpbdx * (aa - PN_MANTISSA(a, 0));
  PN_MANTISSA(a, 1) = FLOOR_POSITIVE(aa);
  aa = mpbdx * (aa - PN_MANTISSA(a, 1));
  PN_MANTISSA(a, 2) = FLOOR_POSITIVE(aa);
  PN_MANTISSA(a, 3) = 0.;
  PN_MANTISSA(a, 4) = 0.;
    
  for (i = 2; i >= 0; --i)
    if (PN_MANTISSA(a, i) != 0.) break;
    
  a->sign = num >= 0 ? i : -i;
  return a;
}

vPN(Decimal) potion_dec_norm(Potion *P, vPN(Decimal) a) {
  double a2, t1, t2, t3;
  int i, ia, na, nd, n4;
  vPN(Decimal) c;

  ia = a->sign >= 0 ? 1 : -1;
  nd = abs((int)a->sign);
  na = min(nd, P->prec);
  n4 = na + 2; 
  a2 = a->real[0];
  c = potion_dec_alloc(P, n4);

  t1 = 0.0;
  for (i = n4 - 1; i >= 0; --i) {
    t3 = t1 + PN_MANTISSA(a, i);
    t2 = t3 * mprdx;
    t1 = (int)t2;
    if (t2 < 0.0 && t1 != t2)
      t1 -= 1.0;
    PN_MANTISSA(c, i) = t3 - t1 * mpbdx;
  }
  c->real[0] = t1;

  if (c->real[0] < 0.0) {
    ia = -ia;
    
    for (i = 0; i <= n4; ++i)
      c->real[i] = - c->real[i];
    for (i = n4 - 1; i >= 0; --i) {
      if (PN_MANTISSA(c, i) < 0) {
        PN_MANTISSA(c, i) = mpbdx + PN_MANTISSA(c, i);
        PN_MANTISSA(c, i-1) -= 1.0;
      }
    }
  }

  if (a->real[0] > 0.0) {
    if (na != P->prec && na < (int)c->len - 3) {
      for (i = n4; i >= 0; --i) PN_MANTISSA(c, i) = PN_MANTISSA(c, i-1);
      na = min(na+1, P->prec);
      a2 += 1;
    } else {
      for (i = n4 - 1; i >= 0; --i) PN_MANTISSA(c, i) = PN_MANTISSA(c, i-1);
      a2 += 1;
    }
  }

  c->sign = ia >= 0 ? na : -na;
  c->real[0] = a2;

  // mproun(c);
  return c;
}

vPN(Decimal) potion_dec_add(Potion *P, vPN(Decimal) a, vPN(Decimal) b) {
  double db;
  vPN(Decimal) d;
  int i, ia, ib, ish, ixa, ixb, ixd, na, nb;
  int m1, m2, m3, m4, m5, nsh, nd;

  ia = a->sign >= 0 ? 1 : -1;
  ib = b->sign >= 0 ? 1 : -1;
  na = min(abs(a->sign), P->prec);
  nb = min(abs(b->sign), P->prec);

  if (na == 0)
    return b;
  else if (nb == 0)
    return a;

  d = potion_dec_alloc(P, min(max(na, nb), P->prec) + 2);

  if (ia == ib) db = 1.0;
  else db = -1.0;

  ixa = (int)a->real[0];
  ixb = (int)b->real[0];
  ish = ixa - ixb;

  d->sign = 0;
  d->real[0] = 0.0;

  if (ish >= 0) {
    m1 = min(na, ish);
    m2 = min(na, nb + ish);
    m3 = na;
    m4 = min(max(na, ish), P->prec + 1);
    m5 = min(max(na, nb + ish), P->prec + 1);
    
    d->sign = a->sign;
    d->real[0] = a->real[0];
    for (i = 0; i < m1; ++i)
      PN_MANTISSA(d, i) = PN_MANTISSA(a, i);
    
    if(db > 0) {
      for (i = m1; i < m2; ++i)
        PN_MANTISSA(d, i) = PN_MANTISSA(a, i) + PN_MANTISSA(b, i-ish);
      
      for (i = m2; i < m3; ++i)
        PN_MANTISSA(d, i) = PN_MANTISSA(a, i);
    
      for (i = m3; i < m4; ++i)
        PN_MANTISSA(d, i) = 0.0;
      
      for (i = m4; i < m5; ++i)
        PN_MANTISSA(d, i) = PN_MANTISSA(b, i-ish);
    } else {
      for (i = m1; i < m2; ++i)
        PN_MANTISSA(d, i) = PN_MANTISSA(a, i) - PN_MANTISSA(b, i-ish);
      
      for (i = m2; i < m3; ++i)
        PN_MANTISSA(d, i) = PN_MANTISSA(a, i);
    
      for (i = m3; i < m4; ++i)
        PN_MANTISSA(d, i) = 0.0;
      
      for (i = m4; i < m5; ++i)
        PN_MANTISSA(d, i) = - PN_MANTISSA(b, i-ish);
    }
    ixd = ixa;

  } else {
    
    nsh = -ish;
    m1 = min(nb, nsh);
    m2 = min(nb, na + nsh);
    m3 = nb;
    m4 = min(max(nb, nsh), P->prec + 1);
    m5 = min(max(nb, na + nsh), P->prec + 1);
    
    if(db > 0) {
      for (i = 0; i < m1; ++i)
        PN_MANTISSA(d, i) = PN_MANTISSA(b, i);
      
      for (i = m1; i < m2; ++i)
        PN_MANTISSA(d, i) = PN_MANTISSA(a, i-nsh) + PN_MANTISSA(b, i);
      
      for (i = m2; i < m3; ++i)
        PN_MANTISSA(d, i) = PN_MANTISSA(b, i);

    } else {
      for (i = 0; i < m1; ++i)
        PN_MANTISSA(d, i) = - PN_MANTISSA(b, i);
      
      for (i = m1; i < m2; ++i)
        PN_MANTISSA(d, i) = PN_MANTISSA(a, i-nsh) - PN_MANTISSA(b, i);
      
      for (i = m2; i < m3; ++i)
        PN_MANTISSA(d, i) = - PN_MANTISSA(b, i);
    }

    for (i = m3; i < m4; ++i)
      PN_MANTISSA(d, i) = 0.0;
    
    for (i = m4; i < m5; ++i)
      PN_MANTISSA(d, i) = PN_MANTISSA(a, i-nsh);
    
    ixd = ixb;
  }
  
  nd = m5;
  PN_MANTISSA(d, nd) = 0.0;
  PN_MANTISSA(d, nd + 1) = 0.0;

  d->sign = ia >= 0 ? nd : -nd;
  d->real[0] = ixd;
  return potion_dec_norm(P, d);
}

vPN(Decimal) potion_dec_sub(Potion *P, vPN(Decimal) a, vPN(Decimal) b) {
  vPN(Decimal) c;
  int i, b1, BreakLoop;

  if (a == b)
    return potion_dec_alloc(P, 0);
  
  if (a->sign == b->sign) {
    BreakLoop = 0;
    for (i = 0; i < abs(a->sign) + 1; ++i) {
      if (a->real[i] != b->real[i]) {
        BreakLoop = 1;
        break;
      }
    }
    if (!BreakLoop)
      return potion_dec_alloc(P, 0);
  }
  
  b1 = b->sign;
  b->sign = -b->sign;

  c = potion_dec_add(P, a, b);
  if (b != c)
    b->sign = b1;

  return c;
}

vPN(Decimal) potion_dec_mult(Potion *P, vPN(Decimal) a, vPN(Decimal) b) {
  int i, j, jd, ia, ib, na, nb, nc, n2;
  double d2, t1, t2, t[2], a_val;
  vPN(Decimal) d;
  
  ia = a->sign >= 0 ? 1 : -1;
  ib = b->sign >= 0 ? 1 : -1;
  na = min(abs(a->sign), P->prec);
  nb = min(abs(b->sign), P->prec);

  if (na == 0 || nb == 0)
    return potion_dec_alloc(P, 0);

  if (na == 1) {
    if (PN_MANTISSA(a, 0) == 1.) {
      if (b->sign >= 0)
        return b;
      d = potion_dec_alloc(P, nb + 2);
      d->sign = ia * ib >= 0 ? nb : -nb;
      d->real[0] = a->real[0] + b->real[0];
      
      for (i = 0; i < nb; ++i) PN_MANTISSA(d, i) = PN_MANTISSA(b, i);
      PN_MANTISSA(d, nb) = PN_MANTISSA(d, nb + 1) = 0.0;
      return d;
    }
  }
  if (nb == 1) {
    if(PN_MANTISSA(b, 0) == 1.) {
      if (a->sign >= 0)
        return a;
      d = potion_dec_alloc(P, na + 2);
      d->sign = ia * ib >= 0 ? na : -na;
      d->real[0] = a->real[0] + b->real[0];
      
      for (i = 0; i < na; ++i) PN_MANTISSA(d, i) = PN_MANTISSA(a, i);
      PN_MANTISSA(d, na) = PN_MANTISSA(d, na + 1) = 0.0;
      return d;
    }
  }

  nc = min(na + nb, P->prec);
  d = potion_dec_alloc(P, nc + 1);
  d2 = a->real[0] + b->real[0];
  for (i = 0; i < nc + 1; ++i) d->real[i] = 0.0;

  for (j = 0; j < na; ++j) {
    a_val = PN_MANTISSA(a, j);
    n2 = min(nb, P->prec + 4 - j);
    
    for (i = 0, jd = j; i < n2; ++i, ++jd) {
      t[0] = mp_two_prod_positive(a_val, PN_MANTISSA(b, i), &t[1]); 
      PN_MANTISSA(d, jd-1) += t[0];
      PN_MANTISSA(d, jd) += t[1];
    }

    if (!((j+1) & (mpnpr-1))) {
      for (i = jd - 1; i >= j; i--) {
        t1 = PN_MANTISSA(d, i);
        t2 = (int)(t1 * mprdx);
        PN_MANTISSA(d, i) = t1 - t2 * mpbdx;
        PN_MANTISSA(d, i-1) += t2;
      }
    }
  }

  if (d->real[0] != 0.0 || (PN_MANTISSA(d, 0) >= mpbdx && (d->real[0] = 0.0, 1))) {
    for (i = nc - 1; i >= 0; --i) PN_MANTISSA(d, i) = d->real[i];
    nc--;
  }

  d->sign = ia + ib ? nc : -nc;
  d->real[0] = d2;

  return potion_dec_norm(P, d);
}

/*
vPN(Decimal) potion_dec_div(Potion *P, vPN(Decimal) a, vPN(Decimal) b) {
  int i, ia, ib, ij, is, i2, i3=0, j, j3, na, nb, nc, BreakLoop;
  double rb, ss, t0, t1, t2, t[2];
  vPN(Decimal) d;
  
  ia = a->sign >= 0 ? 1 : -1;
  ib = b->sign >= 0 ? 1 : -1;
  na = min(abs(a->sign), P->prec);
  nb = min(abs(b->sign), P->prec);
  
  if (na == 0)
    return potion_dec_alloc(P, 0);
  
  if (nb == 1 && PN_MANTISSA(b, 0) == 1.) {
    if (a->sign >= 0)
      return a;
    d = potion_dec_alloc(P, na + 2);
    d->sign = ia * ib >= 0 ? na : -na;
    d->real[0] = a->real[0] - b->real[0];
    
    for (i = 0; i < na; ++i) PN_MANTISSA(d, i) = PN_MANTISSA(a, i);
    return d;
  }
  
  if (nb == 0) {
    // TODO: divide by zero error
    return potion_dec_alloc(P, 0);
  }
  
  d = potion_dec_alloc(P, P->prec);

  t0 = mpbdx * PN_MANTISSA(b, 0);
  if (nb >= 2) t0 = t0 + PN_MANTISSA(b, 1);
  if (nb >= 3) t0 = t0 + mprdx * PN_MANTISSA(b, 2);
  rb = 1.0 / t0;
  
  for (i = 0; i < na; ++i) d->real[i] = a->real[i+1];

  for (j = 0; j <= na-1; ++j) {
    t1 = mpbx2 * PN_MANTISSA(d, j-1) + mpbdx *
      PN_MANTISSA(d, j) + PN_MANTISSA(d, j+1);
    t0 = AINT(rb * t1);
    j3 = j - 1;
    i2 = min(nb, P->prec + 2 - j3) + 2;
    ij = i2 + j3;
    for (i = 3; i <= i2; ++i) {
      i3 = i + j3;
      t[0] = mp_two_prod(t0, b[i], t[1]);
      d[i3-1] -= t[0];   // >= -(2^mpnbt-1), <= 2^mpnbt-1
      d[i3] -= t[1];
    }
    
      // Release carry to avoid overflowing the exact integer capacity
      // (2^52-1) of a floating point word in D.
    if(!(j & (mp::mpnpr-1))) { // assume mpnpr is power of two
      t2 = 0.0;
      for(i=i3;i>j+1;i--) {
        t1 = t2 + d[i];
        t2 = int (t1 * mprdx);     // carry <= 1
        d[i] = t1 - t2 * mpbdx;   // remainder of t1 * 2^(-mpnbt)
      }
      d[i] += t2;
    }
    
    d[j] += mpbdx * d[j-1];
    d[j-1] = t0; // quotient
  }
  
  // Compute additional words of the quotient, as long as the remainder
  // is nonzero.  
  BreakLoop = 0;
  for (j = na+2; j <= prec_words+3; ++j) {
    t1 = mpbx2 * d[j-1] + mpbdx * d[j];
    if (j < prec_words + 3) t1 += d[j+1];
    t0 = AINT (rb * t1); // trial quotient, approx is ok.
    j3 = j - 3;
    i2 = std::min (nb, prec_words + 2 - j3) + 2;
    ij = i2 + j3;
    ss = 0.0;
    
    for (i = 3; i <= i2; ++i) {
      i3 = i + j3;
      t[0] = mp_two_prod(t0, b[i], t[1]);
      d[i3-1] -= t[0];   // >= -(2^mpnbt-1), <= 2^mpnbt-1
      d[i3] -= t[1];
      
      //square to avoid cancellation when d[i3] or d[i3-1] are negative
      ss += sqr (d[i3-1]) + sqr (d[i3]); 
    }
      // Release carry to avoid overflowing the exact integer capacity
      // (2^mpnbt-1) of a floating point word in D.
    if(!(j & (mp::mpnpr-1))) { // assume mpnpr is power of two
      t2 = 0.0;
      for(i=i3;i>j+1;i--) {
        t1 = t2 + d[i];
        t2 = int (t1 * mprdx);     // carry <= 1
        d[i] = t1 - t2 * mpbdx;   // remainder of t1 * 2^(-mpnbt)
      }
      d[i] += t2;
    }

    d[j] += mpbdx * d[j-1];
    d[j-1] = t0;
    if (ss == 0.0) {
      BreakLoop = 1;
      break;
    }
    if (ij <= prec_words+1) d[ij+3] = 0.0;
  } 

  // Set sign and exponent, and fix up result.
  if(!BreakLoop) j--;
  d[j] = 0.0;
  
  if (d[1] == 0.0) {
    is = 1;
    d--; d_add++;
  } else {
    is = 2;
    d-=2; d_add+=2;
    //for (i = j+1; i >= 3; --i) d[i] =  d[i-2];
  }

  nc = std::min( (int(c[0])-FST_M-2), std::min (j-1, prec_words));
  

  d[1] = ia+ib ? nc : -nc;//sign(nc, ia * ib);
  d[2] = a[2] - b[2] + is - 2;  
  
  mpnorm(d, c, prec_words);
  delete [] (d+d_add);
  
  if (debug_level >= 8) print_mpreal("MPDIV O ", c);
  return;
}
*/

PN potion_decimal(Potion *P, int len, int intg, char *str) {
  char *ptr;
  const unsigned int digits_per_step = 6;
  const int factor_per_step = 1000000;
  double v;
  int i = 0, neg = 0, rlen = ((len - 1) / digits_per_step) + 3;
  vPN(Decimal) n = potion_dec_alloc(P, rlen);

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
    int nlen = min(len - i, digits_per_step);
    ptr = str + i + nlen;
    v = strtod(str + i, &ptr);  
    if (nlen < digits_per_step) {
      int j, f = 1;
      for (j = 0; j < nlen; j++) f *= 10;
      n = potion_dec_mult(P, n, potion_real(P, (double)f));
    } else
      n = potion_dec_mult(P, n, potion_real(P, (double)factor_per_step));
    n = potion_dec_add(P, n, potion_real(P, v));
    i += digits_per_step;
  }

  if (neg) n->sign = -n->sign;
  // TODO: allow exponent in string

  return (PN)n;
}

PN potion_pow(Potion *P, PN cl, PN num, PN sup) {
  return PN_NUM((int)pow((double)PN_INT(num), (double)PN_INT(sup)));
}

#define PN_NUM_MATH(int_math, dec_math) \
  if (PN_IS_NUM(self) && PN_IS_NUM(num)) \
    return PN_NUM(PN_INT(self) int_math PN_INT(num)); \
  if (PN_IS_NUM(self)) \
    self = (PN)potion_real(P, (double)PN_INT(self)); \
  if (PN_IS_NUM(num)) \
    num = (PN)potion_real(P, (double)PN_INT(num)); \
  return (PN)dec_math(P, (struct PNDecimal *)self, (struct PNDecimal *)num);

static PN potion_add(Potion *P, PN closure, PN self, PN num) {
  PN_NUM_MATH(+, potion_dec_add);
}

static PN potion_sub(Potion *P, PN closure, PN self, PN num) {
  PN_NUM_MATH(-, potion_dec_sub);
}

static PN potion_mult(Potion *P, PN closure, PN self, PN num) {
  PN_NUM_MATH(*, potion_dec_mult);
}

static PN potion_div(Potion *P, PN closure, PN self, PN num) {
  if (PN_IS_NUM(self) && PN_IS_NUM(num))
    return PN_NUM(PN_INT(self) / PN_INT(num));
  return (PN)potion_real(P, 0.0);
}

static PN potion_rem(Potion *P, PN closure, PN self, PN num) {
  if (PN_IS_NUM(self) && PN_IS_NUM(num))
    return PN_NUM(PN_INT(self) % PN_INT(num));
  return (PN)potion_real(P, 0.0);
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
  potion_method(num_vt, "+", potion_add, "value=N");
  potion_method(num_vt, "-", potion_sub, "value=N");
  potion_method(num_vt, "*", potion_mult, "value=N");
  potion_method(num_vt, "/", potion_div, "value=N");
  potion_method(num_vt, "%", potion_rem, "value=N");
  potion_method(num_vt, "~", potion_bitn, 0);
  potion_method(num_vt, "<<", potion_bitl, "value=N");
  potion_method(num_vt, ">>", potion_bitr, "value=N");
  potion_method(num_vt, "number", potion_num_number, 0);
  potion_method(num_vt, "step", potion_num_step, "end=N,step=N");
  potion_method(num_vt, "string", potion_num_string, 0);
  potion_method(num_vt, "times", potion_num_times, "block=&");
  potion_method(num_vt, "to", potion_num_to, "end=N");
}
