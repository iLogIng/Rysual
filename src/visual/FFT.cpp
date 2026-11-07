#include "visual/FFT.hpp"

#include <cmath>
#include <complex>
#include <numbers>
#include <stdexcept>

namespace rysual
{

FFT::FFT(size_t N) {
    m_N = N;
    if (N == 0 || (N & (N - 1)) != 0) {
        throw std::invalid_argument("FFT size must be a power of 2");
    }
    m_log2N = static_cast<size_t>(std::log2(N));
    initBitRev();
    initTwiddle();
}

void FFT::initBitRev() {
    m_bit_rev.resize(m_N);
    for (size_t i = 0; i < m_N; ++i) {
        size_t rev = 0;
        size_t temp = i;
        for (size_t j = 0; j < m_log2N; ++j) {
            rev = (rev << 1) | (temp & 1);
            temp >>= 1;
        }
        m_bit_rev[i] = rev;
    }
}

void FFT::initTwiddle() {
    m_twiddle.resize(m_N / 2);
    for (size_t i = 0; i < m_N / 2; ++i) {
        const float angle =
            -2.f * std::numbers::pi_v<float>
            * static_cast<float>(i) / static_cast<float>(m_N);
        m_twiddle[i] = std::complex<float>(std::cos(angle), std::sin(angle));
    }
}

void FFT::forward(const float *input, std::vector<std::complex<float>>& output) const {
    std::vector<std::complex<float>> data(m_N);

    for (size_t i = 0; i < m_N; ++i) {
        data[m_bit_rev[i]] = std::complex<float>(input[i], 0.f);
    }

    for (size_t stage = 1; stage <= m_log2N; ++stage) {
        size_t m = 1 << stage;          // 当前层的节点数 (2, 4, 8...)
        size_t m2 = m >> 1;             // 当前层的一半 (1, 2, 4...)
        size_t twiddle_step = m_N / m;  // 旋转因子的步长

        for (size_t k = 0; k < m2; ++k) {
            std::complex<float> w = m_twiddle[k * twiddle_step];
            
            for (size_t j = k; j < m_N; j += m) {
                std::complex<float> t = w * data[j + m2];
                std::complex<float> u = data[j];
                
                // 蝶形运算核心
                data[j] = u + t;
                data[j + m2] = u - t;
            }
        }
    }

    // 因为输入是实数，输出频域是对称的，只需要前 N/2 + 1 个点
    output.resize(m_N / 2 + 1);
    for (size_t i = 0; i <= m_N / 2; ++i) {
        output[i] = data[i];
    }
}

} // namespace rysual

