#ifndef RTSP_SERVER_RTP_PACKET_H_
#define RTSP_SERVER_RTP_PACKET_H_

#include <cstddef>
#include <cstdint>

namespace rtsp_server
{

// RTP 包结构（仅透传，不解析不打包）
struct RtpPacket
{
    const uint8_t* data;  // RTP包数据指针（已包含完整RTP头）
    size_t len;           // RTP包长度
    int stream_index;     // 流索引: 0=RTP, 1=RTCP
};

}  // namespace rtsp_server

#endif  // RTSP_SERVER_RTP_PACKET_H_
