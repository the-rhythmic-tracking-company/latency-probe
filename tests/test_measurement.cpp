#include <cstdint>
#include <random>
#include <vector>

#include "latency_probe/measurement.h"
#include "test_harness.h"

using namespace latency_probe;

namespace {

/// Drives a Measurement the way an audio backend does: fixed-size cycles,
/// render() then capture(), with the "app" modelled as a pure delay plus
/// optional noise. Starts near the 32-bit frame-time wrap on purpose.
MeasurementResult runThroughDelay(Measurement& m, std::size_t delayFrames, float gain,
                                  float noiseRms, std::uint32_t firstFrame = 0xFFFF0000u,
                                  std::uint32_t blockSize = 128)
{
    std::vector<float> history;  // everything rendered, indexed from firstFrame
    std::vector<float> block(blockSize), returned(blockSize);
    std::mt19937 rng(42);
    std::normal_distribution<float> noise(0.0f, noiseRms);

    std::uint32_t frame = firstFrame;
    for (int cycle = 0; cycle < 100000 && !m.done(); ++cycle, frame += blockSize) {
        m.render(frame, block.data(), blockSize);
        history.insert(history.end(), block.begin(), block.end());
        const std::size_t base = history.size() - blockSize;
        for (std::uint32_t i = 0; i < blockSize; ++i) {
            const std::size_t t = base + i;
            returned[i] = (t >= delayFrames ? gain * history[t - delayFrames] : 0.0f)
                          + (noiseRms > 0.0f ? noise(rng) : 0.0f);
        }
        m.capture(frame, returned.data(), blockSize);
    }
    return m.done() ? m.analyze() : MeasurementResult{};
}

}  // namespace

TEST("Given a 37.5 ms delay, when measuring across the frame-time wrap, then every probe reports it")
{
    Measurement m;
    MeasurementConfig config;
    CHECK(m.arm(config).empty());
    const MeasurementResult r = runThroughDelay(m, 1800, 0.8f, 0.0f);
    CHECK(m.done());
    CHECK(r.validCount == config.probeCount);
    CHECK_NEAR(r.medianMs, 37.5, 0.01);
    CHECK_NEAR(r.spreadMs, 0.0, 0.01);
}

TEST("Given zero delay (self-test wiring), when measuring, then 0 ms is reported")
{
    Measurement m;
    CHECK(m.arm(MeasurementConfig{}).empty());
    const MeasurementResult r = runThroughDelay(m, 0, 1.0f, 0.0f, 0u);
    CHECK(r.validCount == 5);
    CHECK_NEAR(r.medianMs, 0.0, 0.01);
}

TEST("Given a quiet return under noise, when measuring, then the median is within a sample")
{
    Measurement m;
    CHECK(m.arm(MeasurementConfig{}).empty());
    // Returned probe RMS ~0.0056 against noise 0.02: ~11 dB below the noise.
    const MeasurementResult r = runThroughDelay(m, 12345, 0.25f, 0.02f);
    CHECK(r.validCount >= 4);
    CHECK_NEAR(r.medianMs, 12345.0 / 48.0, 1.0 / 48.0);
}

TEST("Given an app that never returns the signal, when measuring, then nothing is detected")
{
    Measurement m;
    CHECK(m.arm(MeasurementConfig{}).empty());
    const MeasurementResult r = runThroughDelay(m, 1000, 0.0f, 0.01f);
    CHECK(m.done());
    CHECK(r.validCount == 0);
}

TEST("Given a probe interval shorter than the search window, when arming, then it is rejected")
{
    Measurement m;
    MeasurementConfig config;
    config.intervalSec = 0.5;  // < 0.7 s max latency + 0.2 s probe
    CHECK(!m.arm(config).empty());
}

TEST("Given a probe level outside -60..0 dBFS, when arming, then it is rejected")
{
    MeasurementConfig config;
    config.levelDbfs = -61.0;
    Measurement quiet;
    CHECK(!quiet.arm(config).empty());
    config.levelDbfs = 1.0;
    Measurement loud;
    CHECK(!loud.arm(config).empty());
}

TEST("Given an armed measurement, when arming again, then it is rejected")
{
    Measurement m;
    CHECK(m.arm(MeasurementConfig{}).empty());
    CHECK(!m.arm(MeasurementConfig{}).empty());
}

TEST("Given an unarmed measurement, when rendering, then silence is written")
{
    Measurement m;
    std::vector<float> out(64, 1.0f);
    m.render(0, out.data(), 64);
    for (float s : out) CHECK(s == 0.0f);
}
