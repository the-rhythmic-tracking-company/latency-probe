#pragma once

#include <string>
#include <vector>

#include "latency_probe/measurement.h"

namespace latency_probe::jack {

/// Apps the probe looks for when no --target is given, matched as a
/// case-insensitive prefix of the JACK client name (Jamulus appends the
/// --clientname suffix; JackTrip hub clients get per-peer names).
inline const std::vector<std::string> kKnownTargets = {"Jamulus", "JackTrip", "SonoBus"};

struct ClientPorts {
    std::string name;
    std::vector<std::string> inputs;   ///< Ports the client receives on (we inject here)
    std::vector<std::string> outputs;  ///< Ports the client sends on (we listen here)
};

struct MeasureOptions {
    std::string target;     ///< Exact JACK client name; empty = auto-detect
    bool selfTest = false;  ///< Wire probe-out straight to probe-in (expects ~0 ms)
    MeasurementConfig config;
    double timeoutSec = 0.0;  ///< 0 = derived from the capture length
};

/// Result of a command: the JSON line for stdout and the process exit code.
struct Outcome {
    std::string json;
    int exitCode = 0;
};

/// Exit codes, documented in docs/protocol.md.
enum ExitCode : int {
    kOk = 0,
    kUsage = 1,
    kNotDetected = 2,
    kJackUnavailable = 3,
    kTargetNotFound = 4,
    kTargetAmbiguous = 5,
    kFailed = 6,
};

Outcome listClients();
Outcome measure(const MeasureOptions& options);

}  // namespace latency_probe::jack
