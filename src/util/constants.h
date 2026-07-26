/**
 * @file constants.h
 * @brief 系统级常量定义
 *
 * 集中管理项目中所有系统级硬编码常量，避免魔法数字和字符串散落在代码各处。
 */

#ifndef RTSP_FORWARD_CONSTANTS_H_
#define RTSP_FORWARD_CONSTANTS_H_

#include <cstddef>
#include <cstdint>

namespace rtsp_forward
{

// ===== 网络默认配置 =====

/** 默认监听地址 */
static constexpr const char* kDefaultBindIp = "0.0.0.0";

/** 默认监听端口 */
static const int kDefaultPort = 554;

/** 默认最大并发会话数 */
static const int kDefaultMaxSessions = 10;

/** 默认每个连接的缓冲区大小（字节） */
static const size_t kDefaultBufferSize = 65536;

/** listen() 的 backlog 参数 */
static const int kListenBacklog = 128;

// ===== epoll 配置 =====

/** epoll 事件数组初始大小 */
static const int kEpollEventsSize = 64;

/** epoll_wait 超时时间（毫秒） */
static const int kEpollWaitTimeoutMs = 1000;

// ===== RTSP 协议常量 =====

/** RTSP 协议版本字符串 */
static constexpr const char* kRtspVersion = "RTSP/1.0";

/** OPTIONS 响应中 Public 头部支持的方法列表 */
static constexpr const char* kRtspOptionsMethods =
    "OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, TEARDOWN, GET_PARAMETER, SET_PARAMETER";

/** PLAY 响应中 Range 头部的默认值 */
static constexpr const char* kRtspPlayRange = "npt=0.000-";

/** DESCRIBE 响应的 Content-Type */
static constexpr const char* kContentTypeSdp = "application/sdp";

/** 错误响应的 Content-Type */
static constexpr const char* kContentTypeText = "text/plain";

// ===== RTP 常量 =====

/** RTP interleaved frame 标识符（'$'） */
static const uint8_t kRtpInterleavedMarker = '$';

/** 默认 RTP 通道号 */
static const int kDefaultRtpChannel = 0;

/** 默认 RTCP 通道号 */
static const int kDefaultRtcpChannel = 1;

// ===== 超时配置 =====

/** 默认连接空闲超时（秒），适用于握手未完成的会话，0=不超时 */
static const int kDefaultConnectionTimeoutSec = 30;

/** 默认会话空闲超时（秒），适用于已建立的会话，0=不超时 */
static const int kDefaultSessionTimeoutSec = 60;

/** 超时检查间隔（秒），timerfd 触发周期 */
static const int kTimeoutCheckIntervalSec = 1;

}  // namespace rtsp_forward

#endif  // RTSP_FORWARD_CONSTANTS_H_
