#pragma once

#include <complex>
#include <vector>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace core::dsp {

    // MSVC-safe pi
    inline constexpr float kPi = std::numbers::pi_v<float>;

    // In-place radix-2 Cooley-Tukey FFT
    // - size must be power of 2
    // - forward FFT (no 1/N normalization)
    inline void FftRadix2(std::vector<std::complex<float>>& a) {
        const std::size_t n = a.size();
        if (n <= 1) return;

        // Bit-reversal permutation
        for (std::size_t i = 1, j = 0; i < n; ++i) {
            std::size_t bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap(a[i], a[j]);
        }

        // Iterative FFT
        for (std::size_t len = 2; len <= n; len <<= 1) {
            const float ang = -2.0f * kPi / static_cast<float>(len);
            const std::complex<float> wlen(std::cos(ang), std::sin(ang));

            for (std::size_t i = 0; i < n; i += len) {
                std::complex<float> w(1.0f, 0.0f);
                const std::size_t half = len >> 1;

                for (std::size_t j2 = 0; j2 < half; ++j2) {
                    const auto u = a[i + j2];
                    const auto v = a[i + j2 + half] * w;
                    a[i + j2] = u + v;
                    a[i + j2 + half] = u - v;
                    w *= wlen;
                }
            }
        }
    }

}  // namespace core::dsp
