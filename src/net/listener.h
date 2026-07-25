#ifndef RTSP_SERVER_LISTENER_H_
#define RTSP_SERVER_LISTENER_H_

#include <netinet/in.h>

#include "../util/status.h"
#include "socket.h"

namespace rtsp_server
{

// Listener 工具类（监听socket封装）
class Listener
{
public:
    Listener();
    ~Listener();

    // 禁止拷贝和移动
    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;
    Listener(Listener&&) = delete;
    Listener& operator=(Listener&&) = delete;

    // 开始监听
    Status Listen(const char* addr, int port);

    // 接受新连接
    int Accept(struct sockaddr_in* client_addr);

    // 接受新连接（不带客户端地址）
    int Accept();

    // 获取监听socket
    int fd() const
    {
        return socket_.fd();
    }

    // 关闭监听
    void Close();

    // 是否有效
    bool IsValid() const
    {
        return socket_.IsValid();
    }

private:
    Socket socket_;
};

}  // namespace rtsp_server

#endif  // RTSP_SERVER_LISTENER_H_
