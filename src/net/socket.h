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
    Status Bind(const char* addr, int port);
    Status Listen(int backlog);
    int Accept(struct sockaddr_in* addr);
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
