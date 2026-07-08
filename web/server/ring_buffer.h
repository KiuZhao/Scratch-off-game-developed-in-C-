#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <cstring>
#include <algorithm>
#include <sys/socket.h>

// 线程安全的环形缓冲区, 用于每个连接的读/写
class RingBuffer {
public:
    explicit RingBuffer(size_t cap = 65536)
        : m_cap(cap), m_buf(new char[cap]), m_read(0), m_write(0), m_size(0) {}

    ~RingBuffer() { delete[] m_buf; }

    // 可写空间
    size_t writable() const { return m_cap - m_size; }
    // 可读字节
    size_t readable() const { return m_size; }
    bool   empty()    const { return m_size == 0; }
    bool   full()     const { return m_size >= m_cap; }

    // 从 socket 读取数据到缓冲区 (非阻塞)
    // 返回读取字节数, -1 = 错误, 0 = 无数据
    int readFrom(int fd) {
        if (full()) return 0;
        size_t tail = m_write % m_cap;
        size_t len  = std::min(writable(), m_cap - tail);

        ssize_t n = recv(fd, m_buf + tail, len, 0);
        if (n > 0) {
            m_write += n;
            m_size  += n;
        }
        return (int)n;
    }

    // 写入数据到 socket (非阻塞)
    int writeTo(int fd) {
        if (empty()) return 0;
        size_t head = m_read % m_cap;
        size_t len  = std::min(readable(), m_cap - head);

        ssize_t n = send(fd, m_buf + head, len, MSG_NOSIGNAL);
        if (n > 0) {
            m_read += n;
            m_size -= n;
        }
        return (int)n;
    }

    // 追加数据
    void append(const char* data, size_t len) {
        if (len > writable()) return;
        for (size_t i = 0; i < len; ++i) {
            m_buf[(m_write + i) % m_cap] = data[i];
        }
        m_write += len;
        m_size  += len;
    }

    // 查看数据但不消费
    void peek(char* out, size_t len) const {
        if (len > readable()) return;
        for (size_t i = 0; i < len; ++i)
            out[i] = m_buf[(m_read + i) % m_cap];
    }

    // 消费数据
    void consume(size_t len) {
        if (len > readable()) return;
        m_read += len;
        m_size -= len;
    }

    // 查找换行符位置 (相对于 read 指针)
    int find(char c) const {
        size_t r = readable();
        for (size_t i = 0; i < r; ++i) {
            if (m_buf[(m_read + i) % m_cap] == c)
                return (int)i;
        }
        return -1;
    }

    // 获取连续内存块 (方便解析)
    const char* data() const { return m_buf; }
    size_t readPos()   const { return m_read; }
    size_t writePos()  const { return m_write; }
    size_t capacity()  const { return m_cap; }

    void reset() { m_read = m_write = m_size = 0; }

private:
    size_t m_cap;
    char*  m_buf;
    size_t m_read;   // 累计读字节 (mod cap 得到位置)
    size_t m_write;  // 累计写字节
    size_t m_size;   // 当前数据量
};

#endif
