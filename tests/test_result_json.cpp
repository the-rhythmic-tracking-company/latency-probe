#include <string>

#include "latency_probe/result_json.h"
#include "test_harness.h"

using namespace latency_probe;

namespace {

bool contains(const std::string& haystack, const std::string& needle)
{
    return haystack.find(needle) != std::string::npos;
}

}  // namespace

TEST("Given control characters and quotes, when escaping, then the JSON string is valid")
{
    CHECK(jsonEscape("a\"b\\c\n\x01") == "a\\\"b\\\\c\\n\\u0001");
}

TEST("Given a successful measurement, when reporting, then totals include device latency")
{
    ReportContext ctx;
    ctx.backend = "jack";
    ctx.target = "Jamulus";
    ctx.sampleRate = 48000;
    ctx.bufferFrames = 128;
    ctx.deviceInputMs = 2.5;
    ctx.deviceOutputMs = 3.0;
    MeasurementResult r;
    r.validCount = 1;
    r.medianMs = 40.0;
    r.probes.push_back({true, 40.0, 30.0});
    r.probes.push_back({false, 0.0, 9.0});

    const std::string json = measurementJson(ctx, 2, r);
    CHECK(contains(json, "\"schema\":1,\"ok\":true"));
    CHECK(contains(json, "\"appRoundTripMs\":40.000"));
    CHECK(contains(json, "\"totalRoundTripMs\":45.500"));
    CHECK(contains(json, "{\"detected\":false,\"lagMs\":null,\"snrDb\":9.0}"));
}

TEST("Given no detections, when reporting, then ok is false with error not-detected")
{
    ReportContext ctx;
    MeasurementResult r;
    const std::string json = measurementJson(ctx, 5, r);
    CHECK(contains(json, "\"ok\":false,\"error\":\"not-detected\""));
    CHECK(!contains(json, "totalRoundTripMs"));
}

TEST("Given an error, when reporting, then code and message are present")
{
    CHECK(errorJson("target-not-found", "x") == "{\"schema\":1,\"ok\":false,\"error\":\"target-not-found\",\"message\":\"x\"}");
}
