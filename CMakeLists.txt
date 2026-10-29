cmake_minimum_required(VERSION 3.16)

project(latency-probe
  VERSION 0.1.1
  DESCRIPTION "Audio-loopback latency probe for SyncTrack Stage Bridge"
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(LATENCY_PROBE_BUILD_TESTS "Build unit tests" ON)

# The JACK backend is optional so the platform-independent core (and its
# tests) can be built on machines without JACK headers — e.g. Windows, where
# the ASIO pass-through driver will link the same core.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(JACK IMPORTED_TARGET jack)
endif()
include(CMakeDependentOption)
cmake_dependent_option(LATENCY_PROBE_BUILD_JACK "Build the JACK/PipeWire client" ON
  "JACK_FOUND" OFF)

# ---------------------------------------------------------------------------
# Core: probe signal, FFT, detector, measurement timeline, result JSON.
# No platform or audio-API dependencies; the real-time entry points in
# Measurement allocate nothing and take no locks.
# ---------------------------------------------------------------------------
add_library(latency_probe_core STATIC
  core/src/fft.cpp
  core/src/probe_signal.cpp
  core/src/detector.cpp
  core/src/measurement.cpp
  core/src/result_json.cpp)
target_include_directories(latency_probe_core PUBLIC core/include)

if(LATENCY_PROBE_BUILD_JACK)
  add_executable(latency-probe
    jack/src/main.cpp
    jack/src/jack_probe.cpp)
  target_link_libraries(latency-probe PRIVATE latency_probe_core PkgConfig::JACK)
  target_compile_definitions(latency-probe PRIVATE
    LATENCY_PROBE_VERSION="${PROJECT_VERSION}")
  install(TARGETS latency-probe RUNTIME DESTINATION bin)

  # Development helper for end-to-end tests (scripts/e2e-jack.sh); not installed.
  add_executable(fake-app tools/fake_app.cpp)
  target_link_libraries(fake-app PRIVATE PkgConfig::JACK)
endif()

if(LATENCY_PROBE_BUILD_TESTS)
  enable_testing()
  add_executable(latency_probe_tests
    tests/test_main.cpp
    tests/test_fft.cpp
    tests/test_probe_signal.cpp
    tests/test_detector.cpp
    tests/test_measurement.cpp
    tests/test_result_json.cpp)
  target_link_libraries(latency_probe_tests PRIVATE latency_probe_core)
  add_test(NAME latency_probe_tests COMMAND latency_probe_tests)
endif()

# ---------------------------------------------------------------------------
# Packaging: .deb for amd64 / arm64. Built inside ubuntu:22.04 (glibc 2.35)
# so one package runs on Pi OS bookworm+, Debian 12+, Ubuntu 22.04+ and DGX OS.
# ---------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "latency-probe")
set(CPACK_PACKAGE_VENDOR "The Rhythmic Tracking Company")
set(CPACK_PACKAGE_CONTACT "nathan@artisticnetwork.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
# Same alternatives Jamulus's own .deb uses. On PipeWire systems
# (pipewire-jack) the jackd2 library is still installed and PipeWire's
# libjack is substituted at load time, so this resolves either way.
set(CPACK_DEBIAN_PACKAGE_DEPENDS
  "libc6 (>= 2.35), libstdc++6 (>= 11), libjack-jackd2-0 | libjack-0.125")
include(CPack)
