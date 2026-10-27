#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace latency_probe {

struct MeasurementConfig {
    double sampleRate = 48000.0;
    int probeCount = 5;
    double intervalSec = 1.0;    ///< Start-to-start spacing of probes.
    double leadInSec = 0.25;     ///< Silence before the first probe.
    double maxLatencySec = 0.7;  ///< Longest round trip we search for.
    double levelDbfs = -30.0;
    double minSnrDb = 18.0;
};

struct ProbeResult {
    bool detected = false;
    double lagMs = 0.0;
    double snrDb = 0.0;
};

struct MeasurementResult {
    std::vector<ProbeResult> probes;
    int validCount = 0;
    double medianMs = 0.0;  ///< Median lag of detected probes (0 if none).
    double spreadMs = 0.0;  ///< Max - min lag of detected probes.
};

/// One measurement run on a single sample timeline shared by the probe
/// output and the capture input.
///
/// Threading contract:
///  - arm() and analyze() run on a control thread.
///  - render() and capture() run on real-time audio threads. They allocate
///    nothing, take no locks, and may run on different threads as long as,
///    within a cycle, render() for a frame happens before capture() of the
///    same frame (true for JACK when probe-out feeds the app which feeds
///    probe-in, and for an ASIO pass-through within one bufferSwitch).
///  - Frame times are 32-bit and compared with wrap-safe unsigned
///    differences, matching jack_nframes_t.
///
/// A Measurement is single-use: arm once, then analyze once done().
class Measurement {
public:
    /// Allocates the probe and capture buffer. Returns an error message, or
    /// an empty string on success. Must be called before the audio callbacks
    /// can observe this object as armed (it publishes with release order).
    std::string arm(const MeasurementConfig& config);

    /// Writes this cycle's probe output into `out` (overwrites; zeros when
    /// no probe is sounding). The first call after arm() fixes the timeline.
    void render(std::uint32_t frameTime, float* out, std::uint32_t nframes) noexcept;

    /// Records what came back from the app. Frames before the timeline
    /// start or past the capture window are ignored.
    void capture(std::uint32_t frameTime, const float* in, std::uint32_t nframes) noexcept;

    bool done() const noexcept;

    /// Runs detection for every probe. Only valid once done().
    MeasurementResult analyze() const;

    const std::vector<float>& probe() const noexcept { return probe_; }
    std::size_t captureLength() const noexcept { return captured_.size(); }

private:
    enum State : int { Idle, Armed, Running, Done };

    std::size_t emissionOffset(int probeIndex) const noexcept;

    MeasurementConfig config_;
    std::vector<float> probe_;
    std::vector<float> captured_;
    std::size_t leadInFrames_ = 0;
    std::size_t intervalFrames_ = 0;
    std::size_t maxLagFrames_ = 0;
    std::uint32_t startFrame_ = 0;  // written by render() before publishing Running
    std::atomic<int> state_{Idle};
};

}  // namespace latency_probe
