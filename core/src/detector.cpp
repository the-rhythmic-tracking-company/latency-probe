#include "latency_probe/detector.h"

#include <algorithm>
#include <cmath>
#include <complex>

#include "latency_probe/fft.h"

namespace latency_probe {

Detection detect(const float* signal, std::size_t signalLen,
                 const std::vector<float>& templ, std::size_t maxLag,
                 double minSnrDb)
{
    Detection result;
    if (templ.empty() || signalLen < templ.size()) return result;

    // Only lags where the whole template fits inside the signal count.
    const std::size_t lastLag = std::min(maxLag, signalLen - templ.size());

    // Zero-pad to >= signalLen + templ.size() so circular correlation equals
    // linear correlation over every lag we read.
    const std::size_t n = nextPowerOfTwo(signalLen + templ.size());
    std::vector<std::complex<double>> x(n), h(n);
    for (std::size_t i = 0; i < signalLen; ++i) x[i] = signal[i];
    for (std::size_t i = 0; i < templ.size(); ++i) h[i] = templ[i];
    fft(x, false);
    fft(h, false);
    for (std::size_t i = 0; i < n; ++i) x[i] *= std::conj(h[i]);
    fft(x, true);

    // Peak of |r|: some chains invert polarity.
    std::size_t peak = 0;
    double peakMag = -1.0;
    for (std::size_t lag = 0; lag <= lastLag; ++lag) {
        const double mag = std::abs(x[lag].real());
        if (mag > peakMag) {
            peakMag = mag;
            peak = lag;
        }
    }

    // Noise floor: RMS of the correlation away from the peak's main lobe
    // and nearest sidelobes.
    const std::size_t guard = templ.size() / 4;
    double sumSq = 0.0;
    std::size_t count = 0;
    for (std::size_t lag = 0; lag <= lastLag; ++lag) {
        const std::size_t dist = lag > peak ? lag - peak : peak - lag;
        if (dist <= guard) continue;
        const double v = x[lag].real();
        sumSq += v * v;
        ++count;
    }
    if (count == 0 || peakMag <= 0.0) return result;
    const double noiseRms = std::sqrt(sumSq / static_cast<double>(count));
    result.snrDb = noiseRms > 0.0 ? 20.0 * std::log10(peakMag / noiseRms) : 300.0;

    // Parabolic interpolation on the signed peak neighbourhood.
    double offset = 0.0;
    if (peak > 0 && peak < lastLag) {
        const double sign = x[peak].real() < 0.0 ? -1.0 : 1.0;
        const double y0 = sign * x[peak - 1].real();
        const double y1 = sign * x[peak].real();
        const double y2 = sign * x[peak + 1].real();
        const double denom = y0 - 2.0 * y1 + y2;
        if (denom < 0.0) offset = 0.5 * (y0 - y2) / denom;
    }
    result.lagSamples = static_cast<double>(peak) + offset;
    result.found = result.snrDb >= minSnrDb;
    return result;
}

}  // namespace latency_probe
