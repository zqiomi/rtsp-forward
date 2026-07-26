#ifndef RTSP_FORWARD_RTSP_SESSION_H_
#define RTSP_FORWARD_RTSP_SESSION_H_

#include <netinet/in.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <sys/uio.h>

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

    // ===== Connection 包装接口（供 RtspServer 调用）=====

    int fd() const { return conn_.fd(); }
    bool IsClosed() const { return conn_.IsClosed(); }

    // 接收数据
    ssize_t Recv() { return conn_.Recv(); }

    // 刷新写缓冲
    ssize_t Flush() { return conn_.Flush(); }

    // 发送数据
    ssize_t Send(const void* data, size_t len) { return conn_.Send(data, len); }
    ssize_t SendV(const struct iovec* iov, int iovcnt, size_t total_len) { return conn_.SendV(iov, iovcnt, total_len); }

    // 读缓冲区操作
    const char* GetReadBuffer() const { return conn_.GetReadBuffer(); }
    size_t GetReadBufferSize() const { return conn_.GetReadBufferSize(); }
    void Consume(size_t len) { conn_.Consume(len); }
    Status Peek(void* data, size_t size) const { return conn_.Peek(data, size); }
    size_t FindSubstring(const char* substr, size_t substr_len) const { return conn_.FindSubstring(substr, substr_len); }

    // 背压控制
    void RecordDrop() { conn_.RecordDrop(); }
    void RecordSuccess() { conn_.RecordSuccess(); }
    int GetConsecutiveDrops() { return conn_.GetConsecutiveDrops(); }
    bool NeedFlush() { return conn_.NeedFlush(); }

    // ===== 状态与会话信息 =====

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
    // 以下静态成员均为无状态工具类实例，内部无可变状态，线程安全：
    // - RtspParser::Parse 为纯解析函数，不修改 parser 自身
    // - RtspBuilder::Build* 为纯构建函数，不修改 builder 自身
    // - RtpForwarder::Forward* 为纯转发函数，不修改 forwarder 自身
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
