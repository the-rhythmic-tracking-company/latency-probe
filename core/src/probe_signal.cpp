#include "latency_probe/probe_signal.h"

#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace latency_probe {

std::vector<float> makeSweep(const ProbeSpec& spec)
{
    if (spec.sampleRate <= 0.0 || spec.durationSec <= 0.0)
        throw std::invalid_argument("probe: sample rate and duration must be positive");
    if (spec.startHz <= 0.0 || spec.endHz <= spec.startHz || spec.endHz >= spec.sampleRate / 2.0)
        throw std::invalid_argument("probe: need 0 < startHz < endHz < Nyquist");
    if (spec.fadeSec < 0.0 || 2.0 * spec.fadeSec > spec.durationSec)
        throw std::invalid_argument("probe: fades longer than the sweep");
    if (spec.levelDbfs > 0.0)
        throw std::invalid_argument("probe: level must be <= 0 dBFS");

    const auto length = static_cast<std::size_t>(std::llround(spec.durationSec * spec.sampleRate));
    const auto fadeLen = static_cast<std::size_t>(std::llround(spec.fadeSec * spec.sampleRate));
    const double amplitude = std::pow(10.0, spec.levelDbfs / 20.0);
    const double pi = std::acos(-1.0);
    const double logRatio = std::log(spec.endHz / spec.startHz);
    const double k = 2.0 * pi * spec.startHz * spec.durationSec / logRatio;

    std::vector<float> out(length);
    for (std::size_t i = 0; i < length; ++i) {
        const double t = static_cast<double>(i) / spec.sampleRate;
        const double phase = k * (std::exp(t / spec.durationSec * logRatio) - 1.0);
        double gain = amplitude;
        if (fadeLen > 0) {
            const std::size_t fromEnd = length - 1 - i;
            const std::size_t edge = i < fromEnd ? i : fromEnd;
            if (edge < fadeLen)
                gain *= 0.5 - 0.5 * std::cos(pi * static_cast<double>(edge) / static_cast<double>(fadeLen));
        }
        out[i] = static_cast<float>(gain * std::sin(phase));
    }
    return out;
}

}  // namespace latency_probe
