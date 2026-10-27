#include "latency_probe/measurement.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "latency_probe/detector.h"
#include "latency_probe/probe_signal.h"

namespace latency_probe {

namespace {

std::size_t framesFor(double seconds, double sampleRate)
{
    return static_cast<std::size_t>(std::llround(seconds * sampleRate));
}

}  // namespace

std::string Measurement::arm(const MeasurementConfig& config)
{
    if (state_.load(std::memory_order_acquire) != Idle) return "measurement already armed";
    if (config.probeCount < 1 || config.probeCount > 50) return "probe count must be 1..50";
    if (config.maxLatencySec <= 0.0 || config.maxLatencySec > 5.0) return "max latency must be in (0, 5] s";
    if (config.leadInSec < 0.0) return "lead-in must be >= 0";
    // Below -60 dBFS the probe sinks under any real interface's noise floor
    // (and Opus's), so it would only ever "work" on a purely digital path.
    if (config.levelDbfs < -60.0 || config.levelDbfs > 0.0) return "level must be -60..0 dBFS";

    ProbeSpec spec;
    spec.sampleRate = config.sampleRate;
    spec.levelDbfs = config.levelDbfs;
    try {
        probe_ = makeSweep(spec);
    } catch (const std::invalid_argument& e) {
        return e.what();
    }

    // Each probe's search window is [emission, emission + maxLag + probe).
    // The next probe must not start inside it, or it could be mistaken for
    // a late return of the current one.
    if (config.intervalSec < config.maxLatencySec + spec.durationSec)
        return "interval must be >= max latency + probe duration";

    config_ = config;
    leadInFrames_ = framesFor(config.leadInSec, config.sampleRate);
    intervalFrames_ = framesFor(config.intervalSec, config.sampleRate);
    maxLagFrames_ = framesFor(config.maxLatencySec, config.sampleRate);
    captured_.assign(emissionOffset(config.probeCount - 1) + maxLagFrames_ + probe_.size(), 0.0f);

    state_.store(Armed, std::memory_order_release);
    return {};
}

std::size_t Measurement::emissionOffset(int probeIndex) const noexcept
{
    return leadInFrames_ + static_cast<std::size_t>(probeIndex) * intervalFrames_;
}

void Measurement::render(std::uint32_t frameTime, float* out, std::uint32_t nframes) noexcept
{
    std::fill(out, out + nframes, 0.0f);

    int state = state_.load(std::memory_order_acquire);
    if (state == Armed) {
        startFrame_ = frameTime;
        state_.store(Running, std::memory_order_release);
        state = Running;
    }
    if (state != Running) return;

    const std::size_t cycleStart = static_cast<std::uint32_t>(frameTime - startFrame_);
    const std::size_t cycleEnd = cycleStart + nframes;
    for (int k = 0; k < config_.probeCount; ++k) {
        const std::size_t probeStart = emissionOffset(k);
        const std::size_t probeEnd = probeStart + probe_.size();
        const std::size_t from = std::max(cycleStart, probeStart);
        const std::size_t to = std::min(cycleEnd, probeEnd);
        for (std::size_t t = from; t < to; ++t) out[t - cycleStart] = probe_[t - probeStart];
    }
}

void Measurement::capture(std::uint32_t frameTime, const float* in, std::uint32_t nframes) noexcept
{
    if (state_.load(std::memory_order_acquire) != Running) return;

    // Wrap-safe: a frame before startFrame_ yields a huge offset and is ignored.
    const std::size_t offset = static_cast<std::uint32_t>(frameTime - startFrame_);
    const std::size_t length = captured_.size();
    if (offset >= length) return;

    const std::size_t count = std::min<std::size_t>(nframes, length - offset);
    std::copy(in, in + count, captured_.begin() + static_cast<std::ptrdiff_t>(offset));
    if (offset + count == length) state_.store(Done, std::memory_order_release);
}

bool Measurement::done() const noexcept
{
    return state_.load(std::memory_order_acquire) == Done;
}

MeasurementResult Measurement::analyze() const
{
    if (!done()) throw std::logic_error("analyze() before capture finished");

    MeasurementResult result;
    std::vector<double> lags;
    const std::size_t windowLen = maxLagFrames_ + probe_.size();
    for (int k = 0; k < config_.probeCount; ++k) {
        const Detection d = detect(captured_.data() + emissionOffset(k), windowLen,
                                   probe_, maxLagFrames_, config_.minSnrDb);
        ProbeResult p;
        p.detected = d.found;
        p.snrDb = d.snrDb;
        p.lagMs = d.lagSamples * 1000.0 / config_.sampleRate;
        if (p.detected) lags.push_back(p.lagMs);
        result.probes.push_back(p);
    }

    result.validCount = static_cast<int>(lags.size());
    if (!lags.empty()) {
        std::sort(lags.begin(), lags.end());
        const std::size_t mid = lags.size() / 2;
        result.medianMs = lags.size() % 2 ? lags[mid] : 0.5 * (lags[mid - 1] + lags[mid]);
        result.spreadMs = lags.back() - lags.front();
    }
    return result;
}

}  // namespace latency_probe
