#!/usr/bin/env bash
# Builds, tests and packages latency-probe for Linux inside Docker.
#
#   scripts/build-linux.sh            # amd64 and arm64
#   scripts/build-linux.sh arm64      # one architecture
#
# arm64 runs under QEMU emulation on x86 hosts (Docker Desktop includes it),
# so it is slower but needs no cross-toolchain. Output: build/linux-<arch>/*.deb
set -euo pipefail

cd "$(dirname "$0")/.."
if [ $# -gt 0 ]; then archs=("$@"); else archs=(amd64 arm64); fi

# Git Bash on Windows rewrites /src-style paths; give Docker a Windows path.
export MSYS_NO_PATHCONV=1
src="$(pwd -W 2>/dev/null || pwd)"

for arch in "${archs[@]}"; do
  image="latency-probe-build:${arch}"
  echo "==> ${arch}: build image"
  docker build --platform "linux/${arch}" -t "${image}" -f docker/Dockerfile.linux docker

  echo "==> ${arch}: configure, build, test, package"
  docker run --rm --platform "linux/${arch}" -v "${src}:/src" -w /src "${image}" bash -euc "
    cmake -S . -B build/linux-${arch} -DCMAKE_BUILD_TYPE=Release
    cmake --build build/linux-${arch} -j\$(nproc)
    (cd build/linux-${arch} && ctest --output-on-failure && cpack -G DEB)
  "
done

ls -l build/linux-*/*.deb
