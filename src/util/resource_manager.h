#ifndef RTSP_SERVER_RESOURCE_MANAGER_H_
#define RTSP_SERVER_RESOURCE_MANAGER_H_

#include <string>
#include <vector>

namespace rtsp_server
{

// 可追踪资源接口
class Trackable
{
public:
    virtual ~Trackable() = default;
    virtual const char* GetName() const = 0;
};

// 资源追踪器 - 仅追踪，不负责内存释放
class ResourceManager
{
public:
    ResourceManager() = default;
    ~ResourceManager();

    // 禁止拷贝和移动
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&) = delete;
    ResourceManager& operator=(ResourceManager&&) = delete;

    // 注册资源
    void Register(Trackable* resource, const char* name);

    // 注销资源
    void Unregister(Trackable* resource);

    // 检测并打印未注销的资源（泄漏检测）
    void DumpLeaks();

    // 获取资源数量
    size_t GetResourceCount() const
    {
        return resources_.size();
    }

private:
    struct ResourceEntry
    {
        Trackable* resource;
        const char* name;
    };

    std::vector<ResourceEntry> resources_;
};

}  // namespace rtsp_server

#endif  // RTSP_SERVER_RESOURCE_MANAGER_H_
