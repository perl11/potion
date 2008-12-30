//
// potion-test.c
// tests of the Potion C api
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "potion.h"
#include "internal.h"
#include "CuTest.h"

PN num = PN_NUM(490);
Potion *P;

void potion_test_nil(CuTest *T) {
  CuAssert(T, "nil isn't a nil type", PN_TYPE(PN_NIL) == PN_TNIL);
  CuAssert(T, "nil is a ref", !potion_is_ref(PN_NIL));
  CuAssert(T, "nil nil? is false",
    PN_TRUE == potion_send(PN_NIL, potion_str(P, "nil?")));
}

void potion_test_bool(CuTest *T) {
  CuAssert(T, "true isn't a bool type", PN_TYPE(PN_TRUE) == PN_TBOOLEAN);
  CuAssert(T, "true is a ref", !potion_is_ref(PN_TRUE));
  CuAssert(T, "false isn't a bool type", PN_TYPE(PN_FALSE) == PN_TBOOLEAN);
  CuAssert(T, "false is a ref", !potion_is_ref(PN_FALSE));
}

void potion_test_int1(CuTest *T) {
  PN zero = PN_NUM(0);
  CuAssert(T, "zero isn't zero", PN_INT(zero) == 0);
  CuAssert(T, "zero isn't a number", PN_IS_NUM(zero));
  CuAssert(T, "zero is a ref", !potion_is_ref(zero));
  CuAssert(T, "zero bad add",
    490 == PN_INT(potion_send(zero, potion_str(P, "+"), num)));
}

void potion_test_int2(CuTest *T) {
  PN pos = PN_NUM(10891);
  CuAssert(T, "positive numbers invalid", PN_INT(pos) == 10891);
  CuAssert(T, "positive not a number", PN_IS_NUM(pos));
  CuAssert(T, "positive is a ref", !potion_is_ref(pos));
  CuAssert(T, "positive bad add",
    11381 == PN_INT(potion_send(pos, potion_str(P, "+"), num)));
}

void potion_test_int3(CuTest *T) {
  PN neg = PN_NUM(-4343);
  CuAssert(T, "negative numbers invalid", PN_INT(neg) == -4343);
  CuAssert(T, "negative not a number", PN_IS_NUM(neg));
  CuAssert(T, "negative is a ref", !potion_is_ref(neg));
  CuAssert(T, "negative bad add",
    -3853 == PN_INT(potion_send(neg, potion_str(P, "+"), num)));
}

void potion_test_str(CuTest *T) {
  CuAssert(T, "string isn't a string", PN_IS_STR(PN_inspect));
  CuAssert(T, "string isn't a ref", potion_is_ref(PN_inspect));
  CuAssert(T, "string length isn't working",
    7 == PN_INT(potion_send(PN_inspect, potion_str(P, "length"))));
}

void potion_test_empty(CuTest *T) {
  CuAssert(T, "empty isn't a tuple", PN_IS_TUPLE(PN_EMPTY));
  CuAssert(T, "empty is a ref", !potion_is_ref(PN_EMPTY));
  CuAssertIntEquals(T, "tuple length is off",
    PN_INT(potion_send(PN_EMPTY, potion_str(P, "length"))), 0);
}

void potion_test_tuple(CuTest *T) {
  PN tup = potion_tuple_with_size(P, 3);
  PN_TUPLE_AT(tup, 0) = PN_EMPTY;
  PN_TUPLE_AT(tup, 1) = PN_inspect;
  PN_TUPLE_AT(tup, 2) = tup;
  CuAssert(T, "tuple isn't a tuple", PN_IS_TUPLE(tup));
  CuAssert(T, "tuple isn't a ref", potion_is_ref(tup));
  CuAssertIntEquals(T, "tuple length is off",
    PN_INT(potion_send(tup, potion_str(P, "length"))), 3);
}

CuSuite *potion_suite() {
  CuSuite *S = CuSuiteNew();
  SUITE_ADD_TEST(S, potion_test_nil);
  SUITE_ADD_TEST(S, potion_test_bool);
  SUITE_ADD_TEST(S, potion_test_int1);
  SUITE_ADD_TEST(S, potion_test_int2);
  SUITE_ADD_TEST(S, potion_test_int3);
  SUITE_ADD_TEST(S, potion_test_str);
  SUITE_ADD_TEST(S, potion_test_empty);
  SUITE_ADD_TEST(S, potion_test_tuple);
  return S;
}

int main(void) {
  int count;
  P = potion_create();
  CuString *out = CuStringNew();
  CuSuite *suite = potion_suite();
  CuSuiteRun(suite);
  CuSuiteSummary(suite, out);
  CuSuiteDetails(suite, out);
  printf("%s\n", out->buffer);
  count = suite->failCount;
  CuSuiteFree(suite);
  CuStringFree(out);
  potion_destroy(P);
  return count;
}
