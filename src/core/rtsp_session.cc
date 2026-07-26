#include "rtsp_session.h"

#include <chrono>
#include <cstdint>
#include <sstream>

#include "rtsp_forward.h"
#include "util/log.h"

namespace rtsp_forward
{

RtspSession::RtspSession(RtspForward* server, Connection* conn)
    : server_(server), conn_(conn), state_(RtspSessionState::kInit), session_id_(GenerateSessionId())
{
    LOG_INFO("RtspSession created, session_id=%s", session_id_.c_str());
}

RtspSession::~RtspSession()
{
    Close();
    if (conn_)
    {
        delete conn_;
        conn_ = nullptr;
    }
    LOG_INFO("RtspSession destroyed, session_id=%s", session_id_.c_str());
}

Status RtspSession::ProcessData(const std::string& data)
{
    RtspRequest request;
    Status status = parser_.Parse(data, request);
    if (!status.ok())
    {
        LOG_ERROR("RtspSession::ProcessData: parse failed, status=%s", status.ToString().c_str());
        return status;
    }

    return HandleRequest(request);
}

Status RtspSession::ForwardRtp(const RtpPacket& packet)
{
    return rtp_forwarder_.ForwardTcp(conn_, packet);
}

void RtspSession::Close()
{
    state_ = RtspSessionState::kTeardown;
}

Status RtspSession::HandleRequest(const RtspRequest& request)
{
    LOG_DEBUG("RtspSession::HandleRequest: method=%s, url=%s, cseq=%d",
              RtspParser::MethodToString(request.method).c_str(), request.url.c_str(), request.cseq);

    switch (request.method)
    {
        case RtspMethod::kOptions:
            return HandleOptions(request);
        case RtspMethod::kDescribe:
            return HandleDescribe(request);
        case RtspMethod::kSetup:
            return HandleSetup(request);
        case RtspMethod::kPlay:
            return HandlePlay(request);
        case RtspMethod::kTeardown:
            return HandleTeardown(request);
        case RtspMethod::kPause:
            return HandlePause(request);
        default:
            LOG_WARN("RtspSession::HandleRequest: unknown method=%d", static_cast<int>(request.method));
            std::string response = builder_.BuildErrorResponse(request.cseq, 405, "Method Not Allowed");
            conn_->Send(response.data(), response.size());
            return Status::InvalidArgument("unknown method");
    }
}

Status RtspSession::HandleOptions(const RtspRequest& request)
{
    LOG_DEBUG("RtspSession::HandleOptions");
    std::string response = builder_.BuildOptionsResponse(request.cseq);
    conn_->Send(response.data(), response.size());
    state_ = RtspSessionState::kOptionsSent;
    return Status::Ok();
}

Status RtspSession::HandleDescribe(const RtspRequest& request)
{
    LOG_DEBUG("RtspSession::HandleDescribe");
    url_ = request.url;

    // 从服务器获取 SDP 内容
    const std::string& sdp = server_->GetSdp();
    if (sdp.empty())
    {
        // SDP 未设置，返回错误
        std::string response = builder_.BuildErrorResponse(request.cseq, 500, "SDP not configured");
        conn_->Send(response.data(), response.size());
        return Status::FailedPrecondition("SDP not configured");
    }

    std::string response = builder_.BuildDescribeResponse(request.cseq, sdp);
    conn_->Send(response.data(), response.size());
    state_ = RtspSessionState::kDescribeSent;
    return Status::Ok();
}

Status RtspSession::HandleSetup(const RtspRequest& request)
{
    LOG_DEBUG("RtspSession::HandleSetup");
    url_ = request.url;

    // 获取 Transport 头部
    auto it = request.headers.find("transport");
    if (it != request.headers.end())
    {
        transport_ = it->second;
    }

    std::string response = builder_.BuildSetupResponse(request.cseq, session_id_);
    conn_->Send(response.data(), response.size());
    state_ = RtspSessionState::kSetupSent;
    return Status::Ok();
}

Status RtspSession::HandlePlay(const RtspRequest& request)
{
    LOG_DEBUG("RtspSession::HandlePlay");
    url_ = request.url;
    std::string response = builder_.BuildPlayResponse(request.cseq, session_id_);
    conn_->Send(response.data(), response.size());
    state_ = RtspSessionState::kPlaying;
    LOG_INFO("RtspSession::HandlePlay: session %s started playing", session_id_.c_str());
    return Status::Ok();
}

Status RtspSession::HandleTeardown(const RtspRequest& request)
{
    LOG_DEBUG("RtspSession::HandleTeardown");
    std::string response = builder_.BuildSimpleResponse(request.cseq, session_id_);
    conn_->Send(response.data(), response.size());
    Close();
    return Status::Ok();
}

Status RtspSession::HandlePause(const RtspRequest& request)
{
    LOG_DEBUG("RtspSession::HandlePause");
    std::string response = builder_.BuildSimpleResponse(request.cseq, session_id_);
    conn_->Send(response.data(), response.size());
    state_ = RtspSessionState::kPaused;
    return Status::Ok();
}

std::string RtspSession::GenerateSessionId()
{
    // 时间戳 + 服务器实例内递增序列号
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    uint64_t timestamp = static_cast<uint64_t>(ms.count());
    uint64_t seq = server_->NextSessionSequence();

    std::stringstream ss;
    ss << std::hex << timestamp << std::hex << seq;
    return ss.str();
}

}  // namespace rtsp_forward
