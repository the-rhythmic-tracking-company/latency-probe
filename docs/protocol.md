# latency-probe output protocol (schema 1)

This is the contract between `latency-probe` and its caller, normally the
SyncTrack Stage Bridge. The bridge runs the probe as a child process and
reads its stdout. There is no daemon and no socket, so there's nothing to
authenticate or keep alive.

## Invocation

```
latency-probe list
latency-probe measure [--target NAME | --self-test] [--count N] [--interval SEC]
                      [--max-latency MS] [--level DBFS] [--min-snr DB] [--timeout SEC]
latency-probe --version
```

- **stdout**: exactly one line of JSON, for success and for every error.
- **stderr**: free-form diagnostics (including libjack's own messages). Callers should log it, not parse it.
- **Duration**: `measure` takes about `0.25 + (count - 1) * interval + max-latency + 0.2` seconds (about 5 s with the defaults), plus JACK startup.

Every document has `"schema": 1` and `"ok": true|false`. A caller should
reject a schema it doesn't know rather than guess.

## `measure` — success

```json
{
  "schema": 1, "ok": true,
  "method": "loopback", "backend": "jack", "target": "Jamulus",
  "sampleRate": 48000, "bufferFrames": 128,
  "requestedProbes": 5, "validProbes": 5,
  "appRoundTripMs": 42.500,
  "spreadMs": 0.208,
  "deviceInputMs": 2.667,
  "deviceOutputMs": 5.333,
  "totalRoundTripMs": 50.500,
  "probes": [ { "detected": true, "silent": false, "lagMs": 42.500, "snrDb": 38.1 } ]
}
```

| Field | Meaning |
|---|---|
| `appRoundTripMs` | Median time from the app's input port to the probe reappearing on its output port: the app's buffering, codec, network and the server or peer, both ways. |
| `spreadMs` | Max minus min over the detected probes. A large spread means the jitter buffer was moving during the measurement. |
| `deviceInputMs` / `deviceOutputMs` | The interface's capture and playback latency as reported by the driver (JACK port latency ranges). This is the part a software loopback can't observe. |
| `totalRoundTripMs` | `appRoundTripMs + deviceInputMs + deviceOutputMs`: sound in at the musician's interface to sound out of it. |
| `probes[].snrDb` | Detection margin. Values just above the threshold (18 dB by default) are worth treating with caution. |

The bridge decides how to map this onto SyncTrack's offset; for
`latencySource: 'loopback'` the natural one-way estimate is
`totalRoundTripMs / 2`, mirroring the existing `rtt/2` path.

## `measure` — probe not found (exit 2)

Same fields as success minus the latency figures, with one of two `error`
codes so the caller can tell the musician what to check:

| `error` | What came back | Usual cause |
|---|---|---|
| `silence-returned` | Digital silence (below −100 dBFS RMS) for every probe | The musician's own signal isn't in their mix: own fader down, self-monitor muted, or a JackTrip hub patched without loopback (needs `-p 1` or `-p 4`) |
| `not-detected` | Audio, but no probe in it | A noise gate cutting the quiet probe, or a peer-to-peer app (SonoBus, JackTrip peer mode) that doesn't send your own signal back |

Each entry in `probes` carries `"silent": true|false`; `snrDb` is `null` for
a silent probe, since there was nothing to search.

## Errors

```json
{ "schema": 1, "ok": false, "error": "target-not-found", "message": "..." }
```

| Exit | `error` | Meaning |
|---|---|---|
| 0 | — | Success |
| 1 | `usage` | Bad arguments or measurement config |
| 2 | `silence-returned` / `not-detected` | Measurement ran; no probe came back (see above) |
| 3 | `jack-unavailable` | No JACK / PipeWire-JACK server, or it shut down mid-run |
| 4 | `target-not-found` | No matching app client; `message` lists the clients present |
| 5 | `target-ambiguous` | Several known apps running; pass `--target` |
| 6 | `failed` / `timeout` | Port registration, connection or capture failed |

## `list`

```json
{ "schema": 1, "ok": true, "clients": [
  { "name": "Jamulus", "inputs": ["Jamulus:input left", "Jamulus:input right"],
    "outputs": ["Jamulus:output left", "Jamulus:output right"] } ] }
```

The probe's own clients (`latency-probe-*`) are never listed.
