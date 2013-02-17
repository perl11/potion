#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "CuTest.h"

/*-------------------------------------------------------------------------*
 * CuStr
 *-------------------------------------------------------------------------*/

char* CuStrAlloc(int size)
{
	char* new = (char*) malloc( sizeof(char) * (size) );
	return new;
}

char* CuStrCopy(char* old)
{
	int len = strlen(old);
	char* new = CuStrAlloc(len + 1);
	strcpy(new, old);
	return new;
}

/*-------------------------------------------------------------------------*
 * CuString
 *-------------------------------------------------------------------------*/

void CuStringInit(CuString* str)
{
	str->length = 0;
	str->size = STRING_MAX;
	str->buffer = (char*) malloc(sizeof(char) * str->size);
	str->buffer[0] = '\0';
}

CuString* CuStringNew(void)
{
	CuString* str = (CuString*) malloc(sizeof(CuString));
	str->length = 0;
	str->size = STRING_MAX;
	str->buffer = (char*) malloc(sizeof(char) * str->size);
	str->buffer[0] = '\0';
	return str;
}

void CuStringResize(CuString* str, int newSize)
{
	str->buffer = (char*) realloc(str->buffer, sizeof(char) * newSize);
	str->size = newSize;
}

void CuStringAppend(CuString* str, char* text)
{
	int length = strlen(text);
	CuStringAppendLen(str, text, length);
}

void CuStringAppendLen(CuString* str, char* text, long length)
{
	if (str->length + length + 1 >= str->size)
		CuStringResize(str, str->length + length + 1 + STRING_INC);
	str->length += length;
	strcat(str->buffer, text);
}

void CuStringAppendChar(CuString* str, char ch)
{
	char text[2];
	text[0] = ch;
	text[1] = '\0';
	CuStringAppend(str, text);
}

void CuStringAppendFormat(CuString* str, char* format, ...)
{
	va_list argp;
	char buf[HUGE_STRING_LEN];
	va_start(argp, format);
	vsprintf(buf, format, argp);
	va_end(argp);
	CuStringAppend(str, buf);
}

void CuStringFree(CuString* str)
{
    if ( str != NULL ) {
        free( str->buffer );
        free( str );
    }
}

/*-------------------------------------------------------------------------*
 * CuTest
 *-------------------------------------------------------------------------*/

void CuTestInit(CuTest* t, char* name, TestFunction function)
{
	t->name = CuStrCopy(name);
	t->failed = 0;
	t->ran = 0;
	t->message = NULL;
	t->function = function;
	t->jumpBuf = NULL;
}

CuTest* CuTestNew(char* name, TestFunction function)
{
	CuTest* tc = CU_ALLOC(CuTest);
	CuTestInit(tc, name, function);
	return tc;
}

void CuTestFree(CuTest* t)
{
    if ( t != NULL ) {
        free( t->name );
        free( t );
    }
}

void CuFail(CuTest* tc, char* message)
{
	tc->failed = 1;
	tc->message = CuStrCopy(message);
	if (tc->jumpBuf != 0) longjmp(*(tc->jumpBuf), 0);
}

void CuAssert(CuTest* tc, char* message, int condition)
{
	if (condition) return;
	CuFail(tc, message);
}

void CuAssertTrue(CuTest* tc, int condition)
{
	if (condition) return;
	CuFail(tc, "assert failed");
}

void CuAssertStrEquals(CuTest* tc, char* expected, char* actual)
{
	CuString* message;
	if (strcmp(expected, actual) == 0) return;
	message = CuStringNew();
	CuStringAppend(message, "expected <");
	CuStringAppend(message, expected);
	CuStringAppend(message, "> but was <");
	CuStringAppend(message, actual);
	CuStringAppend(message, ">");
	CuFail(tc, message->buffer);
}

void CuAssertIntEquals(CuTest* tc, char *message, int expected, int actual)
{
	char buf[STRING_MAX];
	if (expected == actual) return;
	sprintf(buf, "%s, expected <%d> but was <%d>", message, expected, actual);
	CuFail(tc, buf);
}

