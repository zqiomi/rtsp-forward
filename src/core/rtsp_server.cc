#include "rtsp_server.h"

#include <memory>

#include "../net/connection.h"
#include "rtsp_session.h"

namespace rtsp_server
{

RtspServer::RtspServer() : running_(false), port_(0)
{
    LOG_INFO("RtspServer created");
}

RtspServer::~RtspServer()
{
    Stop();
    LOG_INFO("RtspServer destroyed");
}

Status RtspServer::Start(const std::string& ip, int port)
{
    LOG_INFO("RtspServer::Start: ip=%s, port=%d", ip.c_str(), port);

    // 创建监听器
    Status status = listener_.Listen(ip.c_str(), port);
    if (!status.ok())
    {
        LOG_ERROR("RtspServer::Start: listener listen failed, status=%s", status.ToString().c_str());
        return status;
    }

    port_ = port;

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

    running_ = true;
    LOG_INFO("RtspServer::Start: server started on %s:%d", ip.c_str(), port);
    return Status::Ok();
}

void RtspServer::Stop()
{
    if (!running_)
    {
        return;
    }

    LOG_INFO("RtspServer::Stop: stopping server");

    running_ = false;

    // 停止事件循环
    event_loop_.Stop();

    // 关闭所有会话
    for (auto& pair : sessions_)
    {
        pair.second->Close();
    }
    sessions_.clear();

    // 关闭监听器
    listener_.Close();

    LOG_INFO("RtspServer::Stop: server stopped");
}

void RtspServer::Run()
{
    if (!running_)
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
    if (!running_)
    {
        return Status::FailedPrecondition("server not running");
    }

    for (auto& pair : sessions_)
    {
        RtspSession* session = pair.second.get();
        if (session->state() == RtspSessionState::kPlaying)
        {
            Status status = session->ForwardRtp(packet);
            if (!status.ok())
            {
                LOG_WARN("RtspServer::BroadcastRtp: forward failed for session %s", session->session_id().c_str());
            }
        }
    }

    return Status::Ok();
}

void RtspServer::OnNewConnection(int fd)
{
    LOG_DEBUG("RtspServer::OnNewConnection: fd=%d", fd);

    int client_fd = listener_.Accept();
    if (client_fd < 0)
    {
        LOG_ERROR("RtspServer::OnNewConnection: accept failed");
        return;
    }

    LOG_INFO("RtspServer::OnNewConnection: new client connected, fd=%d", client_fd);

    // 创建连接和会话
    Connection* conn = new Connection(client_fd);
    RtspSession* session = new RtspSession(conn);

    // 添加读事件
    Status status = event_loop_.AddFd(client_fd, EventType::kRead,
                                      [this](int fd, EventType type, EventResult result)
                                      {
                                          (void)type;
                                          (void)result;
                                          if (fd > 0)
                                          {
                                              this->OnRead(fd);
                                          }
                                      });

    if (!status.ok())
    {
        LOG_ERROR("RtspServer::OnNewConnection: add client fd failed, status=%s", status.ToString().c_str());
        delete session;
        delete conn;
        return;
    }

    // 保存会话（同时持有 connection 的所有权）
    sessions_[client_fd] = std::unique_ptr<RtspSession>(session);
}

void RtspServer::OnRead(int fd)
{
    LOG_DEBUG("RtspServer::OnRead: fd=%d", fd);
    ProcessConnectionData(fd);
}

void RtspServer::OnWrite(int fd)
{
    LOG_DEBUG("RtspServer::OnWrite: fd=%d", fd);
}

void RtspServer::OnError(int fd)
{
    LOG_DEBUG("RtspServer::OnError: fd=%d", fd);
    CloseConnection(fd);
}

void RtspServer::CloseConnection(int fd)
{
    LOG_DEBUG("RtspServer::CloseConnection: fd=%d", fd);

    auto it = sessions_.find(fd);
    if (it != sessions_.end())
    {
        LOG_INFO("RtspServer::CloseConnection: client disconnected, fd=%d", fd);
        sessions_.erase(it);
    }
}

void RtspServer::ProcessConnectionData(int fd)
{
    auto it = sessions_.find(fd);
    if (it == sessions_.end())
    {
        LOG_ERROR("RtspServer::ProcessConnectionData: session not found for fd=%d", fd);
        return;
    }

    RtspSession* session = it->second.get();
    Connection* conn = session->connection();

    if (!conn || conn->IsClosed())
    {
        LOG_ERROR("RtspServer::ProcessConnectionData: connection is closed, fd=%d", fd);
        CloseConnection(fd);
        return;
    }

    // 先接收数据到缓冲区
    ssize_t ret = conn->Recv();
    if (ret < 0)
    {
        LOG_ERROR("RtspServer::ProcessConnectionData: recv failed, fd=%d", fd);
        CloseConnection(fd);
        return;
    }

    // 尝试读取一行（RTSP 请求以 \r\n\r\n 结尾）
    std::string line;
    bool has_line = conn->ReadLine(line);

    if (has_line)
    {
        LOG_DEBUG("RtspServer::ProcessConnectionData: received data, fd=%d, size=%zu", fd, line.size());
        Status process_status = session->ProcessData(line);
        if (!process_status.ok())
        {
            LOG_WARN("RtspServer::ProcessConnectionData: process failed, status=%s", process_status.ToString().c_str());
        }
    }
    else if (conn->IsClosed())
    {
        LOG_INFO("RtspServer::ProcessConnectionData: connection closed, fd=%d", fd);
        CloseConnection(fd);
    }
}

}  // namespace rtsp_server
