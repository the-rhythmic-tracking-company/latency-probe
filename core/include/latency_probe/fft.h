#pragma once

#include <complex>
#include <cstddef>
#include <vector>

namespace latency_probe {

/// Smallest power of two >= n (returns 1 for n == 0).
std::size_t nextPowerOfTwo(std::size_t n);

/// In-place iterative radix-2 FFT. data.size() must be a power of two.
/// The inverse transform is scaled by 1/N, so fft(fft(x), true) == x.
void fft(std::vector<std::complex<double>>& data, bool inverse);

}  // namespace latency_probe
