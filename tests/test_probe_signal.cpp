#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "latency_probe/probe_signal.h"
#include "test_harness.h"

using namespace latency_probe;

TEST("Given the default spec, when rendering the sweep, then length and peak level match")
{
    const ProbeSpec spec;
    const auto sweep = makeSweep(spec);
    CHECK(sweep.size() == 9600);  // 0.2 s at 48 kHz

    float peak = 0.0f;
    for (float s : sweep) peak = std::max(peak, std::fabs(s));
    CHECK_NEAR(20.0 * std::log10(peak), spec.levelDbfs, 0.1);
}

TEST("Given fades, when rendering the sweep, then it starts and ends at silence")
{
    const auto sweep = makeSweep(ProbeSpec{});
    CHECK_NEAR(sweep.front(), 0.0, 1e-6);
    CHECK_NEAR(sweep.back(), 0.0, 1e-3);
}

TEST("Given a band above Nyquist, when rendering, then it is rejected")
{
    ProbeSpec spec;
    spec.sampleRate = 16000.0;  // Nyquist 8 kHz == default endHz
    bool threw = false;
    try {
        makeSweep(spec);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

TEST("Given a positive dBFS level, when rendering, then it is rejected")
{
    ProbeSpec spec;
    spec.levelDbfs = 3.0;
    bool threw = false;
    try {
        makeSweep(spec);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}
