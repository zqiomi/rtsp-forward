#include "rtsp_parser.h"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <vector>

#include "util/constants.h"
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

Status RtspParser::Parse(const char* data, size_t len, RtspRequest& request)
{
    if (!data || len == 0)
    {
        return Status::InvalidArgument("empty data");
    }

    if (len > kMaxRtspRequestDataLen)
    {
        LOG_ERROR("RtspParser::Parse: request too large, size=%zu, max=%zu", len, kMaxRtspRequestDataLen);
        return Status::ParseError("request too large");
    }

    LOG_TRACE("Parse '%.*s'", static_cast<int>(len), data);

    request.method = RtspMethod::kUnknown;
    request.url.clear();
    request.version.clear();
    request.cseq = 0;
    request.headers.clear();
    request.body.clear();

    const char* pos = data;
    const char* end = data + len;

    bool in_body = false;
    bool request_line_parsed = false;

    while (pos < end)
    {
        const char* newline = reinterpret_cast<const char*>(memchr(pos, '\n', end - pos));
        if (!newline)
        {
            break;
        }

        size_t line_len = newline - pos;
        if (line_len > 0 && pos[line_len - 1] == '\r')
        {
            line_len--;
        }

        if (line_len == 0 && !in_body)
        {
            in_body = true;
            pos = newline + 1;
            continue;
        }

        if (in_body)
        {
            request.body.append(pos, line_len);
            request.body += "\n";
        }
        else
        {
            if (!request_line_parsed)
            {
                Status status = ParseRequestLine(std::string(pos, line_len), request);
                if (!status.ok())
                {
                    return status;
                }
                request_line_parsed = true;
            }
            else
            {
                std::string line(pos, line_len);
                size_t colon_pos = line.find(':');
                if (colon_pos != std::string::npos)
                {
                    std::string key = line.substr(0, colon_pos);
                    std::string value = line.substr(colon_pos + 1);
                    key = Trim(key);
                    value = Trim(value);
                    std::transform(key.begin(), key.end(), key.begin(), ::tolower);

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
                    else if (key == "transport")
                    {
                        request.transport = ParseTransport(value);
                    }
                    request.headers[key] = value;
                }
            }
        }

        pos = newline + 1;
    }

    if (!request_line_parsed)
    {
        return Status::ParseError("missing request line");
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
    if (request.method == RtspMethod::kUnknown)
    {
        LOG_WARN("RtspParser: unknown method='%s', request_line='%s'", method_str.c_str(), line.c_str());
    }
    request.url = url;
    request.version = version;

    return Status::Ok();
}

TransportInfo RtspParser::ParseTransport(const std::string& transport_str)
{
    TransportInfo info;

    if (transport_str.find("TCP") != std::string::npos)
    {
        info.is_tcp = true;
    }
    else if (transport_str.find("RTP/AVP") != std::string::npos || transport_str.find("UDP") != std::string::npos)
    {
        info.is_udp = true;
    }
    else
    {
        // RTSP 默认使用 UDP (RTP/AVP)，如果无法识别传输类型，默认设为 UDP
        info.is_udp = true;
    }

    size_t client_port_pos = transport_str.find("client_port=");
    if (client_port_pos != std::string::npos)
    {
        const char* p = transport_str.c_str() + client_port_pos + 12;
        char* end_ptr = nullptr;
        info.client_rtp_port = static_cast<int>(std::strtol(p, &end_ptr, 10));
        if (*end_ptr == '-')
        {
            info.client_rtcp_port = static_cast<int>(std::strtol(end_ptr + 1, &end_ptr, 10));
        }
        else
        {
            info.client_rtcp_port = info.client_rtp_port + 1;
        }
    }

    size_t server_port_pos = transport_str.find("server_port=");
    if (server_port_pos != std::string::npos)
    {
        const char* p = transport_str.c_str() + server_port_pos + 12;
        char* end_ptr = nullptr;
        info.server_rtp_port = static_cast<int>(std::strtol(p, &end_ptr, 10));
        if (*end_ptr == '-')
        {
            info.server_rtcp_port = static_cast<int>(std::strtol(end_ptr + 1, &end_ptr, 10));
        }
        else
        {
            info.server_rtcp_port = info.server_rtp_port + 1;
        }
    }

    size_t interleaved_pos = transport_str.find("interleaved=");
    if (interleaved_pos != std::string::npos)
    {
        const char* p = transport_str.c_str() + interleaved_pos + 12;
        char* end_ptr = nullptr;
        info.interleaved_rtp = static_cast<int>(std::strtol(p, &end_ptr, 10));
        if (*end_ptr == '-')
        {
            info.interleaved_rtcp = static_cast<int>(std::strtol(end_ptr + 1, &end_ptr, 10));
        }
        else
        {
            info.interleaved_rtcp = info.interleaved_rtp + 1;
        }
    }

    LOG_DEBUG("RtspParser::ParseTransport: is_udp=%d, is_tcp=%d, client_port=%d-%d, interleaved=%d-%d",
              info.is_udp, info.is_tcp, info.client_rtp_port, info.client_rtcp_port,
              info.interleaved_rtp, info.interleaved_rtcp);

    return info;
}

RtspMethod RtspParser::ParseMethod(const std::string& method_str)
{
    if (method_str == "OPTIONS") return RtspMethod::kOptions;
    if (method_str == "DESCRIBE") return RtspMethod::kDescribe;
    if (method_str == "SETUP") return RtspMethod::kSetup;
    if (method_str == "PLAY") return RtspMethod::kPlay;
    if (method_str == "PAUSE") return RtspMethod::kPause;
    if (method_str == "TEARDOWN") return RtspMethod::kTeardown;
    if (method_str == "GET_PARAMETER") return RtspMethod::kGetParameter;
    if (method_str == "SET_PARAMETER") return RtspMethod::kSetParameter;
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
        case RtspMethod::kGetParameter:
            return "GET_PARAMETER";
        case RtspMethod::kSetParameter:
            return "SET_PARAMETER";
        default:
            return "UNKNOWN";
    }
}

}  // namespace rtsp_forward
