#include "socket.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>

#include <cstring>

#include "util/log.h"

namespace rtsp_forward
{

Status Socket::Create(int domain, int type, int protocol)
{
    if (fd_guard_.IsValid())
    {
        LOG_WARN("Socket already created, fd=%d", fd_guard_.fd());
        Close();
    }

    int fd = socket(domain, type, protocol);
    if (fd < 0)
    {
        LOG_ERROR("socket() failed: %s", strerror(errno));
        return Status::NetworkError("socket() failed");
    }

    fd_guard_.Reset(fd);
    LOG_DEBUG("Socket::Create: fd=%d", fd);
    return Status::Ok();
}

Status Socket::SetReuseAddr(bool enable)
{
    if (!fd_guard_.IsValid())
    {
        return Status::Error("socket not created");
    }

    int opt = enable ? 1 : 0;
    int ret = setsockopt(fd_guard_.fd(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (ret < 0)
    {
        LOG_ERROR("setsockopt SO_REUSEADDR failed: %s", strerror(errno));
        return Status::NetworkError("setsockopt SO_REUSEADDR failed");
    }

    return Status::Ok();
}

Status Socket::SetNonBlocking(bool enable)
{
    if (!fd_guard_.IsValid())
    {
        return Status::Error("socket not created");
    }

    int flags = fcntl(fd_guard_.fd(), F_GETFL, 0);
    if (flags < 0)
    {
        LOG_ERROR("fcntl F_GETFL failed: %s", strerror(errno));
        return Status::NetworkError("fcntl F_GETFL failed");
    }

    if (enable)
    {
        flags |= O_NONBLOCK;
    }
    else
    {
        flags &= ~O_NONBLOCK;
    }

    int ret = fcntl(fd_guard_.fd(), F_SETFL, flags);
    if (ret < 0)
    {
        LOG_ERROR("fcntl F_SETFL failed: %s", strerror(errno));
        return Status::NetworkError("fcntl F_SETFL failed");
    }

    LOG_DEBUG("Socket::SetNonBlocking: fd=%d, enable=%d", fd_guard_.fd(), enable);
    return Status::Ok();
}

Status Socket::SetNoSigPipe(bool enable)
{
    (void)enable;
    return Status::Ok();
}

Status Socket::SetTcpNoDelay(bool enable)
{
    if (!fd_guard_.IsValid())
    {
        return Status::Error("socket not created");
    }

    int opt = enable ? 1 : 0;
    int ret = setsockopt(fd_guard_.fd(), IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    if (ret < 0)
    {
        LOG_ERROR("setsockopt TCP_NODELAY failed: %s", strerror(errno));
        return Status::NetworkError("setsockopt TCP_NODELAY failed");
    }

    LOG_DEBUG("Socket::SetTcpNoDelay: fd=%d, enable=%d", fd_guard_.fd(), enable);
    return Status::Ok();
}

Status Socket::Bind(const char* addr, int port)
{
    if (!fd_guard_.IsValid())
    {
        return Status::Error("socket not created");
    }

    struct sockaddr_in sock_addr;
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(port);

    if (addr == nullptr || strcmp(addr, "0.0.0.0") == 0)
    {
        sock_addr.sin_addr.s_addr = INADDR_ANY;
    }
    else
    {
        if (inet_pton(AF_INET, addr, &sock_addr.sin_addr) <= 0)
        {
            LOG_ERROR("inet_pton failed: %s", strerror(errno));
            return Status::InvalidArgument("invalid address");
        }
    }

    int ret = bind(fd_guard_.fd(), reinterpret_cast<struct sockaddr*>(&sock_addr), sizeof(sock_addr));
    if (ret < 0)
    {
        LOG_ERROR("bind() failed: %s", strerror(errno));
        return Status::NetworkError("bind() failed");
    }

    LOG_DEBUG("Socket::Bind: fd=%d, addr=%s, port=%d", fd_guard_.fd(), addr, port);
    return Status::Ok();
}

Status Socket::Listen(int backlog)
{
    if (!fd_guard_.IsValid())
    {
        return Status::Error("socket not created");
    }

    int ret = listen(fd_guard_.fd(), backlog);
    if (ret < 0)
    {
        LOG_ERROR("listen() failed: %s", strerror(errno));
        return Status::NetworkError("listen() failed");
    }

    LOG_DEBUG("Socket::Listen: fd=%d, backlog=%d", fd_guard_.fd(), backlog);
    return Status::Ok();
}

int Socket::Accept(struct sockaddr_in* addr)
{
    if (!fd_guard_.IsValid())
    {
        LOG_ERROR("socket not created");
        return -1;
    }

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd = accept(fd_guard_.fd(), reinterpret_cast<struct sockaddr*>(&client_addr), &addr_len);
    if (client_fd < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return -1;
        }
        LOG_ERROR("accept() failed: %s", strerror(errno));
        return -1;
    }

    if (addr != nullptr)
    {
        *addr = client_addr;
    }

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
    LOG_DEBUG("Socket::Accept: client_fd=%d, ip=%s, port=%d", client_fd, ip, ntohs(client_addr.sin_port));

    return client_fd;
}

Status Socket::Connect(const char* addr, int port)
{
    if (!fd_guard_.IsValid())
    {
        return Status::Error("socket not created");
    }

    struct sockaddr_in sock_addr;
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, addr, &sock_addr.sin_addr) <= 0)
    {
        LOG_ERROR("inet_pton failed: %s", strerror(errno));
        return Status::InvalidArgument("invalid address");
    }

    int ret = connect(fd_guard_.fd(), reinterpret_cast<struct sockaddr*>(&sock_addr), sizeof(sock_addr));
    if (ret < 0)
    {
        if (errno == EINPROGRESS)
        {
            return Status::Ok();
        }
        LOG_ERROR("connect() failed: %s", strerror(errno));
        return Status::NetworkError("connect() failed");
    }

    LOG_DEBUG("Socket::Connect: fd=%d, addr=%s, port=%d", fd_guard_.fd(), addr, port);
    return Status::Ok();
}

ssize_t Socket::Send(const void* data, size_t len)
{
    if (!fd_guard_.IsValid())
    {
        LOG_ERROR("socket not created");
        return -1;
    }

    if (!data || len == 0)
    {
        return 0;
    }

    ssize_t ret = send(fd_guard_.fd(), data, len, MSG_NOSIGNAL);
    if (ret < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return 0;
        }
        if (errno == EPIPE)
        {
            LOG_ERROR("send() EPIPE: connection closed");
            return -1;
        }
        LOG_ERROR("send() failed: %s", strerror(errno));
        return -1;
    }

    return ret;
}

ssize_t Socket::Recv(void* buf, size_t len)
{
    if (!fd_guard_.IsValid())
    {
        LOG_ERROR("socket not created");
        return -1;
    }

    if (!buf || len == 0)
    {
        return 0;
    }

    ssize_t ret = recv(fd_guard_.fd(), buf, len, 0);
    if (ret < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return 0;
        }
        LOG_ERROR("recv() failed: %s", strerror(errno));
        return -1;
    }

    if (ret == 0)
    {
        LOG_DEBUG("Socket::Recv: fd=%d, connection closed", fd_guard_.fd());
    }

    return ret;
}

void Socket::Close()
{
    if (fd_guard_.IsValid())
    {
        LOG_DEBUG("Socket::Close: fd=%d", fd_guard_.fd());
    }
    fd_guard_.Close();
}

}  // namespace rtsp_forward
