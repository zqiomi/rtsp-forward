#ifndef RTSP_SERVER_SOCKET_H_
#define RTSP_SERVER_SOCKET_H_

#include <netinet/in.h>
#include <sys/socket.h>

#include "../util/status.h"

namespace rtsp_server
{

// Socket 工具类
class Socket
{
public:
    Socket();
    ~Socket();

    // 禁止拷贝和移动
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&&) = delete;
    Socket& operator=(Socket&&) = delete;

    // 创建socket
    Status Create(int domain, int type, int protocol);

    // 设置socket选项
    Status SetReuseAddr(bool enable);
    Status SetNonBlocking(bool enable);
    Status SetNoSigPipe(bool enable);
    Status SetTcpNoDelay(bool enable);

    // 绑定地址
    Status Bind(const char* addr, int port);

    // 监听
    Status Listen(int backlog);

    // 接受连接
    int Accept(struct sockaddr_in* addr);

    // 连接
    Status Connect(const char* addr, int port);

    // 发送数据
    ssize_t Send(const void* data, size_t len);

    // 接收数据
    ssize_t Recv(void* buf, size_t len);

    // 关闭socket
    void Close();

    // 获取文件描述符
    int fd() const
    {
        return fd_;
    }
    bool IsValid() const
    {
        return fd_ >= 0;
    }

private:
    int fd_;
};

}  // namespace rtsp_server

#endif  // RTSP_SERVER_SOCKET_H_
