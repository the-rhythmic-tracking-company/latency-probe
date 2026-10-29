#!/usr/bin/env bash
# End-to-end check of the JACK backend against a real jackd (dummy driver,
# no sound card) and tools/fake_app.cpp standing in for Jamulus.
#
# Runs inside the build container after scripts/build-linux.sh:
#   docker run --rm --shm-size=256m -v "$PWD:/src" -w /src latency-probe-build:amd64 \
#     bash scripts/e2e-jack.sh build/linux-amd64
#
# --shm-size matters: jackd's shared-memory segments exceed Docker's 64 MB
# default and it dies with a bus error.
set -euo pipefail

build="${1:?usage: e2e-jack.sh BUILD_DIR}"
probe="${build}/latency-probe"
fake="${build}/fake-app"

if ! command -v jackd >/dev/null; then
  apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq --no-install-recommends jackd2 >/dev/null
fi

export JACK_NO_AUDIO_RESERVATION=1
jackd --no-realtime -d dummy -r 48000 -p 128 >/tmp/jackd.log 2>&1 &
jackd_pid=$!
trap 'kill $(jobs -p) 2>/dev/null || true' EXIT
for _ in $(seq 50); do jack_lsp >/dev/null 2>&1 && break; sleep 0.1; done
if ! jack_lsp >/dev/null 2>&1; then
  echo "FAIL jackd did not start:"; cat /tmp/jackd.log; exit 1
fi

fail=0
check() {  # name, expected-exit, json, jq-free grep pattern
  local name="$1" want="$2" got="$3" json="$4" pattern="$5"
  if [ "$got" = "$want" ] && grep -q -- "$pattern" <<<"$json"; then
    echo "ok   ${name}"
  else
    echo "FAIL ${name} (exit ${got}, wanted ${want}): ${json}"
    fail=1
  fi
}

set +e
json=$("$probe" measure --self-test); rc=$?
check "self-test reports ~0 ms" 0 "$rc" "$json" '"appRoundTripMs":0.0'

json=$("$probe" measure); rc=$?
check "no app running -> target-not-found" 4 "$rc" "$json" '"error":"target-not-found"'

"$fake" Jamulus 42.5 & sleep 0.5
json=$("$probe" list); rc=$?
check "list shows the fake app" 0 "$rc" "$json" '"name":"Jamulus"'

json=$("$probe" measure --count 3); rc=$?
check "auto-detected Jamulus, 42.5 ms round trip" 0 "$rc" "$json" '"appRoundTripMs":42.5'
echo "     ${json}"

"$fake" SonoBus 10 & sleep 0.5
json=$("$probe" measure --count 1); rc=$?
check "two known apps -> target-ambiguous" 5 "$rc" "$json" '"error":"target-ambiguous"'

json=$("$probe" measure --target SonoBus --count 2); rc=$?
check "explicit --target SonoBus, 10 ms" 0 "$rc" "$json" '"appRoundTripMs":10.0'

json=$("$probe" measure --target SonoBus --count 1 --level -200); rc=$?
check "absurd probe level -> usage error" 1 "$rc" "$json" '"error":"usage"'

"$fake" JackTrip mute & sleep 0.5
json=$("$probe" measure --target JackTrip --count 2); rc=$?
check "app that returns silence -> silence-returned" 2 "$rc" "$json" '"error":"silence-returned"'
set -e

kill "$jackd_pid"
exit "$fail"
