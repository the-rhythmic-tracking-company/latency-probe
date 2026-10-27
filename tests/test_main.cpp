#include "test_harness.h"

int main()
{
    for (const auto& c : test::registry()) {
        const int before = test::failures();
        c.fn();
        std::cerr << (test::failures() == before ? "ok   " : "FAIL ") << c.name << "\n";
    }
    std::cerr << test::registry().size() << " tests, " << test::failures() << " failed checks\n";
    return test::failures() == 0 ? 0 : 1;
}
