#ifndef RTSP_FORWARD_LOG_H_
#define RTSP_FORWARD_LOG_H_

#include "log_kit.h"

namespace rtsp_forward
{

/**
 * @brief 获取本模块 ID（首次调用时自动注册）
 */
inline int GetModuleId()
{
    static int id = log_kit::LogRegister("rtsp_forward");
    return id;
}

/**
 * @brief 设置日志级别
 */
inline void SetLogLevel(log_kit::LogLevel level)
{
    log_kit::LogSetLevel(GetModuleId(), level);
}

/**
 * @brief 获取当前日志级别
 */
inline log_kit::LogLevel GetLogLevel()
{
    return log_kit::LogGetLevel(GetModuleId());
}

}  // namespace rtsp_forward

// ===== 日志宏（自动注入模块 ID）=====

// 取消 log_kit 的原始宏（需要注入 module_id）
#undef LOG_TRACE
#undef LOG_DEBUG
#undef LOG_INFO
#undef LOG_WARN
#undef LOG_ERROR
#undef LOG_FATAL

#define RTSP_LOG_TRACE(fmt, ...) \
    ::log_kit::LogWrite(::rtsp_forward::GetModuleId(), ::log_kit::LogLevel::kTrace, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define RTSP_LOG_DEBUG(fmt, ...) \
    ::log_kit::LogWrite(::rtsp_forward::GetModuleId(), ::log_kit::LogLevel::kDebug, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define RTSP_LOG_INFO(fmt, ...) \
    ::log_kit::LogWrite(::rtsp_forward::GetModuleId(), ::log_kit::LogLevel::kInfo, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define RTSP_LOG_WARN(fmt, ...) \
    ::log_kit::LogWrite(::rtsp_forward::GetModuleId(), ::log_kit::LogLevel::kWarn, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define RTSP_LOG_ERROR(fmt, ...) \
    ::log_kit::LogWrite(::rtsp_forward::GetModuleId(), ::log_kit::LogLevel::kError, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)

#define RTSP_LOG_FATAL(fmt, ...) \
    ::log_kit::LogWrite(::rtsp_forward::GetModuleId(), ::log_kit::LogLevel::kFatal, __FILE__, __func__, __LINE__, fmt, ##__VA_ARGS__)

// 兼容旧宏名
#define LOG_TRACE  RTSP_LOG_TRACE
#define LOG_DEBUG  RTSP_LOG_DEBUG
#define LOG_INFO   RTSP_LOG_INFO
#define LOG_WARN   RTSP_LOG_WARN
#define LOG_ERROR  RTSP_LOG_ERROR
#define LOG_FATAL  RTSP_LOG_FATAL

#endif  // RTSP_FORWARD_LOG_H_
