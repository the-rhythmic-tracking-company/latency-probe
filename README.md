# latency-probe

Measures the real audio round trip of a remote-jam app (Jamulus, JackTrip,
SonoBus, JamKazam…) by sending a quiet sweep into the app's input and timing
its return on the app's output. It feeds that number to SyncTrack through the
Stage Bridge, alongside the latency the bridge already reads from Jamulus and
Zoom.

Because it works at the audio-driver level, it covers apps that have no
latency API at all.

**Status:** early. The JACK/PipeWire client works and is tested end to end
against a real `jackd`. The ASIO pass-through driver (Windows) is next.

## How it works

1. A 0.2 s exponential sine sweep at −30 dBFS is mixed into the app's input
   ports, five times, one second apart.
2. Whatever the app plays back is captured on the same sample timeline.
3. A matched filter (FFT cross-correlation) finds each sweep's return to a
   fraction of a sample. Timing is by sample count, never wall clock, so
   analysis time doesn't affect the result, and the analysis itself takes a
   few milliseconds even on a Raspberry Pi.
4. The median lag, plus the interface latency the driver reports, is printed
   as one line of JSON (see [docs/protocol.md](docs/protocol.md)).

The app must send your own signal back to you. Server-mix apps do (Jamulus,
JamKazam, a JackTrip hub patched with `-p 1` or `-p 4`); peer-to-peer apps
need the far end to send it back.

### Backends

| OS | Backend | How it reaches the app | State |
|---|---|---|---|
| Linux (incl. Raspberry Pi) | JACK / PipeWire-JACK client | Connects alongside the interface; JACK sums inputs, so nothing is rerouted | Working |
| Windows | Pass-through ASIO driver | The app selects "SyncTrack Probe", which wraps the real ASIO driver | Planned |
| macOS | Core Audio server plug-in | Virtual device in front of the interface | Later |

On JACK the probe uses two clients, `latency-probe-out` → app →
`latency-probe-in`, so the graph has no cycle and the processing order
within a cycle is fixed.

## Usage (Linux)

```
latency-probe list                     # JACK clients and their ports
latency-probe measure                  # auto-detects Jamulus / JackTrip / SonoBus
latency-probe measure --target Jamulus --count 7
latency-probe measure --self-test      # sanity check, expect ~0 ms
```

On PipeWire systems, run it from a session where JACK clients already work
(e.g. where Jamulus runs). If not, `pw-jack latency-probe measure`.

## Building

Linux packages are built in Docker for both architectures; no local toolchain
is needed:

```
scripts/build-linux.sh            # amd64 + arm64 → build/linux-<arch>/latency-probe_*.deb
scripts/build-linux.sh arm64      # one architecture (emulated on x86 hosts)
```

End-to-end test against a real `jackd` (dummy driver) and a fake app with a
known delay:

```
docker run --rm --shm-size=256m -v "$PWD:/src" -w /src latency-probe-build:amd64 \
  bash scripts/e2e-jack.sh build/linux-amd64
```

Native build (needs CMake ≥ 3.16, a C++17 compiler, and `libjack-jackd2-dev`
for the JACK backend; without it only the core and tests are built):

```
cmake -S . -B build && cmake --build build && (cd build && ctest)
```

### Targets

| Target | Build |
|---|---|
| Linux x86-64 (Intel and AMD) | `.deb`, built on Ubuntu 22.04 (glibc 2.35) |
| Linux ARM64 (Pi 4/5 on 64-bit Pi OS; NVIDIA Grace / DGX OS) | `.deb`, same base |
| Windows x64 | ASIO driver — planned |
| Windows ARM64, macOS universal | Later |

## Layout

```
core/     platform-independent: probe signal, FFT, detector, measurement timeline, JSON
jack/     JACK/PipeWire command-line client
tools/    development helpers (fake-app for end-to-end tests; not installed)
tests/    unit tests (dependency-free harness)
docs/     output protocol consumed by the Stage Bridge
docker/   Linux build image
scripts/  build and end-to-end scripts
```

`core/` must stay free of platform and audio-API dependencies: the ASIO driver
will link the same library. Its `render()`/`capture()` entry points run on
real-time audio threads and must never allocate, lock, or block.

## License

GPL-3.0-only (see [LICENSE](LICENSE)). The Windows driver will use the ASIO SDK
under its GPLv3 option, so it ships without a separate Steinberg
agreement; "ASIO" naming follows Steinberg's trademark guidelines.
