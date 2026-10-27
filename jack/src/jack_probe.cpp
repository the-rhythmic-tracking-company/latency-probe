#include "jack_probe.h"

#include <jack/jack.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <map>
#include <sstream>
#include <thread>

#include "latency_probe/result_json.h"

namespace latency_probe::jack {

namespace {

// Our own clients. Two of them, not one: a single client that both feeds the
// app and listens to it would form a cycle in the JACK graph, and JACK would
// break it at an arbitrary point — up to one period of error. With
// probe-out -> app -> probe-in the order within a cycle is fixed, which is
// what Measurement's timeline relies on.
constexpr const char* kOutClientName = "latency-probe-out";
constexpr const char* kInClientName = "latency-probe-in";
constexpr const char* kOwnClientPrefix = "latency-probe";

/// Owns a jack_client_t and closes it on scope exit.
class Client {
public:
    explicit Client(const char* name)
    {
        jack_status_t status;
        handle_ = jack_client_open(name, JackNoStartServer, &status);
    }
    ~Client()
    {
        if (handle_) {
            if (active_) jack_deactivate(handle_);
            jack_client_close(handle_);
        }
    }
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    jack_client_t* get() const { return handle_; }
    explicit operator bool() const { return handle_ != nullptr; }
    bool activate()
    {
        active_ = jack_activate(handle_) == 0;
        return active_;
    }
    void deactivate()
    {
        if (active_) jack_deactivate(handle_);
        active_ = false;
    }

private:
    jack_client_t* handle_ = nullptr;
    bool active_ = false;
};

struct RealtimeShared {
    Measurement measurement;
    jack_client_t* outClient = nullptr;
    jack_client_t* inClient = nullptr;
    jack_port_t* outPort = nullptr;
    jack_port_t* inPort = nullptr;
    std::atomic<bool> shutdown{false};
};

int processOut(jack_nframes_t nframes, void* arg)
{
    auto* s = static_cast<RealtimeShared*>(arg);
    auto* buf = static_cast<float*>(jack_port_get_buffer(s->outPort, nframes));
    s->measurement.render(jack_last_frame_time(s->outClient), buf, nframes);
    return 0;
}

int processIn(jack_nframes_t nframes, void* arg)
{
    auto* s = static_cast<RealtimeShared*>(arg);
    const auto* buf = static_cast<const float*>(jack_port_get_buffer(s->inPort, nframes));
    s->measurement.capture(jack_last_frame_time(s->inClient), buf, nframes);
    return 0;
}

void onShutdown(void* arg)
{
    static_cast<RealtimeShared*>(arg)->shutdown.store(true);
}

std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool startsWithIgnoreCase(const std::string& s, const std::string& prefix)
{
    return lower(s.substr(0, prefix.size())) == lower(prefix);
}

/// Every client with audio ports, except our own.
std::vector<ClientPorts> enumerate(jack_client_t* client)
{
    std::map<std::string, ClientPorts> byName;
    const char** names = jack_get_ports(client, nullptr, JACK_DEFAULT_AUDIO_TYPE, 0);
    if (!names) return {};
    for (const char** p = names; *p; ++p) {
        const std::string full = *p;
        const auto colon = full.find(':');
        if (colon == std::string::npos) continue;
        const std::string owner = full.substr(0, colon);
        if (owner.rfind(kOwnClientPrefix, 0) == 0) continue;

        jack_port_t* port = jack_port_by_name(client, *p);
        if (!port) continue;
        ClientPorts& entry = byName[owner];
        entry.name = owner;
        if (jack_port_flags(port) & JackPortIsInput)
            entry.inputs.push_back(full);
        else
            entry.outputs.push_back(full);
    }
    jack_free(names);

    std::vector<ClientPorts> out;
    for (auto& kv : byName) out.push_back(std::move(kv.second));
    return out;
}

std::string jsonStringArray(const std::vector<std::string>& items)
{
    std::string out = "[";
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i) out += ",";
        out += "\"" + jsonEscape(items[i]) + "\"";
    }
    return out + "]";
}

std::string clientNames(const std::vector<ClientPorts>& clients)
{
    std::string out;
    for (const auto& c : clients) out += (out.empty() ? "" : ", ") + c.name;
    return out.empty() ? "(none)" : out;
}

/// Worst-case latency, in frames, across `ports` in the given direction.
jack_nframes_t maxLatency(jack_client_t* client, const std::vector<std::string>& ports,
                          jack_latency_callback_mode_t mode)
{
    jack_nframes_t worst = 0;
    for (const auto& name : ports) {
        jack_port_t* port = jack_port_by_name(client, name.c_str());
        if (!port) continue;
        jack_latency_range_t range;
        jack_port_get_latency_range(port, mode, &range);
        worst = std::max(worst, range.max);
    }
    return worst;
}

bool connectPorts(jack_client_t* client, const std::string& from, const std::string& to)
{
    const int rc = jack_connect(client, from.c_str(), to.c_str());
    return rc == 0 || rc == EEXIST;
}

}  // namespace

