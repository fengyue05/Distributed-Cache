#ifndef _SRC_INCLUDE_CACHE_CACHEGROUP_H_
#define _SRC_INCLUDE_CACHE_CACHEGROUP_H_

#include "cache.h"
#include "slightflight.h"
#include <functional>
#include <string>
#include <memory>
#include <string_view>
#include <atomic>

namespace Litguidyo
{
using DataGetter = std::function<ByteViewOptional(const std::string_view& key)>;

struct GroupStatus {
    std::atomic_int64_t loads;          // 加载次数
    std::atomic_int64_t local_hits;     // 本地缓存命中次数
    std::atomic_int64_t local_misses;   // 本地缓存未命中次数
    std::atomic_int64_t peer_hits;      // 从对等节点获取成功次数
    std::atomic_int64_t peer_misses;    // 从对等节点获取失败次数
    std::atomic_int64_t loader_hits;    // 从加载器获取成功次数
    std::atomic_int64_t loader_errors;  // 从加载器获取失败次数
    std::atomic_int64_t load_duration;  // 加载总耗时（纳秒）};
};

class CacheGroup
{
public:
    CacheGroup() = default;

    CacheGroup(std::string_view name, int64_t bytes, DataGetter getter)
        : cache_(std::make_unique<LRUCache>(bytes)), name_(name), getter_(getter)
    {}

    CacheGroup(const CacheGroup&) = delete;
    auto operator=(const CacheGroup& other) -> CacheGroup = delete;

    CacheGroup(CacheGroup&& other)
    {
        cache_ = std::move(other.cache_);
        name_ = std::move(other.name_);
        getter_ = std::move(other.getter_);
    }

    auto operator=(CacheGroup&& other) -> CacheGroup&
    {
        cache_ = std::move(other.cache_);
        name_ = std::move(other.name_);
        getter_ = std::move(other.getter_);
        return *this;
    }

    auto Get(const std::string_view& key) -> ByteViewOptional;
    bool Set(const std::string_view& key, ByteView data);
    bool Delete(const std::string_view& key);

    // 处理来自其他结点失效的请求
    bool InvalidateFromPeer(const std::string_view& key);

private:
    auto Load(const std::string_view& key) -> ByteViewOptional;
    auto LoadData(const std::string_view& key) -> ByteViewOptional;

    std::unique_ptr<LRUCache> cache_; // 本地缓存
    std::string_view name_;
    std::atomic<bool> is_close_{false};
    DataGetter getter_;
    SingleFlight loader_;
    GroupStatus status_;
};

auto MakeCacheGroup (const std::string_view& name, int64_t bytes, DataGetter getter) -> CacheGroup&;
auto GetCacheGroup (const std::string_view& name) -> CacheGroup*;
}

#endif