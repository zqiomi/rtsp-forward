#include "connection.h"

#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <string>

#include "util/log.h"

namespace rtsp_forward
{

Connection::Connection(int fd, size_t buffer_size) : fd_(fd), read_buffer_(buffer_size), write_buffer_(buffer_size)
{
    LOG_DEBUG("Connection created, fd=%d, buffer_size=%zu", fd_, buffer_size);
}

Connection::~Connection()
{
    Close();
}

ssize_t Connection::Recv()
{
    if (IsClosed())
    {
        return 0;
    }

    if (read_buffer_.WritableSize() == 0)
    {
        LOG_WARN("Connection::Recv: read buffer full");
        return -1;
    }

    char* buf = read_buffer_.WritePtr();
    // 必须用连续可写空间，否则 write_pos_ 靠近末尾时 recv 会越过 buffer 末尾越界写
    size_t writable = read_buffer_.ContiguousWritableSize();

    ssize_t ret = ::recv(fd_, buf, writable, 0);
    if (ret < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return 0;
        }
        LOG_ERROR("Connection::Recv failed: %s", strerror(errno));
        return -1;
    }

    if (ret == 0)
    {
        LOG_DEBUG("Connection::Recv: fd=%d, connection closed", fd_);
        return 0;
    }

    read_buffer_.Produce(ret);
    LOG_TRACE("Connection::Recv: fd=%d, received=%zd", fd_, ret);
    return ret;
}

ssize_t Connection::Send(const void* data, size_t len)
{
    std::lock_guard<std::mutex> lock(send_mutex_);

    if (IsClosed())
    {
        return 0;
    }

    if (!data || len == 0)
    {
        return 0;
    }

    // 尝试直接发送
    if (write_buffer_.ReadableSize() == 0)
    {
        ssize_t ret = ::send(fd_, data, len, MSG_NOSIGNAL);
        if (ret > 0)
        {
            size_t sent = static_cast<size_t>(ret);
            if (sent < len)
            {
                // 部分发送：剩余数据写入缓冲区，避免丢失
                Status st = write_buffer_.Write(static_cast<const char*>(data) + sent, len - sent);
                if (!st.ok())
                {
                    LOG_WARN("Connection::Send: write buffer full on partial send, fd=%d", fd_);
                    return static_cast<ssize_t>(sent);
                }
                LOG_TRACE("Connection::Send: fd=%d, sent=%zu, buffered=%zu", fd_, sent, len - sent);
            }
            else
            {
                LOG_TRACE("Connection::Send: fd=%d, sent=%zu", fd_, len);
            }
            return static_cast<ssize_t>(len);  // 报告全部成功（已发 + 已缓存）
        }
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            LOG_ERROR("Connection::Send failed: %s", strerror(errno));
            return -1;
        }
    }

    // 写入缓冲区
    Status status = write_buffer_.Write(data, len);
    if (!status.ok())
    {
        LOG_WARN("Connection::Send: write buffer full");
        return -1;
    }

    LOG_TRACE("Connection::Send: fd=%d, buffered=%zu", fd_, len);
    return len;
}

ssize_t Connection::Flush()
{
    std::lock_guard<std::mutex> lock(send_mutex_);

    if (IsClosed())
    {
        return 0;
    }

    // 只发送从读指针起的连续段，避免数据环绕时 send 越过 buffer 末尾越界读
    size_t contiguous = write_buffer_.ContiguousReadableSize();
    if (contiguous == 0)
    {
        return 0;
    }

    const char* buf = write_buffer_.ReadPtr();
    ssize_t ret = ::send(fd_, buf, contiguous, MSG_NOSIGNAL);
    if (ret < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return 0;
        }
        LOG_ERROR("Connection::Flush failed: %s", strerror(errno));
        return -1;
    }

    write_buffer_.Consume(ret);
    LOG_DEBUG("Connection::Flush: fd=%d, flushed=%zd", fd_, ret);
    return ret;
}

bool Connection::ReadLine(std::string& line, size_t max_line_len)
{
    if (IsClosed())
    {
        return false;
    }

    size_t readable = read_buffer_.ReadableSize();
    if (readable == 0)
    {
        return false;
    }

    size_t newline_offset = read_buffer_.FindChar('\n', max_line_len);
    if (newline_offset == RingBuffer::npos)
    {
        if (readable >= max_line_len)
        {
            LOG_WARN("Connection::ReadLine: line too long, max=%zu", max_line_len);
            read_buffer_.Consume(max_line_len);
        }
        return false;
    }

    size_t line_len = newline_offset + 1;
    line.resize(line_len);
    if (!read_buffer_.Peek(&line[0], line_len).ok())
    {
        return false;
    }

    // 先去掉末尾 \n，再判断是否有 \r
    if (line.size() >= 1 && line[line.size() - 1] == '\n')
    {
        line.resize(line.size() - 1);
    }
    if (line.size() >= 1 && line[line.size() - 1] == '\r')
    {
        line.resize(line.size() - 1);
    }

    read_buffer_.Consume(line_len);
    return true;
}

const char* Connection::GetReadBuffer() const
{
    return read_buffer_.ReadPtr();
}

size_t Connection::GetReadBufferSize() const
{
    return read_buffer_.ReadableSize();
}

void Connection::Consume(size_t len)
{
    read_buffer_.Consume(len);
}

bool Connection::IsWritable() const
{
    return write_buffer_.WritableSize() > 0;
}

void Connection::Close()
{
    if (fd_ >= 0)
    {
        LOG_DEBUG("Connection::Close: fd=%d", fd_);
        ::close(fd_);
        fd_ = -1;
    }
}

}  // namespace rtsp_forward
