// Minimal test framework: TEST(name) { CHECK(...); CHECK_NEAR(a, b, eps); }
#pragma once
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace test {
struct Case { const char* name; std::function<void()> fn; };
inline std::vector<Case>& cases() { static std::vector<Case> c; return c; }
inline int& failures() { static int f = 0; return f; }
struct Reg { Reg(const char* n, std::function<void()> f) { cases().push_back({n, f}); } };
inline void fail(const char* file, int line, const std::string& msg) {
    std::printf("    FAIL %s:%d: %s\n", file, line, msg.c_str());
    failures()++;
}
} // namespace test

#define TEST_CAT2(a, b) a##b
#define TEST_CAT(a, b) TEST_CAT2(a, b)
#define TEST(name) \
    static void name(); \
    static test::Reg TEST_CAT(reg_, name)(#name, name); \
    static void name()
#define CHECK(cond) do { if (!(cond)) test::fail(__FILE__, __LINE__, #cond); } while (0)
#define CHECK_NEAR(a, b, eps) do { double _a = (a), _b = (b); if (!(std::fabs(_a - _b) <= (eps))) \
    test::fail(__FILE__, __LINE__, std::string(#a " = ") + std::to_string(_a) + " expected " + std::to_string(_b)); } while (0)
