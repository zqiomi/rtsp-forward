#ifndef RTSP_SERVER_EVENT_LOOP_H_
#define RTSP_SERVER_EVENT_LOOP_H_

#include <functional>
#include <vector>

#include "../util/status.h"

namespace rtsp_server
{

// 事件类型
enum class EventType
{
    kRead = 1 << 0,
    kWrite = 1 << 1,
};

// 事件结果
struct EventResult
{
    int fd;
    int events;
};

// 事件回调函数
typedef std::function<void(int fd, EventType type, EventResult result)> EventCallback;

// EventLoop 抽象接口
class EventLoop
{
public:
    virtual ~EventLoop() = default;

    // 初始化
    virtual Status Init() = 0;

    // 添加文件描述符（带回调）
    virtual Status AddFd(int fd, EventType events, EventCallback callback) = 0;

    // 修改文件描述符事件
    virtual Status ModifyFd(int fd, EventType events) = 0;

    // 移除文件描述符
    virtual Status RemoveFd(int fd) = 0;

    // 等待事件
    virtual int Wait(int timeout_ms, std::vector<EventResult>& results) = 0;

    // 运行事件循环
    virtual void Run() = 0;

    // 停止事件循环
    virtual void Stop() = 0;
};

}  // namespace rtsp_server

#endif  // RTSP_SERVER_EVENT_LOOP_H_
