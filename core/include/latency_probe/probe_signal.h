#pragma once

#include <vector>

namespace latency_probe {

/// Parameters for the probe: an exponential (log) sine sweep. A sweep is used
/// instead of a plain sine because its autocorrelation has a single sharp
/// peak, so the return time is unambiguous to a fraction of a sample, and it
/// survives Opus coding and jitter-buffer smoothing. The band stays well
/// inside what low-bitrate Opus preserves.
struct ProbeSpec {
    double sampleRate = 48000.0;
    double durationSec = 0.2;
    double startHz = 200.0;
    double endHz = 8000.0;
    double levelDbfs = -30.0;  ///< Peak level of the sweep.
    double fadeSec = 0.005;    ///< Raised-cosine fade in/out, avoids clicks.
};

/// Throws std::invalid_argument for a spec that cannot be rendered
/// (non-positive duration, band outside (0, Nyquist), etc.).
std::vector<float> makeSweep(const ProbeSpec& spec);

}  // namespace latency_probe
