#ifndef RTSP_FORWARD_RTSP_SESSION_H_
#define RTSP_FORWARD_RTSP_SESSION_H_

#include <netinet/in.h>

#include <atomic>
#include <cstdint>
#include <string>

#include "net/connection.h"
#include "net/fd_guard.h"
#include "protocol/rtsp_builder.h"
#include "protocol/rtsp_parser.h"
#include "rtp/rtp_forwarder.h"
#include "util/status.h"

namespace rtsp_forward
{

enum class RtspSessionState
{
    kInit,
    kOptionsSent,
    kDescribeSent,
    kSetupSent,
    kPlaying,
    kPaused,
    kTeardown
};

class RtspServer;

class RtspSession
{
public:
    RtspSession(RtspServer* server, int fd, size_t buffer_size);
    ~RtspSession();

    RtspSession(const RtspSession&) = delete;
    RtspSession& operator=(const RtspSession&) = delete;
    RtspSession(RtspSession&&) = delete;
    RtspSession& operator=(RtspSession&&) = delete;

    Connection& connection()
    {
        return conn_;
    }
    const Connection& connection() const
    {
        return conn_;
    }

    RtspSessionState state() const
    {
        return state_.load(std::memory_order_acquire);
    }

    void set_state(RtspSessionState state)
    {
        state_.store(state, std::memory_order_release);
    }

    const std::string& session_id() const
    {
        return session_id_;
    }

    Status ProcessData(const std::string& data);
    Status ProcessData(const char* data, size_t len);
    Status ForwardRtp(const RtpPacket& packet);
    void Close();
    bool IsTimedOut(int connection_timeout_sec, int session_timeout_sec) const;
    void UpdateActivity();

private:
    Status HandleRequest(const RtspRequest& request);
    Status HandleOptions(const RtspRequest& request);
    Status HandleDescribe(const RtspRequest& request);
    Status HandleSetup(const RtspRequest& request);
    Status HandlePlay(const RtspRequest& request);
    Status HandleTeardown(const RtspRequest& request);
    Status HandlePause(const RtspRequest& request);
    Status HandleParameter(const RtspRequest& request);
    std::string GenerateSessionId();

    int CreateUdpSocket(int port);

    RtspServer* server_;
    Connection conn_;
    std::atomic<RtspSessionState> state_;

    std::string session_id_;
    static RtspParser parser_;
    static RtspBuilder builder_;
    static RtpForwarder rtp_forwarder_;

    std::string url_;
    std::string transport_;

    std::atomic<bool> is_udp_;
    FdGuard udp_rtp_fd_;
    FdGuard udp_rtcp_fd_;
    struct sockaddr_in client_rtp_addr_;
    struct sockaddr_in client_rtcp_addr_;

    std::atomic<int64_t> last_activity_sec_;
};

}  // namespace rtsp_forward

#endif  // RTSP_FORWARD_RTSP_SESSION_H_
