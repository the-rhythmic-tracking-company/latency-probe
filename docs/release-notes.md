Measures the real audio round trip of your remote-jam app (Jamulus, JackTrip,
SonoBus, JamKazam…) for SyncTrack's Stage Bridge browser extension.

**Linux only for now** (JACK or PipeWire). Windows and macOS are coming.

## Install

Download the package for your system below, then install it:

| System | File | Command |
|---|---|---|
| Raspberry Pi OS (64-bit), Debian, Ubuntu on ARM | `latency-probe_*_arm64.deb` | `sudo apt install ./latency-probe_*_arm64.deb` |
| Debian, Ubuntu, Mint on a PC | `latency-probe_*_amd64.deb` | `sudo apt install ./latency-probe_*_amd64.deb` |
| Fedora, RHEL, Alma, Rocky on a PC | `latency-probe-*.x86_64.rpm` | `sudo dnf install ./latency-probe-*.x86_64.rpm` |
| Fedora, RHEL, Alma, Rocky on ARM | `latency-probe-*.aarch64.rpm` | `sudo dnf install ./latency-probe-*.aarch64.rpm` |

Then open the **Audio Probe** tab in the Stage Bridge popup (Stage Bridge 2.3.0
or later).

`SHA256SUMS` lists each file's checksum: `sha256sum -c SHA256SUMS --ignore-missing`.