Outcome listClients()
{
    Client client(kOutClientName);
    if (!client) return {errorJson("jack-unavailable", "Could not connect to a JACK or PipeWire-JACK server"), kJackUnavailable};

    std::ostringstream o;
    o << "{\"schema\":" << kSchemaVersion << ",\"ok\":true,\"clients\":[";
    const auto clients = enumerate(client.get());
    for (std::size_t i = 0; i < clients.size(); ++i) {
        if (i) o << ",";
        o << "{\"name\":\"" << jsonEscape(clients[i].name) << "\""
          << ",\"inputs\":" << jsonStringArray(clients[i].inputs)
          << ",\"outputs\":" << jsonStringArray(clients[i].outputs) << "}";
    }
    o << "]}";
    return {o.str(), kOk};
}

Outcome measure(const MeasureOptions& options)
{
    RealtimeShared shared;

    Client out(kOutClientName);
    if (!out) return {errorJson("jack-unavailable", "Could not connect to a JACK or PipeWire-JACK server"), kJackUnavailable};

    ClientPorts target;
    if (!options.selfTest) {
        const auto clients = enumerate(out.get());
        std::vector<ClientPorts> matches;
        for (const auto& c : clients) {
            if (c.inputs.empty() || c.outputs.empty()) continue;
            if (!options.target.empty()) {
                if (c.name == options.target) matches.push_back(c);
            } else {
                for (const auto& known : kKnownTargets)
                    if (startsWithIgnoreCase(c.name, known)) matches.push_back(c);
            }
        }
        if (matches.empty()) {
            const std::string wanted = options.target.empty() ? "a known app (Jamulus, JackTrip, SonoBus)" : "'" + options.target + "'";
            return {errorJson("target-not-found", "No JACK client matching " + wanted + " with both inputs and outputs. Clients: " + clientNames(clients)), kTargetNotFound};
        }
        if (matches.size() > 1)
            return {errorJson("target-ambiguous", "Several candidate apps; pass --target. Candidates: " + clientNames(matches)), kTargetAmbiguous};
        target = matches.front();
    }

    Client in(kInClientName);
    if (!in) return {errorJson("jack-unavailable", "Could not open the second JACK client"), kJackUnavailable};

    shared.outClient = out.get();
    shared.inClient = in.get();
    shared.outPort = jack_port_register(out.get(), "probe", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    shared.inPort = jack_port_register(in.get(), "capture", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    if (!shared.outPort || !shared.inPort) return {errorJson("failed", "Could not register JACK ports"), kFailed};

    jack_set_process_callback(out.get(), processOut, &shared);
    jack_set_process_callback(in.get(), processIn, &shared);
    jack_on_shutdown(out.get(), onShutdown, &shared);
    jack_on_shutdown(in.get(), onShutdown, &shared);
    if (!out.activate() || !in.activate()) return {errorJson("failed", "Could not activate JACK clients"), kFailed};

    ReportContext ctx;
    ctx.backend = "jack";
    ctx.target = options.selfTest ? "self-test" : target.name;
    ctx.sampleRate = jack_get_sample_rate(out.get());
    ctx.bufferFrames = jack_get_buffer_size(out.get());

    const std::string outName = jack_port_name(shared.outPort);
    const std::string inName = jack_port_name(shared.inPort);
    bool wired = true;
    if (options.selfTest) {
        wired = connectPorts(out.get(), outName, inName);
    } else {
        // Read device latency before adding our connections, so the
        // ranges reflect only the musician's own interface routing.
        ctx.deviceInputMs = maxLatency(out.get(), target.inputs, JackCaptureLatency) * 1000.0 / ctx.sampleRate;
        ctx.deviceOutputMs = maxLatency(out.get(), target.outputs, JackPlaybackLatency) * 1000.0 / ctx.sampleRate;
        // JACK sums every connection into an input port, so this adds the
        // probe alongside the musician's interface without rerouting it.
        for (const auto& port : target.inputs) wired = connectPorts(out.get(), outName, port) && wired;
        for (const auto& port : target.outputs) wired = connectPorts(out.get(), port, inName) && wired;
    }
    if (!wired) return {errorJson("failed", "Could not connect probe ports to " + ctx.target), kFailed};

    MeasurementConfig config = options.config;
    config.sampleRate = ctx.sampleRate;
    const std::string armError = shared.measurement.arm(config);
    if (!armError.empty()) return {errorJson("usage", armError), kUsage};

    const double captureSec = static_cast<double>(shared.measurement.captureLength()) / ctx.sampleRate;
    const double timeoutSec = options.timeoutSec > 0.0 ? options.timeoutSec : captureSec + 3.0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::duration<double>(timeoutSec);
    while (!shared.measurement.done()) {
        if (shared.shutdown.load()) return {errorJson("jack-unavailable", "JACK server shut down during measurement"), kJackUnavailable};
        if (std::chrono::steady_clock::now() > deadline) return {errorJson("timeout", "Capture did not finish; is the JACK server running?"), kFailed};
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    // Stop the callbacks before touching the capture buffer from this thread.
    in.deactivate();
    out.deactivate();

    const MeasurementResult result = shared.measurement.analyze();
    return {measurementJson(ctx, config.probeCount, result), result.validCount > 0 ? kOk : kNotDetected};
}

}  // namespace latency_probe::jack
