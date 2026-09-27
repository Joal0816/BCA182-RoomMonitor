#ifndef _SHIM_UNITY_H
#define _SHIM_UNITY_H
#include <stdint.h>
#include <string.h>
int strcmp(const char*, const char*);
unsigned long _shim_len(const char*);
void _shim_fail(const char*, int, const char*);
int _shim_begin(void);
int _shim_end(void);
extern int _shim_fails, _shim_tests, _shim_cur_failed;

#define UNITY_BEGIN() _shim_begin()
#define UNITY_END()   _shim_end()
void setUp(void);
void tearDown(void);
#define RUN_TEST(f) do { _shim_cur_failed = 0; _shim_tests++; \
    setUp(); f(); tearDown(); \
    if (!_shim_cur_failed) _shim_pass(#f); } while (0)
void _shim_pass(const char*);

#define _FAIL(m) do { _shim_fail(m, __LINE__, __FILE__); _shim_fails++; _shim_cur_failed=1; } while (0)
#define TEST_ASSERT_EQUAL(e, a) do { double _e=(double)(e), _a=(double)(a); \
    if (_e != _a) _FAIL("TEST_ASSERT_EQUAL"); } while (0)
#define TEST_ASSERT_TRUE(a)  do { if (!(a)) _FAIL("TEST_ASSERT_TRUE"); } while (0)
#define TEST_ASSERT_FALSE(a) do { if ((a))  _FAIL("TEST_ASSERT_FALSE"); } while (0)
#define TEST_ASSERT_EQUAL_STRING(e, a) do { const char *_e=(e), *_a=(a); \
    if (strcmp(_e,_a)!=0) _FAIL("TEST_ASSERT_EQUAL_STRING"); } while (0)
#endif
