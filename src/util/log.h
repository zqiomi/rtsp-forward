#ifndef RTSP_FORWARD_LOG_H_
#define RTSP_FORWARD_LOG_H_

#include "log_kit.h"

namespace rtsp_forward
{

inline int GetModuleId()
{
    static int id = log_kit::LogRegister("rtsp_forward");
    return id;
}

}  // namespace rtsp_forward

// ===== 日志宏（自动注入模块 ID）=====

#define RTSP_LOG_TRACE(fmt, ...) \
    _LOG_TRACE(::rtsp_forward::GetModuleId(), fmt, ##__VA_ARGS__)

#define RTSP_LOG_DEBUG(fmt, ...) \
    _LOG_DEBUG(::rtsp_forward::GetModuleId(), fmt, ##__VA_ARGS__)

#define RTSP_LOG_INFO(fmt, ...) \
    _LOG_INFO(::rtsp_forward::GetModuleId(), fmt, ##__VA_ARGS__)

#define RTSP_LOG_WARN(fmt, ...) \
    _LOG_WARN(::rtsp_forward::GetModuleId(), fmt, ##__VA_ARGS__)

#define RTSP_LOG_ERROR(fmt, ...) \
    _LOG_ERROR(::rtsp_forward::GetModuleId(), fmt, ##__VA_ARGS__)

#define RTSP_LOG_FATAL(fmt, ...) \
    _LOG_FATAL(::rtsp_forward::GetModuleId(), fmt, ##__VA_ARGS__)

// 兼容旧宏名
#define LOG_TRACE  RTSP_LOG_TRACE
#define LOG_DEBUG  RTSP_LOG_DEBUG
#define LOG_INFO   RTSP_LOG_INFO
#define LOG_WARN   RTSP_LOG_WARN
#define LOG_ERROR  RTSP_LOG_ERROR
#define LOG_FATAL  RTSP_LOG_FATAL

#endif  // RTSP_FORWARD_LOG_H_
