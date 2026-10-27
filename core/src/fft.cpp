#include "latency_probe/fft.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace latency_probe {

std::size_t nextPowerOfTwo(std::size_t n)
{
    std::size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

void fft(std::vector<std::complex<double>>& data, bool inverse)
{
    const std::size_t n = data.size();
    if (n == 0 || (n & (n - 1)) != 0)
        throw std::invalid_argument("fft: size must be a power of two");

    // Bit-reversal permutation.
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(data[i], data[j]);
    }

    const double pi = std::acos(-1.0);
    for (std::size_t len = 2; len <= n; len <<= 1) {
        const double angle = 2.0 * pi / static_cast<double>(len) * (inverse ? 1.0 : -1.0);
        const std::complex<double> wLen(std::cos(angle), std::sin(angle));
        for (std::size_t start = 0; start < n; start += len) {
            std::complex<double> w(1.0, 0.0);
            for (std::size_t k = 0; k < len / 2; ++k) {
                const std::complex<double> u = data[start + k];
                const std::complex<double> v = data[start + k + len / 2] * w;
                data[start + k]           = u + v;
                data[start + k + len / 2] = u - v;
                w *= wLen;
            }
        }
    }

    if (inverse) {
        const double scale = 1.0 / static_cast<double>(n);
        for (auto& x : data) x *= scale;
    }
}

}  // namespace latency_probe
