#ifndef RTSP_FORWARD_RTSP_SESSION_H_
#define RTSP_FORWARD_RTSP_SESSION_H_

#include <string>

#include "net/connection.h"
#include "protocol/rtsp_builder.h"
#include "protocol/rtsp_parser.h"
#include "rtp/rtp_forwarder.h"
#include "util/status.h"

namespace rtsp_forward
{

// RTSP 会话状态枚举
enum class RtspSessionState
{
    kInit,          // 初始状态
    kOptionsSent,   // OPTIONS 已发送
    kDescribeSent,  // DESCRIBE 已发送
    kSetupSent,     // SETUP 已发送
    kPlaying,       // 播放中
    kPaused,        // 已暂停
    kTeardown       // 已关闭
};

// RTSP 会话管理类
class RtspForward;

class RtspSession
{
public:
    RtspSession(RtspForward* server, Connection* conn);
    ~RtspSession();

    // 禁止拷贝和移动
    RtspSession(const RtspSession&) = delete;
    RtspSession& operator=(const RtspSession&) = delete;
    RtspSession(RtspSession&&) = delete;
    RtspSession& operator=(RtspSession&&) = delete;

    // 获取连接
    Connection* connection() const
    {
        return conn_;
    }

    // 获取会话状态
    RtspSessionState state() const
    {
        return state_;
    }

    // 获取会话 ID
    const std::string& session_id() const
    {
        return session_id_;
    }

    // 处理接收到的数据
    Status ProcessData(const std::string& data);

    // 透传 RTP 包
    Status ForwardRtp(const RtpPacket& packet);

    // 关闭会话
    void Close();

private:
    // 处理 RTSP 请求
    Status HandleRequest(const RtspRequest& request);

    // 处理 OPTIONS 请求
    Status HandleOptions(const RtspRequest& request);

    // 处理 DESCRIBE 请求
    Status HandleDescribe(const RtspRequest& request);

    // 处理 SETUP 请求
    Status HandleSetup(const RtspRequest& request);

    // 处理 PLAY 请求
    Status HandlePlay(const RtspRequest& request);

    // 处理 TEARDOWN 请求
    Status HandleTeardown(const RtspRequest& request);

    // 处理 PAUSE 请求
    Status HandlePause(const RtspRequest& request);

    // 生成会话 ID（时间戳 + 递增序列号）
    std::string GenerateSessionId();

    RtspForward* server_;
    Connection* conn_;
    RtspSessionState state_;
    std::string session_id_;
    RtspParser parser_;
    RtspBuilder builder_;
    RtpForwarder rtp_forwarder_;
    std::string url_;
    std::string transport_;
};

}  // namespace rtsp_forward

#endif  // RTSP_FORWARD_RTSP_SESSION_H_
