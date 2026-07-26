#include "rtsp_server.h"

#include <sys/timerfd.h>
#include <unistd.h>

#include <memory>
#include <vector>

#include "net/connection.h"
#include "rtsp_forward.h"
#include "rtsp_session.h"
#include "util/constants.h"
#include "util/log.h"

namespace rtsp_forward
{

RtspServer::RtspServer(const std::string& ip, int port, int max_sessions, size_t buffer_size,
                       int connection_timeout_sec, int session_timeout_sec)
    : port_(port),
      max_sessions_(max_sessions),
      buffer_size_(buffer_size),
      ip_(ip),
      session_sequence_(0),
      running_(false),
      connection_timeout_sec_(connection_timeout_sec),
      session_timeout_sec_(session_timeout_sec),
      timer_fd_(-1),
      total_connections_(0),
      timed_out_sessions_(0)
{
    LOG_INFO("RtspServer created, ip=%s, port=%d, max_sessions=%d, buffer_size=%zu, conn_timeout=%d, sess_timeout=%d",
             ip_.c_str(), port_, max_sessions_, buffer_size_, connection_timeout_sec_, session_timeout_sec_);
}

RtspServer::~RtspServer()
{
    Stop();
    LOG_INFO("RtspServer destroyed");
}

Status RtspServer::Start()
{
    LOG_INFO("RtspServer::Start: ip=%s, port=%d", ip_.c_str(), port_);

    // 创建监听器
    Status status = listener_.Listen(ip_.c_str(), port_);
    if (!status.ok())
    {
        LOG_ERROR("RtspServer::Start: listener listen failed, status=%s", status.ToString().c_str());
        return status;
    }

    // 初始化事件循环
    status = event_loop_.Init();
    if (!status.ok())
    {
        LOG_ERROR("RtspServer::Start: event loop init failed, status=%s", status.ToString().c_str());
        return status;
    }

    // 添加监听 socket 到事件循环
    status = event_loop_.AddFd(listener_.fd(), EventType::kRead,
                               [this](int fd, EventType type, EventResult result)
                               {
                                   (void)type;
                                   (void)result;
                                   this->OnNewConnection(fd);
                               });
    if (!status.ok())
    {
        LOG_ERROR("RtspServer::Start: add listener fd failed, status=%s", status.ToString().c_str());
        return status;
    }

    // 创建 timerfd 用于周期性超时检查
    timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timer_fd_ < 0)
    {
        LOG_ERROR("RtspServer::Start: timerfd_create failed");
        return Status::Error("timerfd_create failed");
    }

    struct itimerspec ts;
    ts.it_interval.tv_sec = kTimeoutCheckIntervalSec;
    ts.it_interval.tv_nsec = 0;
    ts.it_value.tv_sec = kTimeoutCheckIntervalSec;
    ts.it_value.tv_nsec = 0;
    if (timerfd_settime(timer_fd_, 0, &ts, nullptr) < 0)
    {
        LOG_ERROR("RtspServer::Start: timerfd_settime failed");
        close(timer_fd_);
        timer_fd_ = -1;
        return Status::Error("timerfd_settime failed");
    }

    status = event_loop_.AddFd(timer_fd_, EventType::kRead,
                               [this](int fd, EventType type, EventResult result)
                               {
                                   (void)type;
                                   (void)result;
                                   // 读取 timerfd 触发次数，避免水平触发反复唤醒
                                   uint64_t expirations;
                                   if (read(fd, &expirations, sizeof(expirations)) > 0)
                                   {
                                       this->CheckTimeouts();
                                   }
                               });
    if (!status.ok())
    {
        LOG_ERROR("RtspServer::Start: add timer fd failed, status=%s", status.ToString().c_str());
        close(timer_fd_);
        timer_fd_ = -1;
        return status;
    }

    start_time_ = std::chrono::steady_clock::now();
    SetRunning(true);
    LOG_INFO("RtspServer::Start: server started on %s:%d", ip_.c_str(), port_);
    return Status::Ok();
}

void RtspServer::Stop()
{
    if (!IsRunning())
    {
        return;
    }

    LOG_INFO("RtspServer::Stop: stopping server");

    SetRunning(false);

    // 停止事件循环
    event_loop_.Stop();

    // 清理 timerfd
    if (timer_fd_ >= 0)
    {
        event_loop_.RemoveFd(timer_fd_);
        close(timer_fd_);
        timer_fd_ = -1;
    }

    // 关闭所有会话
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& pair : sessions_)
        {
            pair.second->Close();
        }
        sessions_.clear();
    }

    // 关闭监听器
    listener_.Close();

    LOG_INFO("RtspServer::Stop: server stopped");
}

void RtspServer::Run()
{
    if (!IsRunning())
    {
        LOG_WARN("RtspServer::Run: server not started");
        return;
    }

    LOG_INFO("RtspServer::Run: entering event loop");
    event_loop_.Run();
    LOG_INFO("RtspServer::Run: event loop exited");
}

