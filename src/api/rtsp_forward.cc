#include <cstdio>
#include <cstring>

#include "core/rtsp_server.h"
#include "util/log.h"
#include "rtp/rtp_packet.h"
#include "rtsp_forward.h"
#include "util/constants.h"
#include "version.h"  // CMake 生成，含版本号宏

// 内部服务器结构体
struct RtspForwardInternal
{
    rtsp_forward::RtspForward* impl;
};

extern "C"
{
int rtsp_forward_create(void** server, const RtspForwardConfig* config)
{
    // 参数校验：server 不能为空
    if (!server)
    {
        return RTSP_INVALID_ARGUMENT;
    }
    *server = nullptr;

    // 设置默认配置
    std::string ip = rtsp_forward::kDefaultBindIp;
    int port = rtsp_forward::kDefaultPort;
    int max_sessions = rtsp_forward::kDefaultMaxSessions;
    size_t buffer_size = rtsp_forward::kDefaultBufferSize;
    const char* sdp_content = nullptr;
    int connection_timeout_sec = rtsp_forward::kDefaultConnectionTimeoutSec;
    int session_timeout_sec = rtsp_forward::kDefaultSessionTimeoutSec;

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
            return RTSP_INVALID_ARGUMENT;
        }
        port = config->port;

        // 最大会话数校验：必须大于0
        if (config->max_sessions <= 0)
        {
            return RTSP_INVALID_ARGUMENT;
        }
        max_sessions = config->max_sessions;

        // 缓冲区大小校验：必须大于0
        if (config->buffer_size == 0)
        {
            return RTSP_INVALID_ARGUMENT;
        }
        buffer_size = config->buffer_size;

        // SDP内容可选
        if (config->sdp_content != nullptr)
        {
            sdp_content = config->sdp_content;
        }

        // 超时配置：负数非法，0表示不超时
        if (config->connection_timeout_sec < 0)
        {
            return RTSP_INVALID_ARGUMENT;
        }
        connection_timeout_sec = config->connection_timeout_sec;

        if (config->session_timeout_sec < 0)
        {
            return RTSP_INVALID_ARGUMENT;
        }
        session_timeout_sec = config->session_timeout_sec;
    }

    RtspForwardInternal* internal = new RtspForwardInternal();
    if (!internal)
    {
        return RTSP_OUT_OF_MEMORY;
    }

    // 创建内部实现
    internal->impl =
        new rtsp_forward::RtspForward(ip, port, max_sessions, buffer_size, connection_timeout_sec, session_timeout_sec);
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

int rtsp_forward_destroy(void* server)
{
    // 参数校验
    if (!server)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    RtspForwardInternal* internal = static_cast<RtspForwardInternal*>(server);
    if (internal->impl)
    {
        delete internal->impl;
    }
    delete internal;

    return RTSP_OK;
}

int rtsp_forward_start(void* server)
{
    // 参数校验
    if (!server)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    RtspForwardInternal* internal = static_cast<RtspForwardInternal*>(server);
    if (!internal->impl)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    // 检查是否已启动
    if (internal->impl->is_running())
    {
        return RTSP_ALREADY_STARTED;
    }

    rtsp_forward::Status status = internal->impl->Start();
    if (!status.ok())
    {
        // 根据内部错误码转换为对外错误码
        switch (status.code())
        {
            case rtsp_forward::StatusCode::kInvalidArgument:
                return RTSP_INVALID_ARGUMENT;
            case rtsp_forward::StatusCode::kNetworkError:
                return RTSP_NETWORK_ERROR;
            default:
                return RTSP_ERROR;
        }
    }

    return RTSP_OK;
}