void CuAssertPtrEquals(CuTest* tc, void* expected, void* actual)
{
	char buf[STRING_MAX];
	if (expected == actual) return;
	sprintf(buf, "expected pointer <0x%p> but was <0x%p>", expected, actual);
	CuFail(tc, buf);
}

void CuAssertPtrNotNull(CuTest* tc, void* pointer)
{
	char buf[STRING_MAX];
	if (pointer != NULL ) return;
	sprintf(buf, "null pointer unexpected");
	CuFail(tc, buf);
}

void CuTestRun(CuTest* tc)
{
	jmp_buf buf;
	tc->jumpBuf = &buf;
	if (setjmp(buf) == 0) {
		tc->ran = 1;
		(tc->function)(tc);
	}
	tc->jumpBuf = 0;
}

/*-------------------------------------------------------------------------*
 * CuSuite
 *-------------------------------------------------------------------------*/

void CuSuiteInit(CuSuite* testSuite)
{
	testSuite->count = 0;
	testSuite->failCount = 0;
}

CuSuite* CuSuiteNew()
{
	CuSuite* testSuite = CU_ALLOC(CuSuite);
	CuSuiteInit(testSuite);
	return testSuite;
}

void CuSuiteFree(CuSuite* testSuite)
{
	int i;
	for (i = 0 ; i < testSuite->count ; ++i) {
		CuTestFree( testSuite->list[i] );
	}
	free( testSuite );
}

void CuSuiteAdd(CuSuite* testSuite, CuTest *testCase)
{
	assert(testSuite->count < MAX_TEST_CASES);
	testSuite->list[testSuite->count] = testCase;
	testSuite->count++;
}

void CuSuiteAddSuite(CuSuite* testSuite, CuSuite* testSuite2)
{
	int i;
	for (i = 0 ; i < testSuite2->count ; ++i) {
		CuTest* testCase = testSuite2->list[i];
		CuSuiteAdd(testSuite, testCase);
	}
}

void CuSuiteRun(CuSuite* testSuite)
{
	int i;
	for (i = 0 ; i < testSuite->count ; ++i) {
		CuTest* testCase = testSuite->list[i];
		CuTestRun(testCase);
		if (testCase->failed) { testSuite->failCount += 1; }
	}
}

void CuSuiteSummary(CuSuite* testSuite, CuString* summary)
{
	int i;
	for (i = 0 ; i < testSuite->count ; ++i) {
		CuTest* testCase = testSuite->list[i];
		CuStringAppend(summary, testCase->failed ? "F" : ".");
	}
	CuStringAppend(summary, "\n");
}

void CuSuiteDetails(CuSuite* testSuite, CuString* details)
{
	int i;
	int failCount = 0;

	if (testSuite->failCount == 0) {
		int passCount = testSuite->count - testSuite->failCount;
		char* testWord = passCount == 1 ? "test" : "tests";
		CuStringAppendFormat(details, "OK (%d %s)\n", passCount, testWord);
	}
	else {
		if (testSuite->failCount == 1)
			CuStringAppend(details, "There was 1 failure:\n");
		else
			CuStringAppendFormat(details, "There were %d failures:\n", testSuite->failCount);

		for (i = 0 ; i < testSuite->count ; ++i) {
			CuTest* testCase = testSuite->list[i];
			if (testCase->failed) {
				failCount++;
				CuStringAppendFormat(details, "%d) %s: %s\n", 
					failCount, testCase->name, testCase->message);
			}
		}
		CuStringAppend(details, "FAIL:\n");

		CuStringAppendFormat(details, "Runs: %d ",   testSuite->count);
		CuStringAppendFormat(details, "Passes: %d ", testSuite->count - testSuite->failCount);
		CuStringAppendFormat(details, "Fails: %d\n",  testSuite->failCount);
	}
}
