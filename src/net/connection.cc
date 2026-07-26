#include "connection.h"

#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>

#include "util/log.h"

namespace rtsp_forward
{

Connection::Connection(int fd, size_t buffer_size)
    : fd_(fd), closed_(false), read_buffer_(new RingBuffer(buffer_size)), write_buffer_(new RingBuffer(buffer_size))
{
    LOG_DEBUG("Connection created, fd=%d, buffer_size=%zu", fd_, buffer_size);
}

Connection::~Connection()
{
    Close();
}

ssize_t Connection::Recv()
{
    if (closed_)
    {
        return 0;
    }

    if (read_buffer_->WritableSize() == 0)
    {
        LOG_WARN("Connection::Recv: read buffer full");
        return -1;
    }

    char* buf = read_buffer_->WritePtr();
    size_t writable = read_buffer_->WritableSize();

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
        closed_ = true;
        return 0;
    }

    read_buffer_->Produce(ret);
    LOG_DEBUG("Connection::Recv: fd=%d, received=%zd", fd_, ret);
    return ret;
}

ssize_t Connection::Send(const void* data, size_t len)
{
    std::lock_guard<std::mutex> lock(send_mutex_);

    if (closed_)
    {
        return 0;
    }

    if (!data || len == 0)
    {
        return 0;
    }

    // 尝试直接发送
    if (write_buffer_->ReadableSize() == 0)
    {
        ssize_t ret = ::send(fd_, data, len, MSG_NOSIGNAL);
        if (ret >= 0)
        {
            LOG_DEBUG("Connection::Send: fd=%d, sent=%zd", fd_, ret);
            return ret;
        }
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            LOG_ERROR("Connection::Send failed: %s", strerror(errno));
            return -1;
        }
    }

    // 写入缓冲区
    Status status = write_buffer_->Write(data, len);
    if (!status.ok())
    {
        LOG_WARN("Connection::Send: write buffer full");
        return -1;
    }

    LOG_DEBUG("Connection::Send: fd=%d, buffered=%zu", fd_, len);
    return len;
}

ssize_t Connection::Flush()
{
    std::lock_guard<std::mutex> lock(send_mutex_);

    if (closed_)
    {
        return 0;
    }

    size_t readable = write_buffer_->ReadableSize();
    if (readable == 0)
    {
        return 0;
    }

    const char* buf = write_buffer_->ReadPtr();
    ssize_t ret = ::send(fd_, buf, readable, MSG_NOSIGNAL);
    if (ret < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return 0;
        }
        LOG_ERROR("Connection::Flush failed: %s", strerror(errno));
        return -1;
    }

    write_buffer_->Consume(ret);
    LOG_DEBUG("Connection::Flush: fd=%d, flushed=%zd", fd_, ret);
    return ret;
}

bool Connection::ReadLine(std::string& line)
{
    if (closed_)
    {
        return false;
    }

    size_t readable = read_buffer_->ReadableSize();
    if (readable == 0)
    {
        return false;
    }

    const char* buf = read_buffer_->ReadPtr();
    const char* newline = reinterpret_cast<const char*>(memchr(buf, '\n', readable));
    if (newline == nullptr)
    {
        return false;
    }

    size_t line_len = newline - buf + 1;
    line.assign(buf, line_len);

    // 去掉 \r\n 或 \n
    if (line.size() >= 2 && line[line.size() - 2] == '\r')
    {
        line.resize(line.size() - 2);
    }
    else if (line.size() >= 1 && line[line.size() - 1] == '\n')
    {
        line.resize(line.size() - 1);
    }

    read_buffer_->Consume(line_len);
    return true;
}

const char* Connection::GetReadBuffer() const
{
    return read_buffer_->ReadPtr();
}

size_t Connection::GetReadBufferSize() const
{
    return read_buffer_->ReadableSize();
}

void Connection::Consume(size_t len)
{
    read_buffer_->Consume(len);
}

bool Connection::IsWritable() const
{
    return write_buffer_->WritableSize() > 0;
}

void Connection::Close()
{
    if (!closed_)
    {
        if (fd_ >= 0)
        {
            close(fd_);
            LOG_DEBUG("Connection::Close: fd=%d", fd_);
            fd_ = -1;
        }
        closed_ = true;
    }
}

}  // namespace rtsp_forward
