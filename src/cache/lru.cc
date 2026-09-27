#include "../include/cache/cache.h"
#include <mutex>

namespace Litguidyo
{
auto LRUCache::Get(const std::string_view& key) -> ByteViewOptional
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (cache_.find(key.data()) == cache_.end())
    {
        return std::nullopt;
    }
    auto element = cache_[key.data()];
    auto [k, value] = *element;
    list_.erase(element);
    list_.emplace_front(key.data(), value);
    cache_[key.data()] = list_.begin();
    return value;
}

void LRUCache::Set(const std::string_view& key, const ByteView& value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (cache_.find(key.data()) != cache_.end())
    {
        // 移除老的
        auto element = cache_[key.data()];
        bytes_ += value.Len() - element->value_.Len();
        list_.erase(element);
    }
    else 
    {
        bytes_ += key.size() + value.Len();
    }
    
    // 插入新的
    list_.emplace_front(key.data(), value);
    cache_[key.data()] = list_.begin();

    // 当 LRUCache 中还有缓存时，如果此时 LRUCache 中的容量超过规定大小，就不断将最久未使用的缓存淘汰
    while (max_bytes_ != 0 && bytes_ > max_bytes_ && !list_.empty())
    {
        RemoveOldest();
    }
}

}