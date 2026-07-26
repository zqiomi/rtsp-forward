#include "rtsp_session.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <unistd.h>

#include "rtsp_forward.h"
#include "util/log.h"

namespace rtsp_forward
{

RtspSession::RtspSession(RtspForward* server, int fd, size_t buffer_size)
    : server_(server),
      conn_(fd, buffer_size),
      state_(RtspSessionState::kInit),
      session_id_(GenerateSessionId()),
      is_udp_(false),
      udp_rtp_fd_(-1),
      udp_rtcp_fd_(-1),
      last_activity_sec_(0)
{
    memset(&client_rtp_addr_, 0, sizeof(client_rtp_addr_));
    memset(&client_rtcp_addr_, 0, sizeof(client_rtcp_addr_));
    UpdateActivity();
    LOG_INFO("RtspSession created, session_id=%s", session_id_.c_str());
}

RtspSession::~RtspSession()
{
    Close();
    LOG_INFO("RtspSession destroyed, session_id=%s", session_id_.c_str());
}

Status RtspSession::ProcessData(const std::string& data)
{
    UpdateActivity();
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
    UpdateActivity();

    if (is_udp_)
    {
        UdpEndpoint endpoint;
        if (packet.stream_index == 0)
        {
            endpoint.fd = udp_rtp_fd_;
            endpoint.addr = client_rtp_addr_;
        }
        else
        {
            endpoint.fd = udp_rtcp_fd_;
            endpoint.addr = client_rtcp_addr_;
        }
        return rtp_forwarder_.ForwardUdp(endpoint, packet);
    }

    return rtp_forwarder_.ForwardTcp(&conn_, packet);
}

void RtspSession::UpdateActivity()
{
    auto now = std::chrono::steady_clock::now();
    last_activity_sec_.store(std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count(),
                             std::memory_order_relaxed);
}

bool RtspSession::IsTimedOut(int connection_timeout_sec, int session_timeout_sec) const
{
    // 根据会话状态选择对应的超时阈值
    int timeout_sec = 0;
    if (state_ == RtspSessionState::kInit || state_ == RtspSessionState::kOptionsSent ||
        state_ == RtspSessionState::kDescribeSent)
    {
        timeout_sec = connection_timeout_sec;
    }
    else
    {
        timeout_sec = session_timeout_sec;
    }

    if (timeout_sec <= 0)
    {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    int64_t now_sec = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    int64_t last_sec = last_activity_sec_.load(std::memory_order_relaxed);

    return (now_sec - last_sec) >= timeout_sec;
}

void RtspSession::Close()
{
    state_ = RtspSessionState::kTeardown;

    if (udp_rtp_fd_ >= 0)
    {
        ::close(udp_rtp_fd_);
        udp_rtp_fd_ = -1;
    }
    if (udp_rtcp_fd_ >= 0)
    {
        ::close(udp_rtcp_fd_);
        udp_rtcp_fd_ = -1;
    }
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
        case RtspMethod::kGetParameter:
        case RtspMethod::kSetParameter:
            return HandleParameter(request);
        default:
            LOG_WARN("RtspSession::HandleRequest: unknown method=%d", static_cast<int>(request.method));
            std::string response = builder_.BuildErrorResponse(request.cseq, 405, "Method Not Allowed");
            conn_.Send(response.data(), response.size());
            return Status::InvalidArgument("unknown method");
    }
}

Status RtspSession::HandleOptions(const RtspRequest& request)
{
    LOG_DEBUG("RtspSession::HandleOptions");
    std::string response = builder_.BuildOptionsResponse(request.cseq);
    conn_.Send(response.data(), response.size());
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
        conn_.Send(response.data(), response.size());
        return Status::FailedPrecondition("SDP not configured");
    }

    std::string response = builder_.BuildDescribeResponse(request.cseq, sdp);
    conn_.Send(response.data(), response.size());
    state_ = RtspSessionState::kDescribeSent;
    return Status::Ok();
}

