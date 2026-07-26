#ifndef RTSP_FORWARD_LOG_H_
#define RTSP_FORWARD_LOG_H_

#include <cstdarg>

namespace rtsp_forward
{

// 日志级别
enum class LogLevel
{
    kTrace = 0,
    kDebug,
    kInfo,
    kWarn,
    kError,
    kFatal,
};

// 日志类
class Logger
{
public:
    // 设置日志级别
    static void SetLevel(LogLevel level);

    // 获取当前日志级别
    static LogLevel GetLevel();

    // 统一日志接口
    static void Log(LogLevel level, const char* file, const char* func, int line, const char* format, ...);

private:
    // 当前日志级别
    static LogLevel current_level_;
};

// 日志宏（自动携带文件、函数、行号）
#define LOG_TRACE(format, ...) Logger::Log(LogLevel::kTrace, __FILE__, __func__, __LINE__, format, ##__VA_ARGS__)
#define LOG_DEBUG(format, ...) Logger::Log(LogLevel::kDebug, __FILE__, __func__, __LINE__, format, ##__VA_ARGS__)
#define LOG_INFO(format, ...) Logger::Log(LogLevel::kInfo, __FILE__, __func__, __LINE__, format, ##__VA_ARGS__)
#define LOG_WARN(format, ...) Logger::Log(LogLevel::kWarn, __FILE__, __func__, __LINE__, format, ##__VA_ARGS__)
#define LOG_ERROR(format, ...) Logger::Log(LogLevel::kError, __FILE__, __func__, __LINE__, format, ##__VA_ARGS__)
#define LOG_FATAL(format, ...) Logger::Log(LogLevel::kFatal, __FILE__, __func__, __LINE__, format, ##__VA_ARGS__)

}  // namespace rtsp_forward

#endif  // RTSP_FORWARD_LOG_H_
