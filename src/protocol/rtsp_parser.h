#ifndef RTSP_FORWARD_RTSP_PARSER_H_
#define RTSP_FORWARD_RTSP_PARSER_H_

#include <map>
#include <string>
#include <vector>

#include "util/status.h"

namespace rtsp_forward
{

// RTSP 方法枚举
enum class RtspMethod
{
    kUnknown,
    kOptions,
    kDescribe,
    kSetup,
    kPlay,
    kPause,
    kTeardown,
};

// RTSP 请求结构
struct RtspRequest
{
    RtspMethod method;
    std::string url;
    std::string version;
    int cseq;
    std::map<std::string, std::string> headers;
    std::string body;
};

// RTSP 请求解析器
class RtspParser
{
public:
    RtspParser() = default;
    ~RtspParser() = default;

    // 禁止拷贝和移动
    RtspParser(const RtspParser&) = delete;
    RtspParser& operator=(const RtspParser&) = delete;
    RtspParser(RtspParser&&) = delete;
    RtspParser& operator=(RtspParser&&) = delete;

    // 解析 RTSP 请求
    Status Parse(const std::string& data, RtspRequest& request);

    // 解析方法字符串
    RtspMethod ParseMethod(const std::string& method_str);

    // 将方法转换为字符串（静态方法）
    static std::string MethodToString(RtspMethod method);

private:
    // 解析请求行
    Status ParseRequestLine(const std::string& line, RtspRequest& request);

    // 解析头部
    Status ParseHeaders(const std::vector<std::string>& lines, RtspRequest& request);
};

}  // namespace rtsp_forward

#endif  // RTSP_FORWARD_RTSP_PARSER_H_