Status RtspServer::BroadcastRtp(const RtpPacket& packet)
{
    if (!IsRunning())
    {
        return Status::FailedPrecondition("server not running");
    }

    // 短暂持锁，拷贝需要转发的 session 列表
    // 策略：只在拷贝期间持锁，ForwardRtp 使用 shared_ptr 保证 session 生命周期
    // ForwardRtp 内部操作（UpdateActivity/发送）都是线程安全的，无需额外锁保护
    std::vector<std::shared_ptr<RtspSession>> playing_sessions;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        playing_sessions.reserve(sessions_.size());
        for (auto& pair : sessions_)
        {
            RtspSession* session = pair.second.get();
            if (session->state() == RtspSessionState::kPlaying)
            {
                playing_sessions.push_back(pair.second);
            }
        }
    }

    // 释放锁后逐个转发，避免长时间持锁阻塞主线程（新连接/超时检测）
    for (auto& session : playing_sessions)
    {
        Status status = session->ForwardRtp(packet);
        if (!status.ok())
        {
            // ResourceExhausted (EAGAIN) 是正常情况，仅记录 WARN 不中断转发
            // 其他错误则记录日志并继续处理下一个 session
            LOG_WARN("RtspServer::BroadcastRtp: forward failed for session %s", session->session_id().c_str());
            continue;
        }

        // 如果有数据被缓冲（socket 满导致 EAGAIN），注册 EPOLLOUT 触发 flush
        // 否则缓冲的 RTP 数据不会被发出，客户端会因超时断开重连
        Connection& conn = session->connection();
        if (conn.NeedFlush())
        {
            event_loop_.ModifyFd(conn.fd(),
                                 EventType(static_cast<int>(EventType::kRead) | static_cast<int>(EventType::kWrite)));
        }
    }

    return Status::Ok();
}

void RtspServer::OnNewConnection(int fd)
{
    LOG_DEBUG("RtspServer::OnNewConnection: fd=%d", fd);

    // 必须先 accept 取出 pending 连接，否则 listener fd 持续可读，epoll 反复触发
    int client_fd = listener_.Accept();
    if (client_fd < 0)
    {
        LOG_ERROR("RtspServer::OnNewConnection: accept failed");
        return;
    }

    // 检查最大会话数限制：超限直接关闭，让客户端立即感知拒绝
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        if (sessions_.size() >= static_cast<size_t>(max_sessions_))
        {
            LOG_WARN("RtspServer::OnNewConnection: max sessions exceeded, current=%zu, max=%d, reject fd=%d",
                     sessions_.size(), max_sessions_, client_fd);
            ::close(client_fd);
            return;
        }
    }

    LOG_INFO("RtspServer::OnNewConnection: new client connected, fd=%d", client_fd);
    total_connections_.fetch_add(1, std::memory_order_relaxed);

    // 添加读事件（先注册事件，再创建会话）
    Status status = event_loop_.AddFd(client_fd, EventType::kRead,
                                      [this](int fd, EventType type, EventResult result)
                                      {
                                          (void)type;
                                          if (fd > 0)
                                          {
                                              if (result.events & static_cast<int>(EventType::kRead))
                                              {
                                                  this->OnRead(fd);
                                              }
                                              if (result.events & static_cast<int>(EventType::kWrite))
                                              {
                                                  this->OnWrite(fd);
                                              }
                                              if (result.events & static_cast<int>(EventType::kError))
                                              {
                                                  this->OnError(fd);
                                              }
                                          }
                                      });

    if (!status.ok())
    {
        LOG_ERROR("RtspServer::OnNewConnection: add client fd failed, status=%s", status.ToString().c_str());
        ::close(client_fd);
        return;
    }

    // 创建会话（Session 直接管理 Connection，shared_ptr 保证 session 多线程安全）
    auto session = std::make_shared<RtspSession>(this, client_fd, buffer_size_);

    // 保存会话
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_[client_fd] = session;
    }
}

void RtspServer::OnRead(int fd)
{
    LOG_TRACE("RtspServer::OnRead: fd=%d", fd);
    ProcessConnectionData(fd);
}

void RtspServer::OnWrite(int fd)
{
    LOG_DEBUG("RtspServer::OnWrite: fd=%d", fd);

    // 短暂持锁获取 session 引用
    std::shared_ptr<RtspSession> session;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = sessions_.find(fd);
        if (it == sessions_.end())
        {
            return;
        }
        session = it->second;
    }

    Connection& conn = session->connection();
    if (conn.IsClosed())
    {
        CloseConnection(fd);
        return;
    }

    // 刷新写缓冲区
    ssize_t ret = conn.Flush();
    if (ret < 0)
    {
        LOG_ERROR("RtspServer::OnWrite: flush failed, fd=%d", fd);
        CloseConnection(fd);
        return;
    }

    // 如果缓冲区已清空，移除写事件
    if (!conn.NeedFlush())
    {
        event_loop_.ModifyFd(fd, EventType::kRead);
    }
}

