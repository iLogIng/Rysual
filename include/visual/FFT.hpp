#pragma once

#include <complex>
#include <cstddef>
#include <vector>

// 傅里叶变换（Fourier Transform）
// F(x) = \int^{\infity}_{- \infity}{ f(x) e^{-j w x} }
// j: 虚数
// w: 探测波的频率
// e^{-j w x}: 探测波
// f(x): 原始信号
//
// 欧拉公式: e^{ix} = cos{x} + i sin{x}
//
// 离散傅里叶变换 DFT
// X[k] = \sigma^{N-1}_{n=0}{ x[n] e^{-j 2PI k n/N} } , k = 0,1,2,...N-1
// x[n]: 时域样本
// e^{-j 2PI k n/N}: 第k个频率的离散探测
// 频率f_k = k f_s/N , f_s:采样率
//
// 快速傅里叶变换 FFT
// DFT的优化算法
// 旋转因子: W_N = e^{ -j 2PI / N }
// 由于存在 周期性 对称性
// 可以将大部分运算复用
// 对称性: W_N^{k + N/2} = -W_N^{k}
//      W_N^{N/2} = e^{-j PI} = cos{-PI} + j * sin{PI} = cos{PI} = -1
// 周期性: W_N^{k + N} = W_N
//      W_N^{N} = e^{-j 2PI} = cos{-2PI} + j * sin{2PI} = cos{2PI} = 1
//
// 离散傅里叶变换转换为: X[k] = \sigma^{N-1}_{n=0}{ x[n] W_N^{k n} }

namespace rysual
{

class FFT
{
public:
    explicit FFT(size_t N);
    ~FFT() = default;

    // 执行 FFT 变换
    // 输入：实数数组 input（长度 N）
    // 输出：复数数组 output（长度 N/2 + 1，因为实信号频域对称）
    void forward(const float *input, std::vector<std::complex<float>>& output) const;

    size_t size() const { return m_N; }

private:
    size_t m_N;
    size_t m_log2N;
    std::vector<size_t> m_bit_rev;  // 位反转索引表
    std::vector<std::complex<float>> m_twiddle; // 预计算得旋转因子

    mutable std::vector<std::complex<float>> m_data;

    void initBitRev();
    void initTwiddle();

}; // class FFT

} // namespace rysual

