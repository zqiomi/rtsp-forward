#ifndef RTSP_FORWARD_CONNECTION_H_
#define RTSP_FORWARD_CONNECTION_H_

#include <mutex>
#include <string>

#include "buffer/ring_buffer.h"

namespace rtsp_forward
{

// Connection 工具类（带缓冲区）
class Connection
{
public:
    Connection(int fd, size_t buffer_size);
    ~Connection();

    // 禁止拷贝和移动
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) = delete;
    Connection& operator=(Connection&&) = delete;

    // 接收数据到缓冲区
    ssize_t Recv();

    // 发送数据
    ssize_t Send(const void* data, size_t len);

    // 发送缓冲区数据
    ssize_t Flush();

    // 从输入缓冲区读取一行
    // max_line_len: 最大行长度，超过则截断（防止恶意攻击），默认 4096
    bool ReadLine(std::string& line, size_t max_line_len = 4096);

    // 获取缓冲区可读数据
    const char* GetReadBuffer() const;
    size_t GetReadBufferSize() const;

    // 消费缓冲区数据
    void Consume(size_t len);

    // 查看数据（不移动读指针）
    Status Peek(void* data, size_t size) const
    {
        return read_buffer_.Peek(data, size);
    }

    // 获取文件描述符
    int fd() const
    {
        return fd_;
    }

    // 是否可写
    bool IsWritable() const;

    // 是否有数据需要刷新
    bool NeedFlush()
    {
        std::lock_guard<std::mutex> lock(send_mutex_);
        return write_buffer_.ReadableSize() > 0;
    }

    // 关闭连接
    void Close();

    // 连接是否关闭
    bool IsClosed() const
    {
        return closed_;
    }

private:
    int fd_;
    bool closed_;
    RingBuffer read_buffer_;
    RingBuffer write_buffer_;
    std::mutex send_mutex_;  // 保护 Send/Flush 的线程安全
};

}  // namespace rtsp_forward

#endif  // RTSP_FORWARD_CONNECTION_H_
