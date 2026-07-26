#include "rtsp_forward.h"

#include <memory>
#include <vector>

#include "../net/connection.h"
#include "../util/constants.h"
#include "../util/log.h"
#include "rtsp_session.h"

namespace rtsp_forward
{

RtspForward::RtspForward(const std::string& ip, int port, int max_sessions, size_t buffer_size)
    : running_(false),
      port_(port),
      max_sessions_(max_sessions),
      buffer_size_(buffer_size),
      ip_(ip),
      session_sequence_(0)
{
    LOG_INFO("RtspForward created, ip=%s, port=%d, max_sessions=%d, buffer_size=%zu", ip_.c_str(), port_, max_sessions_,
             buffer_size_);
}

RtspForward::~RtspForward()
{
    Stop();
    LOG_INFO("RtspForward destroyed");
}

Status RtspForward::Start()
{
    LOG_INFO("RtspForward::Start: ip=%s, port=%d", ip_.c_str(), port_);

    // 创建监听器
    Status status = listener_.Listen(ip_.c_str(), port_);
    if (!status.ok())
    {
        LOG_ERROR("RtspForward::Start: listener listen failed, status=%s", status.ToString().c_str());
        return status;
    }

    // 初始化事件循环
    status = event_loop_.Init();
    if (!status.ok())
    {
        LOG_ERROR("RtspForward::Start: event loop init failed, status=%s", status.ToString().c_str());
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
        LOG_ERROR("RtspForward::Start: add listener fd failed, status=%s", status.ToString().c_str());
        return status;
    }

    running_ = true;
    LOG_INFO("RtspForward::Start: server started on %s:%d", ip_.c_str(), port_);
    return Status::Ok();
}

void RtspForward::Stop()
{
    if (!running_)
    {
        return;
    }

    LOG_INFO("RtspForward::Stop: stopping server");

    running_ = false;

    // 停止事件循环
    event_loop_.Stop();

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

    LOG_INFO("RtspForward::Stop: server stopped");
}

void RtspForward::Run()
{
    if (!running_)
    {
        LOG_WARN("RtspForward::Run: server not started");
        return;
    }

    LOG_INFO("RtspForward::Run: entering event loop");
    event_loop_.Run();
    LOG_INFO("RtspForward::Run: event loop exited");
}

Status RtspForward::BroadcastRtp(const RtpPacket& packet)
{
    if (!running_)
    {
        return Status::FailedPrecondition("server not running");
    }

    // 短暂持锁，拷贝需要转发的 session 列表
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

    // 释放锁后逐个转发，避免长时间持锁阻塞主线程
    for (auto& session : playing_sessions)
    {
        Status status = session->ForwardRtp(packet);
        if (!status.ok())
        {
            LOG_WARN("RtspForward::BroadcastRtp: forward failed for session %s", session->session_id().c_str());
        }
    }

    return Status::Ok();
}

void RtspForward::OnNewConnection(int fd)
{
    LOG_DEBUG("RtspForward::OnNewConnection: fd=%d", fd);

    // 检查最大会话数限制
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        if (sessions_.size() >= static_cast<size_t>(max_sessions_))
        {
            LOG_WARN("RtspForward::OnNewConnection: max sessions exceeded, current=%zu, max=%d", sessions_.size(),
                     max_sessions_);
            return;
        }
    }

    int client_fd = listener_.Accept();
    if (client_fd < 0)
    {
        LOG_ERROR("RtspForward::OnNewConnection: accept failed");
        return;
    }

    LOG_INFO("RtspForward::OnNewConnection: new client connected, fd=%d", client_fd);

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
        LOG_ERROR("RtspForward::OnNewConnection: add client fd failed, status=%s", status.ToString().c_str());
        return;
    }

    // 创建连接和会话（session 持有 conn 所有权，shared_ptr 保证 session 多线程安全）
    Connection* conn = new Connection(client_fd, buffer_size_);
    std::shared_ptr<RtspSession> session(new RtspSession(this, conn));

    // 保存会话
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_[client_fd] = session;
    }
}

void RtspForward::OnRead(int fd)
{
    LOG_DEBUG("RtspForward::OnRead: fd=%d", fd);
    ProcessConnectionData(fd);
}

void RtspForward::OnWrite(int fd)
{
    LOG_DEBUG("RtspForward::OnWrite: fd=%d", fd);

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

    Connection* conn = session->connection();
    if (!conn || conn->IsClosed())
    {
        CloseConnection(fd);
        return;
    }

    // 刷新写缓冲区
    ssize_t ret = conn->Flush();
    if (ret < 0)
    {
        LOG_ERROR("RtspForward::OnWrite: flush failed, fd=%d", fd);
        CloseConnection(fd);
        return;
    }

    // 如果缓冲区已清空，移除写事件
    if (!conn->NeedFlush())
    {
        event_loop_.ModifyFd(fd, EventType::kRead);
    }
}

void RtspForward::OnError(int fd)
{
    LOG_DEBUG("RtspForward::OnError: fd=%d", fd);
    CloseConnection(fd);
}

void RtspForward::CloseConnection(int fd)
{
    LOG_DEBUG("RtspForward::CloseConnection: fd=%d", fd);

    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(fd);
    if (it != sessions_.end())
    {
        LOG_INFO("RtspForward::CloseConnection: client disconnected, fd=%d", fd);
        // 从事件循环中移除
        event_loop_.RemoveFd(fd);
        sessions_.erase(it);
    }
}

void RtspForward::ProcessConnectionData(int fd)
{
    // 短暂持锁获取 session 引用
    std::shared_ptr<RtspSession> session;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = sessions_.find(fd);
        if (it == sessions_.end())
        {
            LOG_ERROR("RtspForward::ProcessConnectionData: session not found for fd=%d", fd);
            return;
        }
        session = it->second;
    }

    Connection* conn = session->connection();

    if (!conn || conn->IsClosed())
    {
        LOG_ERROR("RtspForward::ProcessConnectionData: connection is closed, fd=%d", fd);
        CloseConnection(fd);
        return;
    }

    // 先接收数据到缓冲区
    ssize_t ret = conn->Recv();
    if (ret < 0)
    {
        LOG_ERROR("RtspForward::ProcessConnectionData: recv failed, fd=%d", fd);
        CloseConnection(fd);
        return;
    }

    // 循环读取完整 RTSP 请求（以空行 \r\n\r\n 结尾）
    std::string request_data;
    std::string line;
    bool request_complete = false;

    while (conn->ReadLine(line))
    {
        if (line.empty())
        {
            // 空行表示请求头结束
            request_complete = true;
            break;
        }
        request_data += line + "\r\n";
    }

    if (request_complete)
    {
        LOG_DEBUG("RtspForward::ProcessConnectionData: complete request, fd=%d, size=%zu", fd, request_data.size());
        Status process_status = session->ProcessData(request_data);
        if (!process_status.ok())
        {
            LOG_WARN("RtspForward::ProcessConnectionData: process failed, status=%s",
                     process_status.ToString().c_str());
        }

        // 处理完请求后检查是否有数据需要刷新
        if (conn->NeedFlush())
        {
            event_loop_.ModifyFd(fd,
                                 EventType(static_cast<int>(EventType::kRead) | static_cast<int>(EventType::kWrite)));
        }
    }
    else if (conn->IsClosed())
    {
        LOG_INFO("RtspForward::ProcessConnectionData: connection closed, fd=%d", fd);
        CloseConnection(fd);
    }
    // 数据不完整，等待下次读事件
}

}  // namespace rtsp_forward
