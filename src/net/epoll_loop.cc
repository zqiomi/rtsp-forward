#include "epoll_loop.h"

#include <errno.h>
#include <sys/epoll.h>

#include <cstring>

#include "util/constants.h"
#include "util/log.h"

namespace rtsp_forward
{

EpollLoop::EpollLoop() : running_(false)
{
    LOG_DEBUG("EpollLoop created");
}

EpollLoop::~EpollLoop()
{
    Stop();
    LOG_DEBUG("EpollLoop destroyed");
}

Status EpollLoop::Init()
{
    if (epoll_fd_.IsValid())
    {
        return Status::Ok();
    }

    int fd = epoll_create1(EPOLL_CLOEXEC);
    if (fd < 0)
    {
        LOG_ERROR("epoll_create1 failed");
        return Status::Error("epoll_create1 failed");
    }

    epoll_fd_.Reset(fd);
    events_.resize(kEpollEventsSize);
    running_ = true;
    LOG_DEBUG("EpollLoop initialized, epoll_fd=%d", fd);
    return Status::Ok();
}

Status EpollLoop::AddFd(int fd, EventType events, EventCallback callback)
{
    if (!epoll_fd_.IsValid())
    {
        return Status::Error("epoll not initialized");
    }

    if (fd < 0)
    {
        return Status::InvalidArgument("invalid fd");
    }

    struct ::epoll_event ev;
    ev.events = ToEpollEvents(events);
    ev.data.fd = fd;

    int ret = epoll_ctl(epoll_fd_.fd(), EPOLL_CTL_ADD, fd, &ev);
    if (ret < 0)
    {
        LOG_ERROR("epoll_ctl ADD failed for fd=%d", fd);
        return Status::NetworkError("epoll_ctl ADD failed");
    }

    callbacks_[fd] = callback;
    LOG_DEBUG("EpollLoop::AddFd: fd=%d, events=%d", fd, static_cast<int>(events));
    return Status::Ok();
}

Status EpollLoop::ModifyFd(int fd, EventType events)
{
    if (!epoll_fd_.IsValid())
    {
        return Status::Error("epoll not initialized");
    }

    if (fd < 0)
    {
        return Status::InvalidArgument("invalid fd");
    }

    struct ::epoll_event ev;
    ev.events = ToEpollEvents(events);
    ev.data.fd = fd;

    int ret = epoll_ctl(epoll_fd_.fd(), EPOLL_CTL_MOD, fd, &ev);
    if (ret < 0)
    {
        if (errno != ENOENT)
        {
            LOG_ERROR("epoll_ctl MOD failed for fd=%d: %s", fd, strerror(errno));
        }
        return Status::NetworkError("epoll_ctl MOD failed");
    }

    LOG_DEBUG("EpollLoop::ModifyFd: fd=%d, events=%d", fd, static_cast<int>(events));
    return Status::Ok();
}

Status EpollLoop::RemoveFd(int fd)
{
    if (!epoll_fd_.IsValid())
    {
        return Status::Error("epoll not initialized");
    }

    if (fd < 0)
    {
        return Status::InvalidArgument("invalid fd");
    }

    int ret = epoll_ctl(epoll_fd_.fd(), EPOLL_CTL_DEL, fd, nullptr);
    if (ret < 0)
    {
        LOG_ERROR("epoll_ctl DEL failed for fd=%d", fd);
        return Status::NetworkError("epoll_ctl DEL failed");
    }

    callbacks_.erase(fd);
    LOG_DEBUG("EpollLoop::RemoveFd: fd=%d", fd);
    return Status::Ok();
}

int EpollLoop::Wait(int timeout_ms, std::vector<EventResult>& results)
{
    if (!epoll_fd_.IsValid())
    {
        LOG_ERROR("epoll not initialized");
        return -1;
    }

    int nfds = epoll_wait(epoll_fd_.fd(), events_.data(), events_.size(), timeout_ms);
    if (nfds < 0)
    {
        if (errno == EINTR)
        {
            LOG_DEBUG("epoll_wait interrupted");
            return 0;
        }
        LOG_ERROR("epoll_wait failed");
        return -1;
    }

    results.clear();
    for (int i = 0; i < nfds; ++i)
    {
        EventResult result;
        result.fd = events_[i].data.fd;
        result.events = 0;
        if (events_[i].events & EPOLLIN)
        {
            result.events |= static_cast<int>(EventType::kRead);
        }
        if (events_[i].events & EPOLLOUT)
        {
            result.events |= static_cast<int>(EventType::kWrite);
        }
        if (events_[i].events & (EPOLLERR | EPOLLHUP))
        {
            result.events |= static_cast<int>(EventType::kError);
        }
        results.push_back(result);
    }

    LOG_TRACE("EpollLoop::Wait: %d events", nfds);
    return nfds;
}

void EpollLoop::Run()
{
    if (!epoll_fd_.IsValid())
    {
        LOG_ERROR("epoll not initialized");
        return;
    }

    running_ = true;
    LOG_INFO("EpollLoop::Run: entering event loop");

    while (running_)
    {
        std::vector<EventResult> results;
        int nfds = Wait(kEpollWaitTimeoutMs, results);
        if (nfds < 0)
        {
            LOG_ERROR("EpollLoop::Run: wait failed");
            continue;
        }

        for (const auto& result : results)
        {
            auto it = callbacks_.find(result.fd);
            if (it != callbacks_.end())
            {
                EventCallback cb = it->second;
                EventType type = static_cast<EventType>(result.events);
                cb(result.fd, type, result);
            }
        }
    }

    LOG_INFO("EpollLoop::Run: event loop exited");
}

void EpollLoop::Stop()
{
    running_ = false;
    LOG_DEBUG("EpollLoop::Stop: stopping event loop");
}

uint32_t EpollLoop::ToEpollEvents(EventType events)
{
    uint32_t ev = 0;
    if (static_cast<int>(events) & static_cast<int>(EventType::kRead))
    {
        ev |= EPOLLIN;
    }
    if (static_cast<int>(events) & static_cast<int>(EventType::kWrite))
    {
        ev |= EPOLLOUT;
    }
    return ev;
}

}  // namespace rtsp_forward
