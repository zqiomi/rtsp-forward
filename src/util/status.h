#ifndef RTSP_FORWARD_STATUS_H_
#define RTSP_FORWARD_STATUS_H_

#include <string>

namespace rtsp_forward
{

// 状态码枚举
enum class StatusCode
{
    kOk = 0,
    kError = -1,
    kInvalidArgument = -2,
    kNetworkError = -3,
    kClosed = -4,
    kBufferFull = -5,
    kParseError = -6,
    kNotImplemented = -7,
    kTimeout = -8,
    kLimitExceeded = -9,
    kFailedPrecondition = -10,
    kResourceExhausted = -11,
};

// Status 类 - 用于错误处理
class Status
{
public:
    // 默认构造 - OK
    Status() : code_(StatusCode::kOk) {}

    // 从状态码构造
    explicit Status(StatusCode code) : code_(code) {}

    // 从状态码和错误信息构造
    Status(StatusCode code, const std::string& message) : code_(code), message_(message) {}

    // 拷贝构造
    Status(const Status& other) = default;

    // 移动构造
    Status(Status&& other) noexcept : code_(other.code_), message_(std::move(other.message_)) {}

    // 拷贝赋值
    Status& operator=(const Status& other) = default;

    // 移动赋值
    Status& operator=(Status&& other) noexcept
    {
        if (this != &other)
        {
            code_ = other.code_;
            message_ = std::move(other.message_);
        }
        return *this;
    }

    // 是否成功
    bool ok() const
    {
        return code_ == StatusCode::kOk;
    }

    // 获取状态码
    StatusCode code() const
    {
        return code_;
    }

    // 获取错误信息
    const std::string& message() const
    {
        return message_;
    }

    // 设置错误信息
    void set_message(const std::string& message)
    {
        message_ = message;
    }

    // 转换为字符串
    std::string ToString() const
    {
        switch (code_)
        {
            case StatusCode::kOk:
                return "OK";
            case StatusCode::kError:
                return "Error: " + message_;
            case StatusCode::kInvalidArgument:
                return "InvalidArgument: " + message_;
            case StatusCode::kNetworkError:
                return "NetworkError: " + message_;
            case StatusCode::kClosed:
                return "Closed: " + message_;
            case StatusCode::kBufferFull:
                return "BufferFull: " + message_;
            case StatusCode::kParseError:
                return "ParseError: " + message_;
            case StatusCode::kNotImplemented:
                return "NotImplemented: " + message_;
            case StatusCode::kTimeout:
                return "Timeout: " + message_;
            case StatusCode::kLimitExceeded:
                return "LimitExceeded: " + message_;
            case StatusCode::kFailedPrecondition:
                return "FailedPrecondition: " + message_;
            case StatusCode::kResourceExhausted:
                return "ResourceExhausted: " + message_;
            default:
                return "Unknown";
        }
    }

    // 静态工厂方法
    static Status Ok()
    {
        return Status();
    }
    static Status Error(const std::string& message = "")
    {
        return Status(StatusCode::kError, message);
    }
    static Status InvalidArgument(const std::string& message = "")
    {
        return Status(StatusCode::kInvalidArgument, message);
    }
    static Status NetworkError(const std::string& message = "")
    {
        return Status(StatusCode::kNetworkError, message);
    }
    static Status Closed(const std::string& message = "")
    {
        return Status(StatusCode::kClosed, message);
    }
    static Status BufferFull(const std::string& message = "")
    {
        return Status(StatusCode::kBufferFull, message);
    }
    static Status ParseError(const std::string& message = "")
    {
        return Status(StatusCode::kParseError, message);
    }
    static Status NotImplemented(const std::string& message = "")
    {
        return Status(StatusCode::kNotImplemented, message);
    }
    static Status Timeout(const std::string& message = "")
    {
        return Status(StatusCode::kTimeout, message);
    }
    static Status LimitExceeded(const std::string& message = "")
    {
        return Status(StatusCode::kLimitExceeded, message);
    }
    static Status FailedPrecondition(const std::string& message = "")
    {
        return Status(StatusCode::kFailedPrecondition, message);
    }
    static Status ResourceExhausted(const std::string& message = "")
    {
        return Status(StatusCode::kResourceExhausted, message);
    }

private:
    StatusCode code_;
    std::string message_;
};

}  // namespace rtsp_forward

#endif  // RTSP_FORWARD_STATUS_H_
