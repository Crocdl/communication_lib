#ifndef TEST_UNITY_STUB_H
#define TEST_UNITY_STUB_H

#include <cstdio>
#include <cstdlib>
#include <cstring>

static inline void unity_fail_impl(const char* expr, const char* file, int line, const char* msg) {
    std::printf("%s:%d: assertion failed: %s", file, line, expr);
    if (msg) {
        std::printf(" (%s)", msg);
    }
    std::printf("\n");
    std::abort();
}

#define UNITY_BEGIN() (std::printf("UNITY_BEGIN\n"), 0)
#define UNITY_END() (std::printf("UNITY_END\n"), 0)
#define RUN_TEST(fn) do { std::printf("RUN_TEST(%s)\n", #fn); fn(); } while (0)

#define TEST_ASSERT_TRUE(v) do { if (!(v)) unity_fail_impl(#v, __FILE__, __LINE__, nullptr); } while (0)
#define TEST_ASSERT_TRUE_MESSAGE(v, msg) do { if (!(v)) unity_fail_impl(#v, __FILE__, __LINE__, msg); } while (0)
#define TEST_ASSERT_FALSE(v) do { if ((v)) unity_fail_impl("!(" #v ")", __FILE__, __LINE__, nullptr); } while (0)
#define TEST_ASSERT_FALSE_MESSAGE(v, msg) do { if ((v)) unity_fail_impl("!(" #v ")", __FILE__, __LINE__, msg); } while (0)

#define TEST_ASSERT_EQUAL(exp, act) do { if ((exp) != (act)) unity_fail_impl(#exp " == " #act, __FILE__, __LINE__, nullptr); } while (0)
#define TEST_ASSERT_EQUAL_MESSAGE(exp, act, msg) do { if ((exp) != (act)) unity_fail_impl(#exp " == " #act, __FILE__, __LINE__, msg); } while (0)
#define TEST_ASSERT_EQUAL_UINT8_MESSAGE(exp, act, msg) TEST_ASSERT_EQUAL_MESSAGE(static_cast<unsigned>(exp), static_cast<unsigned>(act), msg)

#define TEST_ASSERT_GREATER_THAN(exp, act) do { if (!((act) > (exp))) unity_fail_impl(#act " > " #exp, __FILE__, __LINE__, nullptr); } while (0)
#define TEST_ASSERT_GREATER_THAN_MESSAGE(exp, act, msg) do { if (!((act) > (exp))) unity_fail_impl(#act " > " #exp, __FILE__, __LINE__, msg); } while (0)

#define TEST_ASSERT_EQUAL_UINT8_ARRAY(exp, act, len) do { if (std::memcmp((exp), (act), (len)) != 0) unity_fail_impl(#exp " == " #act, __FILE__, __LINE__, nullptr); } while (0)
#define TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(exp, act, len, msg) do { if (std::memcmp((exp), (act), (len)) != 0) unity_fail_impl(#exp " == " #act, __FILE__, __LINE__, msg); } while (0)

#define TEST_FAIL_MESSAGE(msg) unity_fail_impl("TEST_FAIL_MESSAGE", __FILE__, __LINE__, msg)
#define TEST_PASS() do { } while (0)

#endif // TEST_UNITY_STUB_H
