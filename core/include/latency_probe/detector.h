#pragma once

#include <cstddef>
#include <vector>

namespace latency_probe {

struct Detection {
    bool found = false;
    double lagSamples = 0.0;  ///< Sub-sample (parabolic) estimate of the peak.
    double snrDb = 0.0;       ///< Correlation peak relative to the RMS of the other lags.
};

/// Matched-filter search for `templ` inside `signal` at lags [0, maxLag].
///
/// Detection quality is the correlation peak over the RMS correlation at
/// lags away from the peak. Pure noise peaks at roughly 13 dB over a one-
/// second search window, so a threshold around 18 dB keeps false detections
/// rare while still finding a -30 dBFS sweep well below room noise.
///
/// Not real-time safe (allocates, runs FFTs); call after capture is done.
Detection detect(const float* signal, std::size_t signalLen,
                 const std::vector<float>& templ, std::size_t maxLag,
                 double minSnrDb);

}  // namespace latency_probe
