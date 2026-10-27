#pragma once

#include <string>
#include <vector>

#include "latency_probe/measurement.h"

namespace latency_probe {

/// Version of the JSON documents printed on stdout; see docs/protocol.md.
/// Bump on any breaking change so the Stage Bridge can refuse what it
/// doesn't understand.
constexpr int kSchemaVersion = 1;

/// Context a backend adds around a MeasurementResult.
struct ReportContext {
    std::string backend;  ///< "jack" (later: "asio", "coreaudio")
    std::string target;   ///< App whose ports were probed, or "self-test"
    double sampleRate = 0.0;
    unsigned bufferFrames = 0;
    double deviceInputMs = 0.0;   ///< Interface capture latency reported by the driver
    double deviceOutputMs = 0.0;  ///< Interface playback latency reported by the driver
};

std::string jsonEscape(const std::string& s);

/// `{"schema":1,"ok":true,...}` for a finished measurement. `ok` is false
/// (error "not-detected") when no probe came back.
std::string measurementJson(const ReportContext& ctx, int requestedProbes,
                            const MeasurementResult& result);

/// `{"schema":1,"ok":false,"error":code,"message":...}`
std::string errorJson(const std::string& code, const std::string& message);

}  // namespace latency_probe
