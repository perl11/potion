#ifndef CU_TEST_H
#define CU_TEST_H

#include <setjmp.h>
#include <stdarg.h>

/* CuString */

char* CuStrAlloc(int size);
char* CuStrCopy(char* old);

#define CU_ALLOC(TYPE)		((TYPE*) malloc(sizeof(TYPE)))

#define HUGE_STRING_LEN	8192
#define STRING_MAX		256
#define STRING_INC		256

typedef struct
{
	int length;
	int size;
	char* buffer;
} CuString;

void CuStringInit(CuString* str);
CuString* CuStringNew(void);
void CuStringRead(CuString* str, char* path);
void CuStringAppend(CuString* str, char* text);
void CuStringAppendLen(CuString* str, char* text, long length);
void CuStringAppendChar(CuString* str, char ch);
void CuStringAppendFormat(CuString* str, char* format, ...);
void CuStringResize(CuString* str, int newSize);
void CuStringFree(CuString* str);

void CuStringFree(CuString *str);

/* CuTest */

typedef struct CuTest CuTest;

typedef void (*TestFunction)(CuTest *);

struct CuTest
{
	char* name;
	TestFunction function;
	int failed;
	int ran;
	char* message;
	jmp_buf *jumpBuf;
};

void CuTestInit(CuTest* t, char* name, TestFunction function);
CuTest* CuTestNew(char* name, TestFunction function);
void CuFail(CuTest* tc, char* message);
void CuAssert(CuTest* tc, char* message, int condition);
void CuAssertTrue(CuTest* tc, int condition);
void CuAssertStrEquals(CuTest* tc, char* expected, char* actual);
void CuAssertIntEquals(CuTest* tc, char *message, int expected, int actual);
void CuAssertPtrEquals(CuTest* tc, void* expected, void* actual);
void CuAssertPtrNotNull(CuTest* tc, void* pointer);
void CuTestRun(CuTest* tc);

/* CuSuite */

#define MAX_TEST_CASES	1024	

#define SUITE_ADD_TEST(SUITE,TEST)	CuSuiteAdd(SUITE, CuTestNew(#TEST, TEST))

typedef struct
{
	int count;
	CuTest* list[MAX_TEST_CASES]; 
	int failCount;

} CuSuite;


void CuSuiteInit(CuSuite* testSuite);
CuSuite* CuSuiteNew();
void CuSuiteFree(CuSuite* testSuite);
void CuSuiteAdd(CuSuite* testSuite, CuTest *testCase);
void CuSuiteAddSuite(CuSuite* testSuite, CuSuite* testSuite2);
void CuSuiteRun(CuSuite* testSuite);
void CuSuiteSummary(CuSuite* testSuite, CuString* summary);
void CuSuiteDetails(CuSuite* testSuite, CuString* details);
void CuSuiteFree(CuSuite *testsuite);

#endif /* CU_TEST_H */
