#include "../include/cache/cachegroup.h"

#include <fmt/format.h>
#include <pthread.h>

namespace Litguidyo
{
std::unordered_map<std::string_view, CacheGroup> cache_groups;
std::mutex mutex_;

auto MakeCacheGroup(const std::string_view& name, int64_t bytes, DataGetter getter) -> CacheGroup&
{
    if (getter == nullptr)
    {
        std::exit(1);
    }
    std::lock_guard<std::mutex> lock(mutex_);
    cache_groups[name] = std::move(CacheGroup{name, bytes, getter});
    return cache_groups[name];
}

auto GetCacheGroup(const std::string_view& name) -> CacheGroup*
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (cache_groups.find(name) == cache_groups.end())
    {
        return nullptr;
    }
    return &cache_groups[name];
}

auto CacheGroup::Load(const std::string_view& key) -> ByteViewOptional
{
    auto ret = loader_.Do(key.data(), [&] {return LoadData(key);});
    if (!ret)
    {
        return std::nullopt;
    }
    // 更新本地缓存
    cache_->Set(key, ret.value());
    return ret;
}

auto CacheGroup::LoadData(const std::string_view& key) -> ByteViewOptional
{
    auto val = getter_(key);
    if (!val)
    {
        return std::nullopt;
    }
    status_.local_hits++; // 本地缓存次数加1
    return val;
}

auto CacheGroup::Get(const std::string_view& key) -> ByteViewOptional
{
    if (is_close_)
    {
        return std::nullopt;
    }
    if (key == "")
    {
        return std::nullopt;
    }
    // 先从本地缓存中获取
    auto ret = cache_->Get(key);
    if (ret)
    {
        status_.local_hits++; // 本地缓存命中+1
        return ret;
    }

    status_.local_misses++; // 本地缓存未命中+1
    return Load(key);
}

bool CacheGroup::Set(const std::string_view& key, ByteView data)
{
    if (is_close_)
    {
        return false;
    }
    if (key.empty())
    {
        return false;
    }
    cache_->Set(key, data);
    return true;
}

bool CacheGroup::Delete(const std::string_view& key)
{
    if (is_close_)
    {
        return false;
    }
    if (key.empty())
    {
        return false;
    }
    cache_->Delete(key);
    return true;
}

bool CacheGroup::InvalidateFromPeer(const std::string_view& key)
{
    if (is_close_)
    {
        return false;
    }
    if (key.empty())
    {
        return false;
    }
    // 其他节点通知当前节点：这个 key 的缓存可能已经过期，不能继续用了，所以要删除本地这份缓存，避免后续返回旧数据
    /*
    A 的缓存：用户名字 = "张三"
    B 修改了数据：用户名字 = "李四"
          ↓ 
    B 向 A 发送失效请求
          ↓
    A 删除本地缓存中的 "张三"
          ↓
    下次查询时缓存未命中，再走正常加载流程获取新数据
    */
    cache_->Delete(key);
    return true;
}

}