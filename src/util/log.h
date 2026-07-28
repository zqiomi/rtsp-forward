#ifndef RTSP_FORWARD_LOG_H_
#define RTSP_FORWARD_LOG_H_

#include "log_kit.h"

namespace rtsp_forward
{

/**
 * @brief 获取 rtsp_forward 的日志模块 ID
 *
 * 首次调用时自动注册模块，后续调用返回同一 ID。
 * @return 模块 ID（>=1）
 */
inline int GetLogModuleId()
{
    static int id = log_kit_register("rtsp_forward");
    return id;
}

}  // namespace rtsp_forward

// 日志宏，自动注入模块 ID
#define LOG_TRACE(fmt, ...) \
    _LOG_TRACE(::rtsp_forward::GetLogModuleId(), fmt, ##__VA_ARGS__)

#define LOG_DEBUG(fmt, ...) \
    _LOG_DEBUG(::rtsp_forward::GetLogModuleId(), fmt, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) \
    _LOG_INFO(::rtsp_forward::GetLogModuleId(), fmt, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...) \
    _LOG_WARN(::rtsp_forward::GetLogModuleId(), fmt, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
    _LOG_ERROR(::rtsp_forward::GetLogModuleId(), fmt, ##__VA_ARGS__)

#define LOG_FATAL(fmt, ...) \
    _LOG_FATAL(::rtsp_forward::GetLogModuleId(), fmt, ##__VA_ARGS__)

#endif  // RTSP_FORWARD_LOG_H_
