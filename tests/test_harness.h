#pragma once

// Minimal dependency-free test harness: TEST registers a function, CHECK
// records failures without aborting the test, test_main.cpp runs them all.

#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace test {

struct Case {
    std::string name;
    std::function<void()> fn;
};

inline std::vector<Case>& registry()
{
    static std::vector<Case> cases;
    return cases;
}

inline int& failures()
{
    static int count = 0;
    return count;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> fn) { registry().push_back({name, std::move(fn)}); }
};

inline void fail(const char* file, int line, const std::string& what)
{
    ++failures();
    std::cerr << "  FAIL " << file << ":" << line << ": " << what << "\n";
}

}  // namespace test

#define TEST_CONCAT_(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT_(a, b)
#define TEST(name)                                                              \
    static void TEST_CONCAT(test_fn_, __LINE__)();                              \
    static test::Registrar TEST_CONCAT(test_reg_, __LINE__)(name, TEST_CONCAT(test_fn_, __LINE__)); \
    static void TEST_CONCAT(test_fn_, __LINE__)()

#define CHECK(cond) \
    do { if (!(cond)) test::fail(__FILE__, __LINE__, #cond); } while (0)

#define CHECK_NEAR(actual, expected, tol)                                                     \
    do {                                                                                      \
        const double a_ = (actual), e_ = (expected);                                          \
        if (!(std::fabs(a_ - e_) <= (tol)))                                                   \
            test::fail(__FILE__, __LINE__, std::string(#actual " = ") + std::to_string(a_) + \
                       ", expected " + std::to_string(e_) + " +/- " + std::to_string(tol));   \
    } while (0)
