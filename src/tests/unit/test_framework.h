#ifndef MVC_LIGHT_TESTS_UNIT_TEST_FRAMEWORK_H
#define MVC_LIGHT_TESTS_UNIT_TEST_FRAMEWORK_H

/*
 * Phase 0 轻量测试框架（避免阻塞于 Boost.Test 安装）。
 * 后续 Phase 引入 Boost.Test 后可平滑替换。
 */

#include <cstdio>

namespace mvclight_test {

inline int& FailedCount() {
    static int failed = 0;
    return failed;
}

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
            ++mvclight_test::FailedCount();                                  \
        }                                                                    \
    } while (0)

#define CHECK_EQ(a, b)                                                       \
    do {                                                                     \
        auto va = (a);                                                       \
        auto vb = (b);                                                       \
        if (!(va == vb)) {                                                   \
            std::printf("FAIL %s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b); \
            ++mvclight_test::FailedCount();                                  \
        }                                                                    \
    } while (0)

#define TEST_MAIN_RETURN()                                                   \
    do {                                                                     \
        if (mvclight_test::FailedCount() == 0) {                             \
            std::printf("PASS %s\n", __FILE__);                              \
            return 0;                                                        \
        }                                                                    \
        std::printf("FAILED %d check(s) in %s\n",                            \
                    mvclight_test::FailedCount(), __FILE__);                 \
        return 1;                                                            \
    } while (0)

} // namespace mvclight_test

#endif // MVC_LIGHT_TESTS_UNIT_TEST_FRAMEWORK_H
