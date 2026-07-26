#ifndef RTSP_FORWARD_RTP_FORWARDER_H_
#define RTSP_FORWARD_RTP_FORWARDER_H_

#include <cstring>
#include <netinet/in.h>

#include "net/connection.h"
#include "rtp_packet.h"
#include "util/status.h"

namespace rtsp_forward
{

struct UdpEndpoint
{
    int fd;
    struct sockaddr_in addr;

    UdpEndpoint() : fd(-1)
    {
        memset(&addr, 0, sizeof(addr));
    }
};

class RtpForwarder
{
public:
    RtpForwarder() = default;
    ~RtpForwarder() = default;

    RtpForwarder(const RtpForwarder&) = delete;
    RtpForwarder& operator=(const RtpForwarder&) = delete;
    RtpForwarder(RtpForwarder&&) = delete;
    RtpForwarder& operator=(RtpForwarder&&) = delete;

    Status ForwardTcp(Connection* conn, const RtpPacket& packet);

    Status ForwardUdp(const UdpEndpoint& endpoint, const RtpPacket& packet);

private:
    void BuildInterleavedHeader(uint8_t* header, int channel, size_t length);
};

}  // namespace rtsp_forward

#endif  // RTSP_FORWARD_RTP_FORWARDER_H_