Status RtspSession::HandleSetup(const RtspRequest& request)
{
    LOG_DEBUG("RtspSession::HandleSetup");
    url_ = request.url;

    auto it = request.headers.find("transport");
    if (it != request.headers.end())
    {
        transport_ = it->second;
    }

    const TransportInfo& transport = request.transport;

    if (transport.is_udp)
    {
        LOG_INFO("RtspSession::HandleSetup: UDP transport requested");

        if (transport.client_rtp_port == 0)
        {
            LOG_WARN("RtspSession::HandleSetup: UDP without client_port");
            std::string response = builder_.BuildErrorResponse(request.cseq, 461, "Unsupported Transport");
            conn_.Send(response.data(), response.size());
            return Status::InvalidArgument("UDP without client_port");
        }

        udp_rtp_fd_ = CreateUdpSocket(0);
        udp_rtcp_fd_ = CreateUdpSocket(0);

        if (udp_rtp_fd_ < 0 || udp_rtcp_fd_ < 0)
        {
            LOG_ERROR("RtspSession::HandleSetup: failed to create UDP sockets");
            std::string response = builder_.BuildErrorResponse(request.cseq, 500, "Internal Server Error");
            conn_.Send(response.data(), response.size());
            return Status::NetworkError("failed to create UDP sockets");
        }

        int server_rtp_port = 0;
        int server_rtcp_port = 0;
        socklen_t len = sizeof(struct sockaddr_in);
        struct sockaddr_in addr;

        memset(&addr, 0, sizeof(addr));
        if (getsockname(udp_rtp_fd_, reinterpret_cast<struct sockaddr*>(&addr), &len) == 0)
        {
            server_rtp_port = ntohs(addr.sin_port);
        }

        memset(&addr, 0, sizeof(addr));
        if (getsockname(udp_rtcp_fd_, reinterpret_cast<struct sockaddr*>(&addr), &len) == 0)
        {
            server_rtcp_port = ntohs(addr.sin_port);
        }

        len = sizeof(struct sockaddr_in);
        struct sockaddr_in peer_addr;
        memset(&peer_addr, 0, sizeof(peer_addr));
        getpeername(conn_.fd(), reinterpret_cast<struct sockaddr*>(&peer_addr), &len);

        client_rtp_addr_ = peer_addr;
        client_rtp_addr_.sin_port = htons(static_cast<uint16_t>(transport.client_rtp_port));

        client_rtcp_addr_ = peer_addr;
        client_rtcp_addr_.sin_port = htons(static_cast<uint16_t>(transport.client_rtcp_port));

        is_udp_ = true;

        LOG_INFO("RtspSession::HandleSetup: UDP setup complete, server_port=%d-%d, client_port=%d-%d",
                 server_rtp_port, server_rtcp_port, transport.client_rtp_port, transport.client_rtcp_port);

        std::string response = builder_.BuildSetupResponseUdp(request.cseq, session_id_, server_rtp_port, server_rtcp_port,
                                                               transport.client_rtp_port, transport.client_rtcp_port);
        conn_.Send(response.data(), response.size());
        state_ = RtspSessionState::kSetupSent;
        return Status::Ok();
    }

    if (transport.is_tcp || transport.interleaved_rtp != 0)
    {
        LOG_INFO("RtspSession::HandleSetup: TCP interleaved transport requested");
        std::string response = builder_.BuildSetupResponse(request.cseq, session_id_);
        conn_.Send(response.data(), response.size());
        state_ = RtspSessionState::kSetupSent;
        return Status::Ok();
    }

    LOG_WARN("RtspSession::HandleSetup: unsupported transport='%s'", transport_.c_str());
    std::string response = builder_.BuildErrorResponse(request.cseq, 461, "Unsupported Transport");
    conn_.Send(response.data(), response.size());
    return Status::InvalidArgument("unsupported transport");
}

int RtspSession::CreateUdpSocket(int port)
{
    int fd = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (fd < 0)
    {
        LOG_ERROR("RtspSession::CreateUdpSocket: socket failed: %s", strerror(errno));
        return -1;
    }

    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        LOG_WARN("RtspSession::CreateUdpSocket: setsockopt SO_REUSEADDR failed: %s", strerror(errno));
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        LOG_ERROR("RtspSession::CreateUdpSocket: bind failed: %s", strerror(errno));
        ::close(fd);
        return -1;
    }

    LOG_DEBUG("RtspSession::CreateUdpSocket: fd=%d, port=%d", fd, port);
    return fd;
}

Status RtspSession::HandlePlay(const RtspRequest& request)
{
    LOG_DEBUG("RtspSession::HandlePlay");
    url_ = request.url;
    std::string response = builder_.BuildPlayResponse(request.cseq, session_id_);
    conn_.Send(response.data(), response.size());
    state_ = RtspSessionState::kPlaying;
    LOG_INFO("RtspSession::HandlePlay: session %s started playing", session_id_.c_str());
    return Status::Ok();
}

Status RtspSession::HandleTeardown(const RtspRequest& request)
{
    LOG_DEBUG("RtspSession::HandleTeardown");
    std::string response = builder_.BuildSimpleResponse(request.cseq, session_id_);
    conn_.Send(response.data(), response.size());
    Close();
    return Status::Ok();
}

Status RtspSession::HandlePause(const RtspRequest& request)
{
    LOG_DEBUG("RtspSession::HandlePause");
    std::string response = builder_.BuildSimpleResponse(request.cseq, session_id_);
    conn_.Send(response.data(), response.size());
    state_ = RtspSessionState::kPaused;
    return Status::Ok();
}

Status RtspSession::HandleParameter(const RtspRequest& request)
{
    LOG_DEBUG("RtspSession::HandleParameter: %s", RtspParser::MethodToString(request.method).c_str());
    // GET_PARAMETER / SET_PARAMETER 用作心跳保活，直接返回 200 OK
    std::string response = builder_.BuildSimpleResponse(request.cseq, session_id_);
    conn_.Send(response.data(), response.size());
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
