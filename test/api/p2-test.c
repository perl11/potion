//
// p2-test.c
// tests of the p2 C api (perl5)
//
// (c) 2008 why the lucky stiff, the freelance professor
// (c) 2012-2013 by perl11 org
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "p2.h"
//#include "potion.h"
//#include "internal.h"
#include "CuTest.h"

PN num = PN_NUM(490);
Potion *P;

// TODO p2 perl5 api

void p2_test_undef(CuTest *T) {
  CuAssert(T, "undef isn't a nil type", PN_TYPE(PN_NIL) == PN_TNIL);
  CuAssert(T, "undef is a ref", !PN_IS_PTR(PN_NIL));
  CuAssert(T, "defined undef is not false",
    PN_FALSE == potion_send(PN_NIL, potion_str(P, "defined")));
#if 0 // todo: any->defined
  CuAssert(T, "defined num is not true",
    PN_TRUE == potion_send(num, potion_str(P, "defined")));
#endif
}

void p2_test_int1(CuTest *T) {
  PN zero = PN_NUM(0);
  CuAssert(T, "zero isn't zero", PN_INT(zero) == 0);
  CuAssert(T, "zero isn't a number", PN_IS_NUM(zero));
  CuAssert(T, "zero is a ref", !PN_IS_PTR(zero));
  CuAssert(T, "zero bad add",
    490 == PN_INT(potion_send(zero, potion_str(P, "+"), num)));
}

void p2_test_int2(CuTest *T) {
  PN pos = PN_NUM(10891);
  CuAssert(T, "positive numbers invalid", PN_INT(pos) == 10891);
  CuAssert(T, "positive not a number", PN_IS_NUM(pos));
  CuAssert(T, "positive is a ref", !PN_IS_PTR(pos));
  CuAssert(T, "positive bad add",
    11381 == PN_INT(potion_send(pos, potion_str(P, "+"), num)));
}

void p2_test_int3(CuTest *T) {
  PN neg = PN_NUM(-4343);
  CuAssert(T, "negative numbers invalid", PN_INT(neg) == -4343);
  CuAssert(T, "negative not a number", PN_IS_NUM(neg));
  CuAssert(T, "negative is a ref", !PN_IS_PTR(neg));
  CuAssert(T, "negative bad add",
    -3853 == PN_INT(potion_send(neg, potion_str(P, "+"), num)));
}

void p2_test_decimal(CuTest *T) {
  PN dec = potion_decimal(P, "14466", 5);
  CuAssert(T, "decimal not a number", PN_TYPE(dec) == PN_TNUMBER);
}

void p2_test_str(CuTest *T) {
  CuAssert(T, "string isn't a string", PN_IS_STR(PN_string));
  CuAssert(T, "string isn't a ref", PN_IS_PTR(PN_string));
  CuAssert(T, "string length isn't working",
    6 == PN_INT(potion_send(PN_string, potion_str(P, "length"))));
}

void p2_test_empty(CuTest *T) {
  PN empty = PN_TUP0();
  CuAssert(T, "empty isn't a tuple", PN_IS_TUPLE(empty));
  CuAssert(T, "empty isn't a ref", PN_IS_PTR(empty));
  CuAssertIntEquals(T, "tuple length is off",
    PN_INT(potion_send(empty, potion_str(P, "length"))), 0);
}

void p2_test_tuple(CuTest *T) {
  PN tup = potion_tuple_with_size(P, 3);
  PN_TUPLE_AT(tup, 0) = PN_NIL;
  PN_TUPLE_AT(tup, 1) = PN_string;
  PN_TUPLE_AT(tup, 2) = tup;
  CuAssert(T, "tuple isn't a tuple", PN_IS_TUPLE(tup));
  CuAssert(T, "tuple isn't a ref", PN_IS_PTR(tup));
  CuAssertIntEquals(T, "tuple length is off",
    PN_INT(potion_send(tup, potion_str(P, "length"))), 3);
}

void p2_test_sig(CuTest *T) {
  PN sig = p2_sig(P, "Num $num1, Num $num2");
  CuAssert(T, "signature Num $num1, Num $num2 isn't a tuple", PN_IS_TUPLE(sig));

  sig = p2_sig(P, "$x,$y|$r");
  CuAssert(T, "signature $x,$y|$r isn't a tuple", PN_IS_TUPLE(sig));
}

void p2_test_eval(CuTest *T) {
  PN add = p2_eval(P, potion_str(P, "sub($x,$y){$x+$y}"));
  PN_F addfn = PN_CLOSURE_F(add); // TODO FAIL
  PN num = addfn(P, add, 0, PN_NUM(3), PN_NUM(5));
  CuAssertIntEquals(T, "calling closure as c func", 8, PN_INT(num));
}

void p2_test_allocated(CuTest *T) {
  void *scanptr = (void *)((char *)P->mem->birth_lo + PN_ALIGN(sizeof(struct PNMemory), 8));
  while ((PN)scanptr < (PN)P->mem->birth_cur) {
    if (((struct PNFwd *)scanptr)->fwd != POTION_FWD && ((struct PNFwd *)scanptr)->fwd != POTION_COPIED) {
      CuAssert(T, "wrong type for allocated object", ((struct PNObject *)scanptr)->vt <= PN_TUSER);
    }
    scanptr = (void *)((char *)scanptr + potion_type_size(P, scanptr));
    CuAssert(T, "allocated object goes beyond GC pointer", (PN)scanptr <= (PN)P->mem->birth_cur);
  }
}

CuSuite *p2_suite() {
  CuSuite *S = CuSuiteNew();
  //SUITE_ADD_TEST(S, p2_test_eval);
  SUITE_ADD_TEST(S, p2_test_undef);
  //SUITE_ADD_TEST(S, p2_test_bool);
  SUITE_ADD_TEST(S, p2_test_int1);
  SUITE_ADD_TEST(S, p2_test_int2);
  SUITE_ADD_TEST(S, p2_test_int3);
  SUITE_ADD_TEST(S, p2_test_decimal);
  SUITE_ADD_TEST(S, p2_test_str);
  SUITE_ADD_TEST(S, p2_test_empty);
  SUITE_ADD_TEST(S, p2_test_tuple);
  SUITE_ADD_TEST(S, p2_test_sig);
  SUITE_ADD_TEST(S, p2_test_allocated);
  return S;
}

int main(void) {
  POTION_INIT_STACK(sp);
  int count;
  P = potion_create(sp);
  CuString *out = CuStringNew();
  CuSuite *suite = p2_suite();
  CuSuiteRun(suite);
  CuSuiteSummary(suite, out);
  CuSuiteDetails(suite, out);
  printf("%s\n", out->buffer);
  count = suite->failCount;
  CuSuiteFree(suite);
  CuStringFree(out);
  return count;
}
