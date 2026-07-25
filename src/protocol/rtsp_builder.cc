#include "rtsp_builder.h"

#include "util/constants.h"

namespace rtsp_server
{

std::string RtspBuilder::BuildResponse(int status_code, const std::string& reason_phrase)
{
    return std::string(kRtspVersion) + " " + std::to_string(status_code) + " " + reason_phrase + "\r\n\r\n";
}

std::string RtspBuilder::BuildResponse(int status_code, const std::string& reason_phrase,
                                       const std::map<std::string, std::string>& headers)
{
    std::string response = std::string(kRtspVersion) + " " + std::to_string(status_code) + " " + reason_phrase + "\r\n";

    for (const auto& pair : headers)
    {
        response += pair.first + ": " + pair.second + "\r\n";
    }

    response += "\r\n";
    return response;
}

std::string RtspBuilder::BuildResponse(int status_code, const std::string& reason_phrase,
                                       const std::map<std::string, std::string>& headers, const std::string& body)
{
    std::string response = std::string(kRtspVersion) + " " + std::to_string(status_code) + " " + reason_phrase + "\r\n";

    for (const auto& pair : headers)
    {
        response += pair.first + ": " + pair.second + "\r\n";
    }

    response += "\r\n" + body;
    return response;
}

std::string RtspBuilder::BuildOptionsResponse(int cseq, const std::string& session_id)
{
    std::map<std::string, std::string> headers;
    headers["CSeq"] = std::to_string(cseq);
    headers["Public"] = kRtspOptionsMethods;
    if (!session_id.empty())
    {
        headers["Session"] = session_id;
    }

    return BuildResponse(200, "OK", headers);
}

std::string RtspBuilder::BuildDescribeResponse(int cseq, const std::string& sdp_content)
{
    std::map<std::string, std::string> headers;
    headers["CSeq"] = std::to_string(cseq);
    headers["Content-Type"] = kContentTypeSdp;
    headers["Content-Length"] = std::to_string(sdp_content.size());

    return BuildResponse(200, "OK", headers, sdp_content);
}

std::string RtspBuilder::BuildSetupResponse(int cseq, const std::string& session_id, int rtp_channel, int rtcp_channel)
{
    std::map<std::string, std::string> headers;
    headers["CSeq"] = std::to_string(cseq);
    headers["Transport"] =
        "RTP/AVP/TCP;interleaved=" + std::to_string(rtp_channel) + "-" + std::to_string(rtcp_channel);
    if (!session_id.empty())
    {
        headers["Session"] = session_id;
    }

    return BuildResponse(200, "OK", headers);
}

std::string RtspBuilder::BuildPlayResponse(int cseq, const std::string& session_id)
{
    std::map<std::string, std::string> headers;
    headers["CSeq"] = std::to_string(cseq);
    headers["Range"] = kRtspPlayRange;
    if (!session_id.empty())
    {
        headers["Session"] = session_id;
    }

    return BuildResponse(200, "OK", headers);
}

std::string RtspBuilder::BuildSimpleResponse(int cseq, const std::string& session_id)
{
    std::map<std::string, std::string> headers;
    headers["CSeq"] = std::to_string(cseq);
    if (!session_id.empty())
    {
        headers["Session"] = session_id;
    }

    return BuildResponse(200, "OK", headers);
}

std::string RtspBuilder::BuildErrorResponse(int cseq, int status_code, const std::string& message)
{
    std::string reason = GetReasonPhrase(status_code);
    std::map<std::string, std::string> headers;
    headers["CSeq"] = std::to_string(cseq);
    headers["Content-Type"] = kContentTypeText;
    headers["Content-Length"] = std::to_string(message.size());

    return BuildResponse(status_code, reason, headers, message);
}

std::string RtspBuilder::GetReasonPhrase(int status_code)
{
    switch (status_code)
    {
        case 200:
            return "OK";
        case 400:
            return "Bad Request";
        case 401:
            return "Unauthorized";
        case 404:
            return "Not Found";
        case 405:
            return "Method Not Allowed";
        case 451:
            return "Parameter Not Understood";
        case 500:
            return "Internal Server Error";
        case 501:
            return "Not Implemented";
        default:
            return "Unknown";
    }
}

}  // namespace rtsp_server
