#include <random>
#include <vector>

#include "latency_probe/detector.h"
#include "latency_probe/probe_signal.h"
#include "test_harness.h"

using namespace latency_probe;

namespace {

std::vector<float> embed(const std::vector<float>& probe, std::size_t at, std::size_t length,
                         float gain, float noiseRms, unsigned seed)
{
    std::mt19937 rng(seed);
    std::normal_distribution<float> noise(0.0f, noiseRms);
    std::vector<float> out(length);
    for (auto& s : out) s = noiseRms > 0.0f ? noise(rng) : 0.0f;
    for (std::size_t i = 0; i < probe.size() && at + i < length; ++i) out[at + i] += gain * probe[i];
    return out;
}

}  // namespace

TEST("Given a clean delayed probe, when detecting, then the exact lag is found")
{
    const auto probe = makeSweep(ProbeSpec{});
    const auto signal = embed(probe, 1234, 48000, 1.0f, 0.0f, 1);
    const Detection d = detect(signal.data(), signal.size(), probe, 48000 - probe.size(), 18.0);
    CHECK(d.found);
    CHECK_NEAR(d.lagSamples, 1234.0, 0.05);
}

TEST("Given a probe 16 dB below white noise, when detecting, then it is still found")
{
    const auto probe = makeSweep(ProbeSpec{});
    // Probe RMS is ~0.7 * 10^(-30/20) ~= 0.022; noise ~6.7x that. Matched
    // filtering gains ~40 dB over 9600 samples, leaving ~23 dB vs an 18 dB threshold.
    const auto signal = embed(probe, 20000, 43200, 1.0f, 0.15f, 2);
    const Detection d = detect(signal.data(), signal.size(), probe, 33600, 18.0);
    CHECK(d.found);
    CHECK_NEAR(d.lagSamples, 20000.0, 1.0);
}

TEST("Given noise only, when detecting, then nothing is reported")
{
    const auto probe = makeSweep(ProbeSpec{});
    const auto signal = embed(probe, 0, 43200, 0.0f, 0.1f, 3);
    const Detection d = detect(signal.data(), signal.size(), probe, 33600, 18.0);
    CHECK(!d.found);
    CHECK(d.snrDb < 18.0);
}

TEST("Given an inverted probe, when detecting, then the lag is still found")
{
    const auto probe = makeSweep(ProbeSpec{});
    const auto signal = embed(probe, 777, 43200, -0.5f, 0.0f, 4);
    const Detection d = detect(signal.data(), signal.size(), probe, 33600, 18.0);
    CHECK(d.found);
    CHECK_NEAR(d.lagSamples, 777.0, 0.05);
}

TEST("Given a signal shorter than the probe, when detecting, then nothing is reported")
{
    const auto probe = makeSweep(ProbeSpec{});
    std::vector<float> signal(100, 0.0f);
    CHECK(!detect(signal.data(), signal.size(), probe, 1000, 18.0).found);
}
