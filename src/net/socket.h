#ifndef RTSP_FORWARD_SOCKET_H_
#define RTSP_FORWARD_SOCKET_H_

#include <netinet/in.h>
#include <sys/socket.h>

#include "fd_guard.h"
#include "util/status.h"

namespace rtsp_forward
{

class Socket
{
public:
    Socket() = default;
    ~Socket() = default;

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&&) = delete;
    Socket& operator=(Socket&&) = delete;

    Status Create(int domain, int type, int protocol);
    Status SetReuseAddr(bool enable);
    Status SetNonBlocking(bool enable);
    Status SetNoSigPipe(bool enable);
    Status SetTcpNoDelay(bool enable);
    Status Bind(const char* addr, int port);
    Status Listen(int backlog);
    int Accept(struct sockaddr_in* addr);
    Status Connect(const char* addr, int port);
    ssize_t Send(const void* data, size_t len);
    ssize_t Recv(void* buf, size_t len);
    void Close();

    int fd() const
    {
        return fd_guard_.fd();
    }

private:
    FdGuard fd_guard_;
};

}  // namespace rtsp_forward

#endif  // RTSP_FORWARD_SOCKET_H_
