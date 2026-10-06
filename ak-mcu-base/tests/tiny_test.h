/* Minimal test framework - no external dependency. */
#ifndef __TINY_TEST_H__
#define __TINY_TEST_H__

#include <stdio.h>

extern int tt_checks;
extern int tt_fails;

#define CHECK(c) do { \
	tt_checks++; \
	if (!(c)) { \
		tt_fails++; \
		printf("    FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
	} \
} while (0)

#define CHECK_EQ(a, b) do { \
	long long _a = (long long)(a), _b = (long long)(b); \
	tt_checks++; \
	if (_a != _b) { \
		tt_fails++; \
		printf("    FAIL %s:%d: %s == %s (%lld != %lld)\n", __FILE__, __LINE__, #a, #b, _a, _b); \
	} \
} while (0)

#define RUN_TEST(fn) do { printf("  %s\n", #fn); fn(); } while (0)

#define TT_MAIN_BEGIN(name) int tt_checks; int tt_fails; \
	int main(void) { printf("%s\n", name);

#define TT_MAIN_END() \
	printf("%d checks, %d failed\n", tt_checks, tt_fails); \
	return tt_fails ? 1 : 0; }

#endif
