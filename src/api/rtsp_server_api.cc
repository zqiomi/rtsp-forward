#include <cstring>

#include "core/rtsp_server.h"
#include "rtp/rtp_packet.h"
#include "rtsp_server.h"
#include "util/constants.h"

// 内部服务器结构体
struct RtspServerInternal
{
    rtsp_server::RtspServer* impl;
};

extern "C"
{
int rtsp_server_create(void** server, const RtspServerConfig* config)
{
    // 参数校验：server 不能为空
    if (!server)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    // 初始化输出参数为NULL
    *server = nullptr;

    RtspServerInternal* internal = new RtspServerInternal();
    if (!internal)
    {
        return RTSP_OUT_OF_MEMORY;
    }

    // 设置默认配置
    std::string ip = rtsp_server::kDefaultBindIp;
    int port = rtsp_server::kDefaultPort;
    int max_sessions = rtsp_server::kDefaultMaxSessions;
    size_t buffer_size = rtsp_server::kDefaultBufferSize;
    const char* sdp_content = nullptr;

    // 如果提供了配置，使用配置值
    if (config != nullptr)
    {
        // 监听地址
        if (config->ip != nullptr)
        {
            ip = config->ip;
        }

        // 端口校验：1-65535
        if (config->port <= 0 || config->port > 65535)
        {
            delete internal;
            return RTSP_INVALID_ARGUMENT;
        }
        port = config->port;

        // 最大会话数校验：必须大于0
        if (config->max_sessions <= 0)
        {
            delete internal;
            return RTSP_INVALID_ARGUMENT;
        }
        max_sessions = config->max_sessions;

        // 缓冲区大小校验：必须大于0
        if (config->buffer_size == 0)
        {
            delete internal;
            return RTSP_INVALID_ARGUMENT;
        }
        buffer_size = config->buffer_size;

        // SDP内容可选
        if (config->sdp_content != nullptr)
        {
            sdp_content = config->sdp_content;
        }
    }

    // 创建内部实现
    internal->impl = new rtsp_server::RtspServer(ip, port, max_sessions, buffer_size);
    if (!internal->impl)
    {
        delete internal;
        return RTSP_OUT_OF_MEMORY;
    }

    // 设置SDP（如果提供）
    if (sdp_content != nullptr)
    {
        internal->impl->SetSdp(sdp_content);
    }

    *server = internal;
    return RTSP_OK;
}

int rtsp_server_destroy(void* server)
{
    // 参数校验
    if (!server)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    RtspServerInternal* internal = static_cast<RtspServerInternal*>(server);
    if (internal->impl)
    {
        delete internal->impl;
    }
    delete internal;

    return RTSP_OK;
}

int rtsp_server_start(void* server)
{
    // 参数校验
    if (!server)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    RtspServerInternal* internal = static_cast<RtspServerInternal*>(server);
    if (!internal->impl)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    // 检查是否已启动
    if (internal->impl->is_running())
    {
        return RTSP_ALREADY_STARTED;
    }

    rtsp_server::Status status = internal->impl->Start();
    if (!status.ok())
    {
        // 根据内部错误码转换为对外错误码
        switch (status.code())
        {
            case rtsp_server::StatusCode::kInvalidArgument:
                return RTSP_INVALID_ARGUMENT;
            case rtsp_server::StatusCode::kNetworkError:
                return RTSP_NETWORK_ERROR;
            default:
                return RTSP_ERROR;
        }
    }

    return RTSP_OK;
}

int rtsp_server_stop(void* server)
{
    // 参数校验
    if (!server)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    RtspServerInternal* internal = static_cast<RtspServerInternal*>(server);
    if (!internal->impl)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    // 检查是否已停止
    if (!internal->impl->is_running())
    {
        return RTSP_NOT_STARTED;
    }

    internal->impl->Stop();
    return RTSP_OK;
}

int rtsp_server_run(void* server)
{
    // 参数校验
    if (!server)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    RtspServerInternal* internal = static_cast<RtspServerInternal*>(server);
    if (!internal->impl)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    // 检查是否已启动
    if (!internal->impl->is_running())
    {
        return RTSP_NOT_STARTED;
    }

    internal->impl->Run();
    return RTSP_OK;
}

int rtsp_server_send_rtp(void* server, const uint8_t* data, size_t len, int stream_index)
{
    // 参数校验
    if (!server)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    RtspServerInternal* internal = static_cast<RtspServerInternal*>(server);
    if (!internal->impl)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    // 检查是否已启动
    if (!internal->impl->is_running())
    {
        return RTSP_NOT_STARTED;
    }

    // 数据指针校验
    if (!data)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    // 数据长度校验
    if (len == 0)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    // 流索引校验：0或1
    if (stream_index != 0 && stream_index != 1)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    rtsp_server::RtpPacket packet;
    packet.data = data;
    packet.len = len;
    packet.stream_index = stream_index;

    rtsp_server::Status status = internal->impl->BroadcastRtp(packet);
    if (!status.ok())
    {
        switch (status.code())
        {
            case rtsp_server::StatusCode::kInvalidArgument:
                return RTSP_INVALID_ARGUMENT;
            case rtsp_server::StatusCode::kBufferFull:
                return RTSP_BUFFER_FULL;
            case rtsp_server::StatusCode::kLimitExceeded:
                return RTSP_LIMIT_EXCEEDED;
            case rtsp_server::StatusCode::kFailedPrecondition:
                return RTSP_NOT_STARTED;
            default:
                return RTSP_ERROR;
        }
    }

    return RTSP_OK;
}

int rtsp_server_set_sdp(void* server, const char* sdp)
{
    // 参数校验
    if (!server)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    RtspServerInternal* internal = static_cast<RtspServerInternal*>(server);
    if (!internal->impl)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    internal->impl->SetSdp(sdp ? sdp : "");
    return RTSP_OK;
}

int rtsp_server_is_running(void* server, int* running)
{
    // 参数校验
    if (!server)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    if (!running)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    RtspServerInternal* internal = static_cast<RtspServerInternal*>(server);
    if (!internal->impl)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    *running = internal->impl->is_running() ? 1 : 0;
    return RTSP_OK;
}

int rtsp_server_get_port(void* server, int* port)
{
    // 参数校验
    if (!server)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    if (!port)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    RtspServerInternal* internal = static_cast<RtspServerInternal*>(server);
    if (!internal->impl)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    *port = internal->impl->port();
    return RTSP_OK;
}

int rtsp_server_get_active_sessions(void* server, int* count)
{
    // 参数校验
    if (!server)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    if (!count)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    RtspServerInternal* internal = static_cast<RtspServerInternal*>(server);
    if (!internal->impl)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    *count = static_cast<int>(internal->impl->GetActiveSessions());
    return RTSP_OK;
}

}  // extern "C"
