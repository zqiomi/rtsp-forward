#include "resource_manager.h"

#include "log.h"

namespace rtsp_server
{

ResourceManager::~ResourceManager()
{
    DumpLeaks();
}

void ResourceManager::Register(Trackable* resource, const char* name)
{
    if (!resource)
    {
        LOG_WARN("ResourceManager::Register: null resource");
        return;
    }

    for (const auto& entry : resources_)
    {
        if (entry.resource == resource)
        {
            LOG_WARN("ResourceManager::Register: resource already registered");
            return;
        }
    }

    resources_.push_back({resource, name});
    LOG_DEBUG("ResourceManager::Register: %s", name);
}

void ResourceManager::Unregister(Trackable* resource)
{
    if (!resource)
    {
        LOG_WARN("ResourceManager::Unregister: null resource");
        return;
    }

    for (auto it = resources_.begin(); it != resources_.end(); ++it)
    {
        if (it->resource == resource)
        {
            LOG_DEBUG("ResourceManager::Unregister: %s", it->name);
            resources_.erase(it);
            return;
        }
    }

    LOG_WARN("ResourceManager::Unregister: resource not found");
}

void ResourceManager::DumpLeaks()
{
    if (resources_.empty())
    {
        LOG_DEBUG("ResourceManager::DumpLeaks: no leaks");
        return;
    }

    LOG_ERROR("ResourceManager::DumpLeaks: found %zu leaked resources:", resources_.size());
    for (const auto& entry : resources_)
    {
        LOG_ERROR("  - %s", entry.name);
    }
}

}  // namespace rtsp_server
