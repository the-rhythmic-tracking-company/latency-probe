// Development helper, not installed: a JACK client that stands in for a
// remote-jam app by playing its input back on its output after a fixed delay.
// Lets the JACK path be tested end to end without Jamulus or a network.
//
//   fake-app NAME DELAY_MS        e.g. fake-app Jamulus 42.5
//   fake-app NAME mute            outputs silence (an app that doesn't
//                                 send your own signal back)

#include <jack/jack.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

struct State {
    jack_port_t* in = nullptr;
    jack_port_t* out = nullptr;
    std::vector<float> ring;
    std::size_t pos = 0;
    bool mute = false;
};

std::atomic<bool> running{true};

int process(jack_nframes_t nframes, void* arg)
{
    auto* s = static_cast<State*>(arg);
    const auto* in = static_cast<const float*>(jack_port_get_buffer(s->in, nframes));
    auto* out = static_cast<float*>(jack_port_get_buffer(s->out, nframes));
    for (jack_nframes_t i = 0; i < nframes; ++i) {
        out[i] = s->mute ? 0.0f : s->ring[s->pos];
        s->ring[s->pos] = in[i];
        s->pos = (s->pos + 1) % s->ring.size();
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc != 3) {
        std::cerr << "usage: fake-app NAME DELAY_MS\n";
        return 1;
    }
    jack_client_t* client = jack_client_open(argv[1], JackNoStartServer, nullptr);
    if (!client) {
        std::cerr << "fake-app: no JACK server\n";
        return 3;
    }

    State s;
    s.mute = std::string(argv[2]) == "mute";
    const double delayFrames = std::atof(argv[2]) * jack_get_sample_rate(client) / 1000.0;
    s.ring.assign(static_cast<std::size_t>(delayFrames) > 0 ? static_cast<std::size_t>(delayFrames) : 1, 0.0f);
    s.in = jack_port_register(client, "input", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    s.out = jack_port_register(client, "output", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    jack_set_process_callback(client, process, &s);
    std::signal(SIGINT, [](int) { running = false; });
    std::signal(SIGTERM, [](int) { running = false; });
    if (jack_activate(client) != 0) return 6;

    std::cerr << "fake-app: '" << jack_get_client_name(client) << "' delaying " << s.ring.size() << " frames\n";
    while (running) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    jack_client_close(client);
    return 0;
}
