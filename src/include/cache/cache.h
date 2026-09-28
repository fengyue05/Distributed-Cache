#ifndef _SRC_INCLUDE_CACHE_H_
#define _SRC_INCLUDE_CACHE_H_

#include <functional>
#include <list>
#include <mutex>
#include <optional>
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Litguidyo 
{

// 数据使用ByteView来封装
struct ByteView 
{
    std::vector<char> data_{};

    ByteView(const std::string& str)
    {
        data_.resize(str.size());
        std::copy(str.begin(), str.end(), data_.begin());
    }

    [[nodiscard]] auto Len() const -> int64_t { return data_.size();}

    [[nodiscard]] auto ToString () const -> std::string { return std::string(data_.begin(), data_.end());}
};

using ByteViewOptional = std::optional<ByteView>;

// 键值对的完整数据
struct Entry
{
    std::string key_;
    ByteView value_;

    Entry(std::string k, const ByteView& v) : key_(k), value_(v) {}
    
    auto operator==(const Entry& entry) const -> bool 
    {
        return key_ == entry.key_ && value_.ToString() == entry.value_.ToString();
    }
};

// 链表上的每一个元素都对应一个迭代器
class LRUCache 
{
    using EvictedFunc = std::function<void(std::string_view, ByteView)>; // 被淘汰的函数
    using ListElementIter = std::list<Entry>::iterator; // 获得链表的起点（迭代器）

public:
    LRUCache(int max_bytes, const EvictedFunc& evivted_func_ = nullptr)
        : max_bytes_(max_bytes)
        , evivted_func_(evivted_func_)
    {}

    auto Get(const std::string_view& key) -> ByteViewOptional;
    void Set(const std::string_view& key, const ByteView&);
    void Delete(const std::string_view& key);
    void RemoveOldest();

private:
    // 本次选择的是按照字节的总量淘汰内存中的数据
    int64_t bytes_ = 0; // 记录当前又多少数据 （这个不仅要记录key，还要记录value）
    int64_t max_bytes_; // 最高限度
    EvictedFunc evivted_func_;

    std::unordered_map<std::string, ListElementIter> cache_;
    std::list<Entry> list_;
    std::mutex mutex_;
};

}

#endif