void RtspServer::OnError(int fd)
{
    LOG_DEBUG("RtspServer::OnError: fd=%d", fd);
    CloseConnection(fd);
}

void RtspServer::CloseConnection(int fd)
{
    LOG_DEBUG("RtspServer::CloseConnection: fd=%d", fd);

    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(fd);
    if (it != sessions_.end())
    {
        LOG_INFO("RtspServer::CloseConnection: client disconnected, fd=%d", fd);
        // 从事件循环中移除
        event_loop_.RemoveFd(fd);
        sessions_.erase(it);
    }
}

void RtspServer::ProcessConnectionData(int fd)
{
    // 短暂持锁获取 session 引用
    std::shared_ptr<RtspSession> session;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = sessions_.find(fd);
        if (it == sessions_.end())
        {
            LOG_ERROR("RtspServer::ProcessConnectionData: session not found for fd=%d", fd);
            return;
        }
        session = it->second;
    }

    Connection& conn = session->connection();

    if (conn.IsClosed())
    {
        LOG_ERROR("RtspServer::ProcessConnectionData: connection is closed, fd=%d", fd);
        CloseConnection(fd);
        return;
    }

    // 先接收数据到缓冲区
    ssize_t ret = conn.Recv();
    if (ret < 0)
    {
        LOG_ERROR("RtspServer::ProcessConnectionData: recv failed, fd=%d", fd);
        CloseConnection(fd);
        return;
    }

    // 循环处理缓冲区中的所有消息（interleaved 帧 + RTSP 请求）
    while (conn.GetReadBufferSize() > 0)
    {
        uint8_t header[4];
        if (conn.Peek(header, 4).ok() && header[0] == 0x24)
        {
            uint16_t frame_len = static_cast<uint16_t>((header[2] << 8) | header[3]);
            size_t total = static_cast<size_t>(4) + frame_len;
            if (conn.GetReadBufferSize() < total)
            {
                break;
            }
            LOG_TRACE("RtspServer::ProcessConnectionData: skip interleaved frame, fd=%d, channel=%d, len=%d", fd,
                      header[1], frame_len);
            conn.Consume(total);
            continue;
        }

        // 零拷贝：直接在 RingBuffer 上查找请求结束符 \r\n\r\n
        size_t request_end = conn.FindSubstring("\r\n\r\n", 4);
        if (request_end == RingBuffer::npos)
        {
            if (conn.GetReadBufferSize() >= kMaxRtspRequestDataLen)
            {
                LOG_WARN("RtspServer::ProcessConnectionData: request too large, fd=%d", fd);
                CloseConnection(fd);
                return;
            }
            break;
        }

        size_t request_len = request_end + 4;
        if (request_len > conn.GetReadBufferSize())
        {
            break;
        }

        const char* request_ptr = conn.GetReadBuffer();
        LOG_DEBUG("RtspServer::ProcessConnectionData: complete request, fd=%d, size=%zu", fd, request_len);

        Status process_status = session->ProcessData(request_ptr, request_len);
        if (!process_status.ok())
        {
            LOG_WARN("RtspServer::ProcessConnectionData: process failed, status=%s",
                     process_status.ToString().c_str());
        }

        conn.Consume(request_len);

        if (conn.NeedFlush())
        {
            event_loop_.ModifyFd(
                fd, EventType(static_cast<int>(EventType::kRead) | static_cast<int>(EventType::kWrite)));
        }
    }
}

void RtspServer::CheckTimeouts()
{
    // 两个超时都为 0 时无需检查
    if (connection_timeout_sec_ <= 0 && session_timeout_sec_ <= 0)
    {
        return;
    }

    // 收集超时的 fd，在锁外关闭
    std::vector<int> timed_out_fds;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& pair : sessions_)
        {
            if (pair.second->IsTimedOut(connection_timeout_sec_, session_timeout_sec_))
            {
                timed_out_fds.push_back(pair.first);
            }
        }
    }

    for (int fd : timed_out_fds)
    {
        LOG_INFO("RtspServer::CheckTimeouts: session timed out, fd=%d", fd);
        timed_out_sessions_.fetch_add(1, std::memory_order_relaxed);
        CloseConnection(fd);
    }
}

void RtspServer::GetInfo(RtspForwardInfo* info)
{
    if (!info)
    {
        return;
    }

    int active = 0;
    int playing = 0;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& pair : sessions_)
        {
            active++;
            if (pair.second->state() == RtspSessionState::kPlaying)
            {
                playing++;
            }
        }
    }

    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time_);

    info->port = port_;
    info->max_sessions = max_sessions_;
    info->running = IsRunning() ? 1 : 0;
    info->active_sessions = active;
    info->playing_sessions = playing;
    info->total_connections = total_connections_.load(std::memory_order_relaxed);
    info->timed_out_sessions = timed_out_sessions_.load(std::memory_order_relaxed);
    info->uptime_sec = static_cast<uint64_t>(uptime.count());
}

}  // namespace rtsp_forward
