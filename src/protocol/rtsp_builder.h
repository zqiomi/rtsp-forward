#ifndef RTSP_SERVER_RTSP_BUILDER_H_
#define RTSP_SERVER_RTSP_BUILDER_H_

#include <map>
#include <string>

#include "util/constants.h"

namespace rtsp_server
{

// RTSP 响应构建器
class RtspBuilder
{
public:
    RtspBuilder() = default;
    ~RtspBuilder() = default;

    // 禁止拷贝和移动
    RtspBuilder(const RtspBuilder&) = delete;
    RtspBuilder& operator=(const RtspBuilder&) = delete;
    RtspBuilder(RtspBuilder&&) = delete;
    RtspBuilder& operator=(RtspBuilder&&) = delete;

    // 构建响应
    std::string BuildResponse(int status_code, const std::string& reason_phrase);

    // 构建响应（带头部）
    std::string BuildResponse(int status_code, const std::string& reason_phrase,
                              const std::map<std::string, std::string>& headers);

    // 构建响应（带头部和body）
    std::string BuildResponse(int status_code, const std::string& reason_phrase,
                              const std::map<std::string, std::string>& headers, const std::string& body);

    // 构建 OPTIONS 响应
    std::string BuildOptionsResponse(int cseq, const std::string& session_id = "");

    // 构建 DESCRIBE 响应
    std::string BuildDescribeResponse(int cseq, const std::string& sdp_content);

    // 构建 SETUP 响应
    std::string BuildSetupResponse(int cseq, const std::string& session_id, int rtp_channel = kDefaultRtpChannel,
                                   int rtcp_channel = kDefaultRtcpChannel);

    // 构建 PLAY 响应
    std::string BuildPlayResponse(int cseq, const std::string& session_id);

    // 构建简单响应（用于 PAUSE/TEARDOWN/GET_PARAMETER/SET_PARAMETER）
    std::string BuildSimpleResponse(int cseq, const std::string& session_id);

    // 构建错误响应
    std::string BuildErrorResponse(int cseq, int status_code, const std::string& message);

private:
    // 获取状态码对应的原因短语
    std::string GetReasonPhrase(int status_code);
};

}  // namespace rtsp_server

#endif  // RTSP_SERVER_RTSP_BUILDER_H_
