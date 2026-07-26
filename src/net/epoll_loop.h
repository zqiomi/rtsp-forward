#ifndef RTSP_FORWARD_EPOLL_LOOP_H_
#define RTSP_FORWARD_EPOLL_LOOP_H_

#include <sys/epoll.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

#include "fd_guard.h"
#include "util/status.h"

namespace rtsp_forward
{

enum class EventType
{
    kRead = 1 << 0,
    kWrite = 1 << 1,
    kError = 1 << 2,
};

struct EventResult
{
    int fd;
    int events;
};

typedef std::function<void(int fd, EventType type, EventResult result)> EventCallback;

class EpollLoop
{
public:
    EpollLoop();
    ~EpollLoop();

    EpollLoop(const EpollLoop&) = delete;
    EpollLoop& operator=(const EpollLoop&) = delete;
    EpollLoop(EpollLoop&&) = delete;
    EpollLoop& operator=(EpollLoop&&) = delete;

    Status Init();
    Status AddFd(int fd, EventType events, EventCallback callback);
    Status ModifyFd(int fd, EventType events);
    Status RemoveFd(int fd);
    int Wait(int timeout_ms, std::vector<EventResult>& results);
    void Run();
    void Stop();

private:
    static uint32_t ToEpollEvents(EventType events);

    FdGuard epoll_fd_;
    std::atomic<bool> running_;
    std::vector<struct ::epoll_event> events_;
    std::unordered_map<int, EventCallback> callbacks_;
};

}  // namespace rtsp_forward

#endif  // RTSP_FORWARD_EPOLL_LOOP_H_
