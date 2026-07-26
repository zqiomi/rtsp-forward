#include "rtsp_parser.h"

#include <algorithm>
#include <sstream>
#include <vector>

#include "util/log.h"

namespace rtsp_forward
{

// 去除字符串首尾空格
static std::string Trim(const std::string& str)
{
    std::string result = str;
    result.erase(result.begin(), std::find_if(result.begin(), result.end(), [](int ch) { return !std::isspace(ch); }));
    result.erase(std::find_if(result.rbegin(), result.rend(), [](int ch) { return !std::isspace(ch); }).base(),
                 result.end());
    return result;
}

Status RtspParser::Parse(const std::string& data, RtspRequest& request)
{
    if (data.empty())
    {
        return Status::InvalidArgument("empty data");
    }

    // 清空请求结构
    request.method = RtspMethod::kUnknown;
    request.url.clear();
    request.version.clear();
    request.cseq = 0;
    request.headers.clear();
    request.body.clear();

    // 按行分割
    std::vector<std::string> lines;
    std::stringstream ss(data);
    std::string line;
    bool in_body = false;

    while (std::getline(ss, line))
    {
        // 处理 \r\n
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        // 空行表示头部结束，后面是 body
        if (line.empty() && !in_body)
        {
            in_body = true;
            continue;
        }

        if (in_body)
        {
            request.body += line + "\n";
        }
        else
        {
            lines.push_back(line);
        }
    }

    // 至少需要请求行
    if (lines.empty())
    {
        return Status::ParseError("missing request line");
    }

    // 解析请求行
    Status status = ParseRequestLine(lines[0], request);
    if (!status.ok())
    {
        return status;
    }

    // 解析头部
    if (lines.size() > 1)
    {
        status = ParseHeaders(std::vector<std::string>(lines.begin() + 1, lines.end()), request);
        if (!status.ok())
        {
            return status;
        }
    }

    LOG_DEBUG("RtspParser::Parse: method=%s, url=%s", MethodToString(request.method).c_str(), request.url.c_str());
    return Status::Ok();
}

Status RtspParser::ParseRequestLine(const std::string& line, RtspRequest& request)
{
    std::stringstream ss(line);
    std::string method_str, url, version;

    ss >> method_str >> url >> version;

    if (method_str.empty())
    {
        return Status::ParseError("missing method");
    }

    request.method = ParseMethod(method_str);
    request.url = url;
    request.version = version;

    return Status::Ok();
}

Status RtspParser::ParseHeaders(const std::vector<std::string>& lines, RtspRequest& request)
{
    for (const std::string& line : lines)
    {
        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos)
        {
            continue;
        }

        std::string key = line.substr(0, colon_pos);
        std::string value = line.substr(colon_pos + 1);

        // 去除空格
        key = Trim(key);
        value = Trim(value);

        // 转换为小写
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);

        // 解析 CSeq 头部
        if (key == "cseq")
        {
            char* end_ptr = nullptr;
            long val = std::strtol(value.c_str(), &end_ptr, 10);
            if (end_ptr != value.c_str() && *end_ptr == '\0')
            {
                request.cseq = static_cast<int>(val);
            }
            else
            {
                request.cseq = 0;
            }
        }

        request.headers[key] = value;
    }

    return Status::Ok();
}

RtspMethod RtspParser::ParseMethod(const std::string& method_str)
{
    if (method_str == "OPTIONS") return RtspMethod::kOptions;
    if (method_str == "DESCRIBE") return RtspMethod::kDescribe;
    if (method_str == "SETUP") return RtspMethod::kSetup;
    if (method_str == "PLAY") return RtspMethod::kPlay;
    if (method_str == "PAUSE") return RtspMethod::kPause;
    if (method_str == "TEARDOWN") return RtspMethod::kTeardown;
    return RtspMethod::kUnknown;
}

std::string RtspParser::MethodToString(RtspMethod method)
{
    switch (method)
    {
        case RtspMethod::kOptions:
            return "OPTIONS";
        case RtspMethod::kDescribe:
            return "DESCRIBE";
        case RtspMethod::kSetup:
            return "SETUP";
        case RtspMethod::kPlay:
            return "PLAY";
        case RtspMethod::kPause:
            return "PAUSE";
        case RtspMethod::kTeardown:
            return "TEARDOWN";
        default:
            return "UNKNOWN";
    }
}

}  // namespace rtsp_forward