int rtsp_forward_stop(void* server)
{
    // 参数校验
    if (!server)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    RtspForwardInternal* internal = static_cast<RtspForwardInternal*>(server);
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

int rtsp_forward_run(void* server)
{
    // 参数校验
    if (!server)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    RtspForwardInternal* internal = static_cast<RtspForwardInternal*>(server);
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

int rtsp_forward_send_rtp(void* server, const uint8_t* data, size_t len, int stream_index)
{
    // 参数校验
    if (!server)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    RtspForwardInternal* internal = static_cast<RtspForwardInternal*>(server);
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

    rtsp_forward::RtpPacket packet;
    packet.data = data;
    packet.len = len;
    packet.stream_index = stream_index;

    rtsp_forward::Status status = internal->impl->BroadcastRtp(packet);
    if (!status.ok())
    {
        switch (status.code())
        {
            case rtsp_forward::StatusCode::kInvalidArgument:
                return RTSP_INVALID_ARGUMENT;
            case rtsp_forward::StatusCode::kBufferFull:
                return RTSP_BUFFER_FULL;
            case rtsp_forward::StatusCode::kLimitExceeded:
                return RTSP_LIMIT_EXCEEDED;
            case rtsp_forward::StatusCode::kFailedPrecondition:
                return RTSP_NOT_STARTED;
            default:
                return RTSP_ERROR;
        }
    }

    return RTSP_OK;
}

int rtsp_forward_set_sdp(void* server, const char* sdp)
{
    // 参数校验
    if (!server)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    RtspForwardInternal* internal = static_cast<RtspForwardInternal*>(server);
    if (!internal->impl)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    internal->impl->SetSdp(sdp ? sdp : "");
    return RTSP_OK;
}

int rtsp_forward_get_info(void* server, RtspForwardInfo* info)
{
    // 参数校验
    if (!server)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    if (!info)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    RtspForwardInternal* internal = static_cast<RtspForwardInternal*>(server);
    if (!internal->impl)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    internal->impl->GetInfo(info);
    return RTSP_OK;
}

const char* rtsp_forward_version_string(void)
{
    return RTSP_FORWARD_VERSION_STRING " Built at " __DATE__ " " __TIME__;
}

int rtsp_forward_set_log_level(RtspLogLevel level)
{
    if (level < RTSP_LOG_TRACE || level > RTSP_LOG_FATAL)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    rtsp_forward::LogLevel internal_level;
    switch (level)
    {
        case RTSP_LOG_TRACE:
            internal_level = rtsp_forward::LogLevel::kTrace;
            break;
        case RTSP_LOG_DEBUG:
            internal_level = rtsp_forward::LogLevel::kDebug;
            break;
        case RTSP_LOG_INFO:
            internal_level = rtsp_forward::LogLevel::kInfo;
            break;
        case RTSP_LOG_WARN:
            internal_level = rtsp_forward::LogLevel::kWarn;
            break;
        case RTSP_LOG_ERROR:
            internal_level = rtsp_forward::LogLevel::kError;
            break;
        case RTSP_LOG_FATAL:
            internal_level = rtsp_forward::LogLevel::kFatal;
            break;
        default:
            return RTSP_INVALID_ARGUMENT;
    }

    rtsp_forward::Logger::SetLevel(internal_level);
    return RTSP_OK;
}

RtspLogLevel rtsp_forward_get_log_level(void)
{
    rtsp_forward::LogLevel internal_level = rtsp_forward::Logger::GetLevel();
    switch (internal_level)
    {
        case rtsp_forward::LogLevel::kTrace:
            return RTSP_LOG_TRACE;
        case rtsp_forward::LogLevel::kDebug:
            return RTSP_LOG_DEBUG;
        case rtsp_forward::LogLevel::kInfo:
            return RTSP_LOG_INFO;
        case rtsp_forward::LogLevel::kWarn:
            return RTSP_LOG_WARN;
        case rtsp_forward::LogLevel::kError:
            return RTSP_LOG_ERROR;
        case rtsp_forward::LogLevel::kFatal:
            return RTSP_LOG_FATAL;
        default:
            return RTSP_LOG_DEBUG;
    }
}

}  // extern "C"
