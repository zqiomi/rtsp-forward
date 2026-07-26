#ifndef RTSP_FORWARD_CONNECTION_H_
#define RTSP_FORWARD_CONNECTION_H_

#include <mutex>
#include <string>

#include "buffer/ring_buffer.h"
#include "fd_guard.h"
#include "util/constants.h"

namespace rtsp_forward
{

class Connection
{
public:
    Connection(int fd, size_t buffer_size);
    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) = delete;
    Connection& operator=(Connection&&) = delete;

    ssize_t Recv();
    ssize_t Send(const void* data, size_t len);
    ssize_t Flush();
    bool ReadLine(std::string& line, size_t max_line_len = kMaxRtspRequestDataLen);

    bool ReadLine(const char*& line_ptr, size_t& line_len, size_t max_line_len = kMaxRtspRequestDataLen);

    const char* GetReadBuffer() const;
    size_t GetReadBufferSize() const;
    void Consume(size_t len);

    Status Peek(void* data, size_t size) const
    {
        return read_buffer_.Peek(data, size);
    }

    size_t FindSubstring(const char* substr, size_t substr_len, size_t max_search_len = 0) const
    {
        return read_buffer_.FindSubstring(substr, substr_len, max_search_len);
    }

    int fd() const
    {
        return fd_guard_.fd();
    }

    bool IsWritable() const;

    bool NeedFlush()
    {
        std::lock_guard<std::mutex> lock(send_mutex_);
        return write_buffer_.ReadableSize() > 0;
    }

    void Close();

    bool IsClosed() const
    {
        return !fd_guard_.IsValid();
    }

private:
    FdGuard fd_guard_;
    RingBuffer read_buffer_;
    RingBuffer write_buffer_;
    std::mutex send_mutex_;
};

}  // namespace rtsp_forward

#endif  // RTSP_FORWARD_CONNECTION_H_
