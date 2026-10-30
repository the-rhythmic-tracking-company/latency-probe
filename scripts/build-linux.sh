#!/usr/bin/env bash
# Builds, tests and packages latency-probe for Linux inside Docker.
#
#   scripts/build-linux.sh                 # .deb for amd64 and arm64
#   scripts/build-linux.sh arm64           # one architecture
#   FORMAT=rpm scripts/build-linux.sh      # .rpm instead (dnf / yum)
#   FORMAT=all scripts/build-linux.sh      # both
#
# arm64 runs under QEMU emulation on x86 hosts (Docker Desktop includes it),
# so it is slower but needs no cross-toolchain.
# Output: build/linux-<arch>/*.deb and build/rpm-<arch>/*.rpm
set -euo pipefail

cd "$(dirname "$0")/.."
if [ $# -gt 0 ]; then archs=("$@"); else archs=(amd64 arm64); fi
case "${FORMAT:-deb}" in
  deb) formats=(deb) ;;
  rpm) formats=(rpm) ;;
  all) formats=(deb rpm) ;;
  *) echo "FORMAT must be deb, rpm or all" >&2; exit 1 ;;
esac

# Git Bash on Windows rewrites /src-style paths; give Docker a Windows path.
export MSYS_NO_PATHCONV=1
src="$(pwd -W 2>/dev/null || pwd)"

for format in "${formats[@]}"; do
  if [ "$format" = deb ]; then
    dockerfile=docker/Dockerfile.linux; image_base=latency-probe-build; dir_base=linux; generator=DEB
  else
    dockerfile=docker/Dockerfile.rpm;   image_base=latency-probe-rpm;   dir_base=rpm;   generator=RPM
  fi

  for arch in "${archs[@]}"; do
    image="${image_base}:${arch}"
    build="build/${dir_base}-${arch}"
    echo "==> ${format} ${arch}: build image"
    docker build --platform "linux/${arch}" -t "${image}" -f "${dockerfile}" docker

    echo "==> ${format} ${arch}: configure, build, test, package"
    docker run --rm --platform "linux/${arch}" -v "${src}:/src" -w /src "${image}" bash -euc "
      cmake -S . -B ${build} -DCMAKE_BUILD_TYPE=Release
      cmake --build ${build} -j\$(nproc)
      (cd ${build} && ctest --output-on-failure && cpack -G ${generator})
    "
  done
done

ls -l build/linux-*/*.deb build/rpm-*/*.rpm 2>/dev/null || true
