#include "rtp_forwarder.h"

#include <cstring>
#include <vector>

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

    // 构建 interleaved frame: $ + channel(1) + length(2, big-endian) + RTP data
    // 合并为单次写入，避免头和数据被分到不同 flush 周期导致帧错乱
    // 使用 thread_local 缓冲区复用，避免每次分配
    thread_local std::vector<uint8_t> frame_buffer;
    size_t total_len = 4 + packet.len;
    if (frame_buffer.size() < total_len)
    {
        frame_buffer.resize(total_len + 1024);  // 多分配一点，减少扩容次数
    }
    BuildInterleavedHeader(frame_buffer.data(), packet.stream_index, packet.len);
    memcpy(frame_buffer.data() + 4, packet.data, packet.len);

    ssize_t ret = conn->Send(frame_buffer.data(), total_len);
    if (ret < 0)
    {
        LOG_ERROR("RtpForwarder::ForwardTcp: send failed");
        return Status::NetworkError("send failed");
    }

    LOG_TRACE("RtpForwarder::ForwardTcp: channel=%d, length=%zu", packet.stream_index, packet.len);
    return Status::Ok();
}

Status RtpForwarder::ForwardUdp(const UdpEndpoint& endpoint, const RtpPacket& packet)
{
    if (endpoint.fd < 0 || !packet.data || packet.len == 0)
    {
        return Status::InvalidArgument("invalid argument");
    }

    ssize_t ret = ::sendto(endpoint.fd, packet.data, packet.len, 0,
                           reinterpret_cast<const struct sockaddr*>(&endpoint.addr),
                           sizeof(endpoint.addr));
    if (ret < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            LOG_WARN("RtpForwarder::ForwardUdp: sendto would block");
            return Status::ResourceExhausted("sendto would block");
        }
        LOG_ERROR("RtpForwarder::ForwardUdp: sendto failed: %s", strerror(errno));
        return Status::NetworkError("sendto failed");
    }

    LOG_TRACE("RtpForwarder::ForwardUdp: fd=%d, length=%zu", endpoint.fd, packet.len);
    return Status::Ok();
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
