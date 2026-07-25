#ifndef RTSP_SERVER_API_H_
#define RTSP_SERVER_API_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdint.h>

// 错误码定义
typedef enum
{
    RTSP_OK = 0,
    RTSP_ERROR = -1,
    RTSP_INVALID_ARGUMENT = -2,
    RTSP_NETWORK_ERROR = -3,
    RTSP_CLOSED = -4,
    RTSP_BUFFER_FULL = -5,
    RTSP_PARSE_ERROR = -6,
    RTSP_NOT_IMPLEMENTED = -7,
    RTSP_TIMEOUT = -8,
    RTSP_LIMIT_EXCEEDED = -9,
} RtspErrorCode;

// 服务器句柄（不透明指针）
typedef struct RtspServer RtspServer;

// 创建 RTSP 服务器
RtspServer* rtsp_server_create();

// 销毁 RTSP 服务器
void rtsp_server_destroy(RtspServer* server);

// 启动 RTSP 服务器
// @param server: 服务器句柄
// @param ip: 监听地址（可为 NULL，默认 0.0.0.0）
// @param port: 监听端口
// @return: RTSP_OK 表示成功，其他值表示失败
int rtsp_server_start(RtspServer* server, const char* ip, int port);

// 停止 RTSP 服务器
void rtsp_server_stop(RtspServer* server);

// 运行服务器事件循环（阻塞调用）
void rtsp_server_run(RtspServer* server);

// 发送 RTP 数据到所有播放中的会话
// @param server: 服务器句柄
// @param data: RTP 数据指针（已包含完整 RTP 头）
// @param len: 数据长度
// @param stream_index: 流索引，0=RTP, 1=RTCP
// @return: RTSP_OK 表示成功，其他值表示失败
int rtsp_server_send_rtp(RtspServer* server, const uint8_t* data, size_t len, int stream_index);

// 检查服务器是否正在运行
int rtsp_server_is_running(RtspServer* server);

// 获取服务器监听端口
int rtsp_server_get_port(RtspServer* server);

#ifdef __cplusplus
}
#endif

#endif  // RTSP_SERVER_API_H_
