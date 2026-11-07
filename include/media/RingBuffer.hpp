#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace rysual
{

// 单生产者/单消费者的字节环形缓冲区
//
// 说明：
//   - write 为非阻塞；空间不足时返回 false
//   - read 为破坏性读取
//   - 位置使用单调递增的 size_t，对容量取模定位
class RingBuffer
{
public:
    explicit RingBuffer(size_t capacity_bytes)
        : m_capacity(capacity_bytes > 0 ? capacity_bytes : 1)
        , m_data(m_capacity)
    {
    }

    ~RingBuffer() = default;

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    size_t capacity() const { return m_capacity; }

    size_t size() const { return availableRead(); }

    size_t availableRead() const
    {
        // 先读 readPos 再读 writePos，避免第三方线程查询时出现无符号下溢
        const size_t read_pos = m_read_pos.load(std::memory_order_acquire);
        const size_t write_pos = m_write_pos.load(std::memory_order_acquire);
        return write_pos - read_pos;
    }

    size_t availableWrite() const
    {
        const size_t available = availableRead();
        return available < m_capacity ? m_capacity - available : 0;
    }

    // 非阻塞写入 空间不足返回 false
    bool write(const uint8_t *data, size_t bytes)
    {
        if (bytes == 0) {
            return true;
        }
        if (!data || bytes > m_capacity) {
            return false;
        }

        const size_t read_pos = m_read_pos.load(std::memory_order_acquire);
        const size_t write_pos = m_write_pos.load(std::memory_order_relaxed);
        const size_t used = write_pos - read_pos;

        if (m_capacity - used < bytes) {
            return false;
        }

        // 处理回绕，最多分两段拷贝
        const size_t index = write_pos % m_capacity;
        const size_t first = std::min(bytes, m_capacity - index);
        std::memcpy(m_data.data() + index, data, first);
        if (bytes > first) {
            std::memcpy(m_data.data(), data + first, bytes - first);
        }

        m_write_pos.store(write_pos + bytes, std::memory_order_release);
        return true;
    }

    // 破坏性读取，返回实际读取的字节数
    size_t read(uint8_t *dst, size_t max_bytes)
    {
        if (!dst || max_bytes == 0) {
            return 0;
        }

        const size_t write_pos = m_write_pos.load(std::memory_order_acquire);
        const size_t read_pos = m_read_pos.load(std::memory_order_relaxed);
        const size_t n = std::min(max_bytes, write_pos - read_pos);

        if (n == 0) {
            return 0;
        }

        // 处理回绕，最多分两段拷贝
        const size_t index = read_pos % m_capacity;
        const size_t first = std::min(n, m_capacity - index);
        std::memcpy(dst, m_data.data() + index, first);
        if (n > first) {
            std::memcpy(dst + first, m_data.data(), n - first);
        }

        m_read_pos.store(read_pos + n, std::memory_order_release);
        return n;
    }

    // 仅在生产者/消费者均已暂停或停止时调用
    void clear()
    {
        m_read_pos.store(0, std::memory_order_release);
        m_write_pos.store(0, std::memory_order_release);
    }

private:
    size_t m_capacity = 0;
    std::vector<uint8_t> m_data;

    // 单调递增的位置，写入/读取时对容量取模定位
    std::atomic<size_t> m_read_pos{0};
    std::atomic<size_t> m_write_pos{0};

}; // class RingBuffer

} // namespace rysual
