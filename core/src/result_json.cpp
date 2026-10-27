#include "latency_probe/result_json.h"

#include <cmath>
#include <cstdio>
#include <sstream>

namespace latency_probe {

namespace {

// Fixed precision keeps output locale-independent and stable for tests.
std::string num(double v, int decimals = 3)
{
    if (!std::isfinite(v)) return "null";
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.*f", decimals, v);
    return buf;
}

std::string str(const std::string& s)
{
    return "\"" + jsonEscape(s) + "\"";
}

}  // namespace

std::string jsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (const unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

std::string measurementJson(const ReportContext& ctx, int requestedProbes,
                            const MeasurementResult& result)
{
    const bool ok = result.validCount > 0;
    std::ostringstream o;
    o << "{\"schema\":" << kSchemaVersion << ",\"ok\":" << (ok ? "true" : "false");
    if (!ok) {
        o << ",\"error\":\"not-detected\""
          << ",\"message\":" << str("No probe returned; is the app sending your own signal back to you?");
    }
    o << ",\"method\":\"loopback\""
      << ",\"backend\":" << str(ctx.backend)
      << ",\"target\":" << str(ctx.target)
      << ",\"sampleRate\":" << num(ctx.sampleRate, 0)
      << ",\"bufferFrames\":" << ctx.bufferFrames
      << ",\"requestedProbes\":" << requestedProbes
      << ",\"validProbes\":" << result.validCount;
    if (ok) {
        o << ",\"appRoundTripMs\":" << num(result.medianMs)
          << ",\"spreadMs\":" << num(result.spreadMs)
          << ",\"deviceInputMs\":" << num(ctx.deviceInputMs)
          << ",\"deviceOutputMs\":" << num(ctx.deviceOutputMs)
          << ",\"totalRoundTripMs\":" << num(result.medianMs + ctx.deviceInputMs + ctx.deviceOutputMs);
    }
    o << ",\"probes\":[";
    for (std::size_t i = 0; i < result.probes.size(); ++i) {
        const ProbeResult& p = result.probes[i];
        if (i) o << ",";
        o << "{\"detected\":" << (p.detected ? "true" : "false")
          << ",\"lagMs\":" << (p.detected ? num(p.lagMs) : "null")
          << ",\"snrDb\":" << num(p.snrDb, 1) << "}";
    }
    o << "]}";
    return o.str();
}

std::string errorJson(const std::string& code, const std::string& message)
{
    return "{\"schema\":" + std::to_string(kSchemaVersion) + ",\"ok\":false,\"error\":" + str(code)
           + ",\"message\":" + str(message) + "}";
}

}  // namespace latency_probe
