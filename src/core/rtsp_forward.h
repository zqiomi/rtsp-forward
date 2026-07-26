#ifndef RTSP_FORWARD_RTSP_FORWARD_H_
#define RTSP_FORWARD_RTSP_FORWARD_H_

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "net/epoll_loop.h"
#include "net/listener.h"
#include "rtp/rtp_packet.h"
#include "util/status.h"

namespace rtsp_forward
{

class RtspSession;

// RTSP 转发核心类
class RtspForward
{
public:
    /**
     * @brief 构造函数
     *
     * @param ip 监听地址
     * @param port 监听端口
     * @param max_sessions 最大并发会话数
     * @param buffer_size 每个连接的缓冲区大小
     */
    RtspForward(const std::string& ip, int port, int max_sessions, size_t buffer_size);
    ~RtspForward();

    // 禁止拷贝和移动
    RtspForward(const RtspForward&) = delete;
    RtspForward& operator=(const RtspForward&) = delete;
    RtspForward(RtspForward&&) = delete;
    RtspForward& operator=(RtspForward&&) = delete;

    /**
     * @brief 启动服务器
     *
     * 使用构造时配置的端口和地址启动监听
     *
     * @return Status 启动结果
     */
    Status Start();

    /**
     * @brief 停止服务器
     */
    void Stop();

    /**
     * @brief 运行事件循环（阻塞调用）
     */
    void Run();

    /**
     * @brief 向所有会话广播 RTP 包
     *
     * 实现"一个输入，多路转发"的核心功能：输入一个RTP包，
     * 同时转发给所有处于PLAYING状态的会话。
     *
     * @param packet RTP包
     * @return Status 转发结果
     */
    Status BroadcastRtp(const RtpPacket& packet);

    /**
     * @brief 设置 SDP 内容
     *
     * @param sdp SDP字符串
     */
    void SetSdp(const std::string& sdp)
    {
        sdp_ = sdp;
    }

    /**
     * @brief 获取 SDP 内容
     *
     * @return const std::string& SDP字符串
     */
    const std::string& GetSdp() const
    {
        return sdp_;
    }

    /**
     * @brief 获取服务器运行状态
     *
     * @return bool true表示正在运行
     */
    bool is_running() const
    {
        return running_;
    }

    /**
     * @brief 获取监听端口
     *
     * @return int 端口号
     */
    int port() const
    {
        return port_;
    }

    /**
     * @brief 获取当前活跃会话数
     *
     * @return size_t 活跃会话数
     */
    size_t GetActiveSessions()
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        return sessions_.size();
    }

    /**
     * @brief 获取最大会话数限制
     *
     * @return int 最大会话数
     */
    int max_sessions() const
    {
        return max_sessions_;
    }

    /**
     * @brief 获取缓冲区大小
     *
     * @return size_t 缓冲区大小
     */
    size_t buffer_size() const
    {
        return buffer_size_;
    }

    /**
     * @brief 获取下一个会话序列号
     *
     * @return uint64_t 递增的序列号
     */
    uint64_t NextSessionSequence()
    {
        return session_sequence_++;
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
    int max_sessions_;
    size_t buffer_size_;
    std::string sdp_;
    std::string ip_;
    uint64_t session_sequence_;
    EpollLoop event_loop_;
    Listener listener_;
    std::map<int, std::shared_ptr<RtspSession>> sessions_;
    std::mutex sessions_mutex_;  // 保护 sessions_ 容器的线程安全
};

}  // namespace rtsp_forward

#endif  // RTSP_FORWARD_RTSP_FORWARD_H_
