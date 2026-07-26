#ifndef RTSP_FORWARD_EPOLL_LOOP_H_
#define RTSP_FORWARD_EPOLL_LOOP_H_

#include <sys/epoll.h>

#include <atomic>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "event_loop.h"

namespace rtsp_forward
{

// EpollLoop - epoll 实现（Linux）
class EpollLoop : public EventLoop
{
public:
    EpollLoop();
    ~EpollLoop() override;

    // 禁止拷贝和移动
    EpollLoop(const EpollLoop&) = delete;
    EpollLoop& operator=(const EpollLoop&) = delete;
    EpollLoop(EpollLoop&&) = delete;
    EpollLoop& operator=(EpollLoop&&) = delete;

    Status Init() override;
    Status AddFd(int fd, EventType events, EventCallback callback) override;
    Status ModifyFd(int fd, EventType events) override;
    Status RemoveFd(int fd) override;
    int Wait(int timeout_ms, std::vector<EventResult>& results) override;
    void Run() override;
    void Stop() override;

private:
    // 将 EventType 转换为 epoll events
    static uint32_t ToEpollEvents(EventType events);

    int epoll_fd_;
    std::vector<struct ::epoll_event> events_;
    std::unordered_map<int, EventCallback> callbacks_;
    std::atomic<bool> running_;
};

}  // namespace rtsp_forward

#endif  // RTSP_FORWARD_EPOLL_LOOP_H_
