#ifndef RTSP_FORWARD_FD_GUARD_H_
#define RTSP_FORWARD_FD_GUARD_H_

#include <unistd.h>

namespace rtsp_forward
{

class FdGuard
{
public:
    FdGuard() : fd_(-1) {}
    explicit FdGuard(int fd) : fd_(fd) {}

    ~FdGuard()
    {
        Close();
    }

    FdGuard(const FdGuard&) = delete;
    FdGuard& operator=(const FdGuard&) = delete;

    FdGuard(FdGuard&& other) noexcept : fd_(other.fd_)
    {
        other.fd_ = -1;
    }

    FdGuard& operator=(FdGuard&& other) noexcept
    {
        if (this != &other)
        {
            Close();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    void Reset(int fd)
    {
        Close();
        fd_ = fd;
    }

    void Close()
    {
        if (fd_ >= 0)
        {
            ::close(fd_);
            fd_ = -1;
        }
    }

    int Release()
    {
        int fd = fd_;
        fd_ = -1;
        return fd;
    }

    int fd() const
    {
        return fd_;
    }

    bool IsValid() const
    {
        return fd_ >= 0;
    }

    explicit operator bool() const
    {
        return IsValid();
    }

private:
    int fd_;
};

}  // namespace rtsp_forward

#endif  // RTSP_FORWARD_FD_GUARD_H_