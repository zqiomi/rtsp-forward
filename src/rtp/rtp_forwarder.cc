#include "rtp_forwarder.h"

#include <cstring>

#include "util/constants.h"
#include "util/log.h"

namespace rtsp_forward
{

Status RtpForwarder::ForwardTcp(Connection* conn, const RtpPacket& packet)
{
    if (!conn || !packet.data || packet.len == 0)
    {
        return Status::InvalidArgument("invalid argument");
    }

    if (conn->IsClosed())
    {
        return Status::Closed("connection closed");
    }

    // 构建 interleaved frame 头
    // $ + channel(1 byte) + length(2 bytes, big-endian)
    uint8_t header[4];
    BuildInterleavedHeader(header, packet.stream_index, packet.len);

    // 发送头部
    ssize_t ret = conn->Send(header, sizeof(header));
    if (ret < 0)
    {
        LOG_ERROR("RtpForwarder::ForwardTcp: send header failed");
        return Status::NetworkError("send header failed");
    }

    // 发送 RTP 包数据（不修改，直接透传）
    ret = conn->Send(packet.data, packet.len);
    if (ret < 0)
    {
        LOG_ERROR("RtpForwarder::ForwardTcp: send packet failed");
        return Status::NetworkError("send packet failed");
    }

    LOG_DEBUG("RtpForwarder::ForwardTcp: channel=%d, length=%zu", packet.stream_index, packet.len);
    return Status::Ok();
}

Status RtpForwarder::ForwardUdp(const RtpPacket& packet)
{
    (void)packet;
    LOG_WARN("RtpForwarder::ForwardUdp: not implemented");
    return Status::NotImplemented("UDP forwarding not implemented");
}

void RtpForwarder::BuildInterleavedHeader(uint8_t* header, int channel, size_t length)
{
    if (!header)
    {
        return;
    }

    // interleaved frame 标识符
    header[0] = kRtpInterleavedMarker;
    // channel (0=RTP, 1=RTCP)
    header[1] = static_cast<uint8_t>(channel);
    // length (big-endian)
    header[2] = static_cast<uint8_t>((length >> 8) & 0xFF);
    header[3] = static_cast<uint8_t>(length & 0xFF);
}

}  // namespace rtsp_forward
