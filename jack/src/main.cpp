// latency-probe: measures an app's audio round trip by sending a quiet sweep
// into its JACK inputs and timing its return on the app's outputs.
//
// Prints exactly one JSON line on stdout (see docs/protocol.md); diagnostics
// go to stderr.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "jack_probe.h"
#include "latency_probe/result_json.h"

using namespace latency_probe;

namespace {

void printUsage()
{
    std::cerr <<
        "Usage:\n"
        "  latency-probe list\n"
        "  latency-probe measure [options]\n"
        "  latency-probe --version\n"
        "\n"
        "Measure options:\n"
        "  --target NAME       JACK client to probe (default: auto-detect Jamulus/JackTrip/SonoBus)\n"
        "  --self-test         Loop the probe straight back to itself (expects ~0 ms)\n"
        "  --count N           Number of probes (default 5)\n"
        "  --interval SEC      Probe spacing (default 1.0)\n"
        "  --max-latency MS    Longest round trip to search for (default 700)\n"
        "  --level DBFS        Probe peak level (default -30)\n"
        "  --min-snr DB        Detection threshold (default 18)\n"
        "  --timeout SEC       Give up after this long (default: capture length + 3 s)\n";
}

bool parseNumber(const char* text, double& out)
{
    char* end = nullptr;
    out = std::strtod(text, &end);
    return end != text && *end == '\0';
}

int emit(const jack::Outcome& outcome)
{
    std::cout << outcome.json << std::endl;
    return outcome.exitCode;
}

int usageError(const std::string& message)
{
    printUsage();
    return emit({errorJson("usage", message), jack::kUsage});
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2) return usageError("missing command");
    const std::string command = argv[1];

    if (command == "--version") {
        std::cout << LATENCY_PROBE_VERSION << std::endl;
        return 0;
    }
    if (command == "--help" || command == "-h") {
        printUsage();
        return 0;
    }
    if (command == "list") return emit(jack::listClients());
    if (command != "measure") return usageError("unknown command: " + command);

    jack::MeasureOptions options;
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--self-test") {
            options.selfTest = true;
            continue;
        }
        if (i + 1 >= argc) return usageError("missing value for " + arg);
        const char* value = argv[++i];
        if (arg == "--target") {
            options.target = value;
            continue;
        }

        double number = 0.0;
        if (!parseNumber(value, number)) return usageError("not a number for " + arg + ": " + value);
        if (arg == "--count") options.config.probeCount = static_cast<int>(number);
        else if (arg == "--interval") options.config.intervalSec = number;
        else if (arg == "--max-latency") options.config.maxLatencySec = number / 1000.0;
        else if (arg == "--level") options.config.levelDbfs = number;
        else if (arg == "--min-snr") options.config.minSnrDb = number;
        else if (arg == "--timeout") options.timeoutSec = number;
        else return usageError("unknown option: " + arg);
    }
    if (options.selfTest && !options.target.empty()) return usageError("--self-test and --target are exclusive");

    return emit(jack::measure(options));
}
