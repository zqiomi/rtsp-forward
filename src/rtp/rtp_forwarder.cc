#include "rtp_forwarder.h"

#include <cstring>
#include <sys/uio.h>

#include "net/connection.h"
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

    if (packet.len > kMaxRtpPacketSize)
    {
        LOG_ERROR("RtpForwarder::ForwardTcp: packet too large, len=%zu", packet.len);
        return Status::InvalidArgument("packet too large");
    }

    uint8_t header[4];
    BuildInterleavedHeader(header, packet.stream_index, packet.len);

    struct iovec iov[2];
    iov[0].iov_base = header;
    iov[0].iov_len = 4;
    iov[1].iov_base = const_cast<uint8_t*>(packet.data);
    iov[1].iov_len = packet.len;

    ssize_t ret = conn->SendV(iov, 2, 4 + packet.len);
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
                           reinterpret_cast<const struct sockaddr*>(&endpoint.addr), sizeof(endpoint.addr));
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
