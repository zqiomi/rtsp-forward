#ifndef RTSP_SERVER_CONNECTION_H_
#define RTSP_SERVER_CONNECTION_H_

#include <memory>
#include <string>

#include "../buffer/ring_buffer.h"
#include "../util/status.h"

namespace rtsp_server
{

// Connection 工具类（带缓冲区）
class Connection
{
public:
    Connection(int fd);
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
    bool ReadLine(std::string& line);

    // 获取缓冲区可读数据
    const char* GetReadBuffer() const;
    size_t GetReadBufferSize() const;

    // 消费缓冲区数据
    void Consume(size_t len);

    // 获取文件描述符
    int fd() const
    {
        return fd_;
    }

    // 是否可写
    bool IsWritable() const;

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
    std::unique_ptr<RingBuffer> read_buffer_;
    std::unique_ptr<RingBuffer> write_buffer_;
};

}  // namespace rtsp_server

#endif  // RTSP_SERVER_CONNECTION_H_
