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

## Install (Linux)

Download the package for your system from the
[latest release](https://github.com/the-rhythmic-tracking-company/latency-probe/releases/latest)
and install it with your package manager:

```
sudo apt install ./latency-probe_*_arm64.deb        # Raspberry Pi OS 64-bit, Debian/Ubuntu on ARM
sudo apt install ./latency-probe_*_amd64.deb        # Debian, Ubuntu, Mint on a PC
sudo dnf install ./latency-probe-*.x86_64.rpm       # Fedora, RHEL/Alma/Rocky 9+ on a PC
sudo dnf install ./latency-probe-*.aarch64.rpm      # the same on ARM
```

The release page is the single source for every package; getting into the
distributions' own repositories (apt/PPA, Fedora COPR) comes later and will
build from it.

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

Linux packages are built in Docker for both architectures; the only
prerequisite is Docker (Docker Desktop on Windows/macOS, which also provides
the QEMU emulation used for arm64 on x86 hosts). Run everything from the repo
root.

### With the build script (Linux, macOS, Git Bash on Windows)

```
scripts/build-linux.sh            # amd64 + arm64 → build/linux-<arch>/latency-probe_*.deb
scripts/build-linux.sh amd64      # one architecture (arm64 is emulated on x86 hosts)
FORMAT=rpm scripts/build-linux.sh # .rpm instead → build/rpm-<arch>/latency-probe-*.rpm
FORMAT=all scripts/build-linux.sh # both
```

`.rpm` builds use `docker/Dockerfile.rpm` (AlmaLinux 9, glibc 2.34, PipeWire's
JACK headers), so they install on RHEL/Alma/Rocky 9+ and Fedora.

For each architecture the script:

1. builds the `latency-probe-build:<arch>` image from `docker/Dockerfile.linux`
   (Ubuntu 22.04 + toolchain + `libjack-jackd2-dev`; cached after the first run),
2. configures and compiles into `build/linux-<arch>`,
3. runs the unit tests (`ctest`), and
4. packages the `.deb` (`cpack`).

From PowerShell, call Git Bash explicitly — a bare `bash` may resolve to WSL,
where the script's Windows path handling doesn't apply:

```
& "C:\Program Files\Git\bin\bash.exe" scripts/build-linux.sh amd64
```

### Without the script (PowerShell)

The same steps by hand; for arm64 replace every `amd64`:

```
docker build --platform linux/amd64 -t latency-probe-build:amd64 -f docker/Dockerfile.linux docker
docker run --rm --platform linux/amd64 -v "${PWD}:/src" -w /src latency-probe-build:amd64 bash -euc 'cmake -S . -B build/linux-amd64 -DCMAKE_BUILD_TYPE=Release && cmake --build build/linux-amd64 -j$(nproc) && cd build/linux-amd64 && ctest --output-on-failure && cpack -G DEB'
```

Keep the single quotes so PowerShell doesn't expand `$(nproc)` itself.

### End-to-end test

Runs a real `jackd` (dummy driver) and a fake app with a known delay against
the built binaries. Needs the amd64 image and build from above:

```
# Linux / macOS
docker run --rm --shm-size=256m -v "$PWD:/src" -w /src latency-probe-build:amd64 \
  bash scripts/e2e-jack.sh build/linux-amd64

# Git Bash on Windows (stops MSYS rewriting /src and gives Docker a Windows path)
MSYS_NO_PATHCONV=1 docker run --rm --shm-size=256m -v "$(pwd -W):/src" -w /src \
  latency-probe-build:amd64 bash scripts/e2e-jack.sh build/linux-amd64

# PowerShell
docker run --rm --shm-size=256m -v "${PWD}:/src" -w /src latency-probe-build:amd64 bash scripts/e2e-jack.sh build/linux-amd64
```

### Releases

`.github/workflows/build.yml` builds and tests all four packages (deb/rpm ×
amd64/arm64, natively on GitHub's x64 and ARM runners) on every push and pull
request. To release, bump `VERSION` in `CMakeLists.txt`, commit, and push a
matching tag:

```
git tag v0.1.1 && git push origin v0.1.1
```

The workflow refuses a tag that doesn't match the CMake version, then
publishes a GitHub Release with the packages, `SHA256SUMS` and
`docs/release-notes.md` as its text.

### Native build

Needs CMake ≥ 3.16, a C++17 compiler, and `libjack-jackd2-dev`
for the JACK backend; without it only the core and tests are built):

```
cmake -S . -B build && cmake --build build && (cd build && ctest)
```

### Targets

| Target | Build |
|---|---|
| Linux x86-64 (Intel and AMD) | `.deb` built on Ubuntu 22.04 (glibc 2.35); `.rpm` built on AlmaLinux 9 (glibc 2.34) |
| Linux ARM64 (Pi 4/5 on 64-bit Pi OS; NVIDIA Grace / DGX OS) | `.deb` and `.rpm`, same bases |
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
