#ifndef RTSP_SERVER_RTSP_SERVER_H_
#define RTSP_SERVER_RTSP_SERVER_H_

#include <map>
#include <memory>
#include <string>

#include "../net/epoll_loop.h"
#include "../net/event_loop.h"
#include "../net/listener.h"
#include "../rtp/rtp_packet.h"
#include "../util/log.h"
#include "../util/status.h"

namespace rtsp_server
{

class RtspSession;

// RTSP 服务器核心类
class RtspServer
{
public:
    RtspServer();
    ~RtspServer();

    // 禁止拷贝和移动
    RtspServer(const RtspServer&) = delete;
    RtspServer& operator=(const RtspServer&) = delete;
    RtspServer(RtspServer&&) = delete;
    RtspServer& operator=(RtspServer&&) = delete;

    // 启动服务器
    Status Start(const std::string& ip, int port);

    // 停止服务器
    void Stop();

    // 运行事件循环（阻塞调用）
    void Run();

    // 向所有会话广播 RTP 包
    Status BroadcastRtp(const RtpPacket& packet);

    // 获取服务器状态
    bool is_running() const
    {
        return running_;
    }

    // 获取端口
    int port() const
    {
        return port_;
    }

private:
    // 新连接回调
    void OnNewConnection(int fd);

    // 读事件回调
    void OnRead(int fd);

    // 写事件回调
    void OnWrite(int fd);

    // 错误事件回调
    void OnError(int fd);

    // 关闭连接
    void CloseConnection(int fd);

    // 从连接读取数据并处理
    void ProcessConnectionData(int fd);

    bool running_;
    int port_;
    EpollLoop event_loop_;
    Listener listener_;
    std::map<int, std::unique_ptr<RtspSession>> sessions_;
};

}  // namespace rtsp_server

#endif  // RTSP_SERVER_RTSP_SERVER_H_
