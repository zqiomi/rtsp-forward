#include "log.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace rtsp_forward
{

// 静态成员初始化
LogLevel Logger::current_level_ = LogLevel::kDebug;

void Logger::SetLevel(LogLevel level)
{
    current_level_ = level;
}

LogLevel Logger::GetLevel()
{
    return current_level_;
}

void Logger::Log(LogLevel level, const char* file, const char* func, int line, const char* format, ...)
{
    if (current_level_ > level) return;

    // 获取当前时间
    time_t now = time(nullptr);
    struct tm* tm_info = localtime(&now);
    char time_buf[32];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

    // 日志级别前缀
    const char* level_str;
    switch (level)
    {
        case LogLevel::kTrace:
            level_str = "TRACE";
            break;
        case LogLevel::kDebug:
            level_str = "DEBUG";
            break;
        case LogLevel::kInfo:
            level_str = "INFO ";
            break;
        case LogLevel::kWarn:
            level_str = "WARN ";
            break;
        case LogLevel::kError:
            level_str = "ERROR";
            break;
        case LogLevel::kFatal:
            level_str = "FATAL";
            break;
        default:
            level_str = "UNKNOWN";
    }

    // 获取文件名（只取最后一部分）
    const char* filename = strrchr(file, '/');
    if (filename)
    {
        filename++;
    }
    else
    {
        filename = file;
    }

    // 输出日志头（带文件、函数、行号）
    fprintf(stderr, "[%s] [%s] [%s:%s:%d] ", time_buf, level_str, filename, func, line);

    // 输出日志内容
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    // 输出换行
    fprintf(stderr, "\n");
    fflush(stderr);
}

}  // namespace rtsp_forward
