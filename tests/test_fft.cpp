#include <complex>
#include <vector>

#include "latency_probe/fft.h"
#include "test_harness.h"

using latency_probe::fft;
using latency_probe::nextPowerOfTwo;

TEST("Given sizes, when rounding to a power of two, then the next power is returned")
{
    CHECK(nextPowerOfTwo(0) == 1);
    CHECK(nextPowerOfTwo(1) == 1);
    CHECK(nextPowerOfTwo(5) == 8);
    CHECK(nextPowerOfTwo(1024) == 1024);
    CHECK(nextPowerOfTwo(57601) == 65536);
}

TEST("Given an impulse, when transformed, then the spectrum is flat")
{
    std::vector<std::complex<double>> x(16);
    x[0] = 1.0;
    fft(x, false);
    for (const auto& v : x) CHECK_NEAR(std::abs(v), 1.0, 1e-12);
}

TEST("Given a signal, when transformed forward then inverse, then it round-trips")
{
    std::vector<std::complex<double>> x(64), original;
    for (std::size_t i = 0; i < x.size(); ++i) x[i] = {std::sin(0.3 * i), std::cos(0.7 * i)};
    original = x;
    fft(x, false);
    fft(x, true);
    for (std::size_t i = 0; i < x.size(); ++i) CHECK_NEAR(std::abs(x[i] - original[i]), 0.0, 1e-12);
}

TEST("Given a pure tone at bin 3, when transformed, then energy lands in bins 3 and N-3")
{
    const std::size_t n = 32;
    std::vector<std::complex<double>> x(n);
    const double pi = std::acos(-1.0);
    for (std::size_t i = 0; i < n; ++i) x[i] = std::cos(2.0 * pi * 3.0 * i / n);
    fft(x, false);
    CHECK_NEAR(std::abs(x[3]), n / 2.0, 1e-9);
    CHECK_NEAR(std::abs(x[n - 3]), n / 2.0, 1e-9);
    CHECK_NEAR(std::abs(x[5]), 0.0, 1e-9);
}
