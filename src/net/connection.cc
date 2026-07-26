#include "connection.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <cstring>

#include "util/log.h"

namespace rtsp_forward
{

Connection::Connection(int fd, size_t buffer_size)
    : fd_guard_(fd), read_buffer_(buffer_size), write_buffer_(buffer_size)
{
    LOG_DEBUG("Connection created, fd=%d, buffer_size=%zu", fd, buffer_size);
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
    size_t writable = read_buffer_.ContiguousWritableSize();

    ssize_t ret = ::recv(fd_guard_.fd(), buf, writable, 0);
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
        LOG_DEBUG("Connection::Recv: fd=%d, connection closed", fd_guard_.fd());
        return 0;
    }

    read_buffer_.Produce(ret);
    LOG_TRACE("Connection::Recv: fd=%d, received=%zd", fd_guard_.fd(), ret);
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

    if (write_buffer_.ReadableSize() == 0)
    {
        ssize_t ret = ::send(fd_guard_.fd(), data, len, MSG_NOSIGNAL);
        if (ret > 0)
        {
            size_t sent = static_cast<size_t>(ret);
            if (sent < len)
            {
                Status st = write_buffer_.Write(static_cast<const char*>(data) + sent, len - sent);
                if (!st.ok())
                {
                    LOG_WARN("Connection::Send: write buffer full on partial send, fd=%d", fd_guard_.fd());
                    return static_cast<ssize_t>(sent);
                }
                LOG_TRACE("Connection::Send: fd=%d, sent=%zu, buffered=%zu", fd_guard_.fd(), sent, len - sent);
            }
            else
            {
                LOG_TRACE("Connection::Send: fd=%d, sent=%zu", fd_guard_.fd(), len);
            }
            return static_cast<ssize_t>(len);
        }
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            LOG_ERROR("Connection::Send failed: %s", strerror(errno));
            return -1;
        }
    }

    Status status = write_buffer_.Write(data, len);
    if (!status.ok())
    {
        LOG_WARN("Connection::Send: write buffer full");
        return -1;
    }

    LOG_TRACE("Connection::Send: fd=%d, buffered=%zu", fd_guard_.fd(), len);
    return len;
}

ssize_t Connection::SendV(const struct iovec* iov, int iovcnt, size_t total_len)
{
    std::lock_guard<std::mutex> lock(send_mutex_);

    if (IsClosed())
    {
        return 0;
    }

    if (!iov || iovcnt <= 0 || total_len == 0)
    {
        return 0;
    }

    if (write_buffer_.ReadableSize() == 0)
    {
        ssize_t ret = ::writev(fd_guard_.fd(), iov, iovcnt);
        if (ret > 0)
        {
            size_t sent = static_cast<size_t>(ret);
            if (sent < total_len)
            {
                size_t remaining = total_len - sent;
                size_t skip = sent;
                for (int i = 0; i < iovcnt; ++i)
                {
                    if (skip < iov[i].iov_len)
                    {
                        const char* p = static_cast<const char*>(iov[i].iov_base) + skip;
                        size_t n = iov[i].iov_len - skip;
                        Status st = write_buffer_.Write(p, n);
                        if (!st.ok())
                        {
                            LOG_WARN("Connection::SendV: write buffer full on partial writev, fd=%d", fd_guard_.fd());
                            return static_cast<ssize_t>(sent);
                        }
                        for (int j = i + 1; j < iovcnt; ++j)
                        {
                            st = write_buffer_.Write(iov[j].iov_base, iov[j].iov_len);
                            if (!st.ok())
                            {
                                LOG_WARN("Connection::SendV: write buffer full buffering remainder, fd=%d",
                                         fd_guard_.fd());
                                return static_cast<ssize_t>(sent);
                            }
                        }
                        break;
                    }
                    skip -= iov[i].iov_len;
                }
                LOG_TRACE("Connection::SendV: fd=%d, sent=%zu, buffered=%zu", fd_guard_.fd(), sent, remaining);
            }
            else
            {
                LOG_TRACE("Connection::SendV: fd=%d, sent=%zu", fd_guard_.fd(), total_len);
            }
            return static_cast<ssize_t>(total_len);
        }
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            LOG_ERROR("Connection::SendV failed: %s", strerror(errno));
            return -1;
        }
    }

    for (int i = 0; i < iovcnt; ++i)
    {
        if (iov[i].iov_len > 0)
        {
            Status status = write_buffer_.Write(iov[i].iov_base, iov[i].iov_len);
            if (!status.ok())
            {
                LOG_WARN("Connection::SendV: write buffer full");
                return -1;
            }
        }
    }

    LOG_TRACE("Connection::SendV: fd=%d, buffered=%zu", fd_guard_.fd(), total_len);
    return total_len;
}

ssize_t Connection::Flush()
{
    std::lock_guard<std::mutex> lock(send_mutex_);

    if (IsClosed())
    {
        return 0;
    }

    struct iovec iov[2];
    int iovcnt = write_buffer_.GetReadableIoVec(iov);
    if (iovcnt == 0)
    {
        return 0;
    }

    ssize_t ret;
    if (iovcnt == 1)
    {
        ret = ::send(fd_guard_.fd(), iov[0].iov_base, iov[0].iov_len, MSG_NOSIGNAL);
    }
    else
    {
        ret = ::writev(fd_guard_.fd(), iov, iovcnt);
    }

    if (ret < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return 0;
        }
        LOG_ERROR("Connection::Flush failed: %s", strerror(errno));
        return -1;
    }

    write_buffer_.Consume(static_cast<size_t>(ret));
    LOG_DEBUG("Connection::Flush: fd=%d, flushed=%zd", fd_guard_.fd(), ret);
    return ret;
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

void Connection::Close()
{
    if (fd_guard_.IsValid())
    {
        LOG_DEBUG("Connection::Close: fd=%d", fd_guard_.fd());
    }
    fd_guard_.Close();
}

}  // namespace rtsp_forward
