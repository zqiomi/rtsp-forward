#ifndef RTSP_SERVER_RTP_FORWARDER_H_
#define RTSP_SERVER_RTP_FORWARDER_H_

#include "../net/connection.h"
#include "../util/status.h"
#include "rtp_packet.h"

namespace rtsp_server
{

// RTP 转发器（仅透传，不解析不打包）
class RtpForwarder
{
public:
    RtpForwarder() = default;
    ~RtpForwarder() = default;

    // 禁止拷贝和移动
    RtpForwarder(const RtpForwarder&) = delete;
    RtpForwarder& operator=(const RtpForwarder&) = delete;
    RtpForwarder(RtpForwarder&&) = delete;
    RtpForwarder& operator=(RtpForwarder&&) = delete;

    // 透传 RTP 包到连接（TCP模式，添加 interleaved frame 头）
    Status ForwardTcp(Connection* conn, const RtpPacket& packet);

    // 透传 RTP 包（UDP模式，直接发送，暂未实现）
    Status ForwardUdp(const RtpPacket& packet);

private:
    // 构建 interleaved frame 头
    void BuildInterleavedHeader(uint8_t* header, int channel, size_t length);
};

}  // namespace rtsp_server

#endif  // RTSP_SERVER_RTP_FORWARDER_H_
