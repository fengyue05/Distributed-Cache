#ifndef _SRC_INCLUDE_CACHE_SLIGHTFLIGHT_H_
#define _SRC_INCLUDE_CACHE_SLIGHTFLIGHT_H_

#include <future>
#include <optional>
#include <functional>
#include <string>
#include <system_error>
#include <unordered_map>
#include "cache.h"

namespace Litguidyo
{
class SingleFlight
{
    using Result = std::optional<ByteView>;
    using Func = std::function<Result()>;

public:
    Result Do(const std::string& key, Func func)
    {
        std::unique_lock<std::mutex> lock(mutex_);

        // 检查是否有进行中的调用
        if (map_.find(key) != map_.end())
        {
            auto existing_call = map_[key];
            lock.unlock(); // 释放组锁，避免阻塞其他键的处理

            // 直接等待 future 的结果
            auto result = existing_call->fut.get();
            return result;
        }

        auto new_call = std::make_shared<Call>();
        map_[key] = new_call;
        lock.unlock();

        // 执行用户函数并设置promise
        Result val = func();
        new_call->prom.set_value(val);

        // 确保从映射中删除条目
        std::lock_guard<std::mutex> lock_(mutex_);
        map_.erase(key);
        return val;
    }

private:
    struct Call 
    {
        /*
        std::promise 是 C++11 提供的一个工具，用来把结果从一个线程传给另一个线程。
        它通常和 std::future 配合使用
        promise：负责设置结果。
        future：负责等待并获取结果。
        一个 promise 通常只能设置一次结果，不适合反复传数据
        future.get() 只能调用一次；需要多次读取可以用 std::shared_future
        如果 promise 销毁时还没提供结果，接收方的 get() 会抛出 broken_promise 对应的 std::future_error
        */ 
        std::promise<Result> prom;
        // get_future()：从 promise 拿到接收结果的 future  share()：把这个 future 转成可以共享的 shared_future
        std::shared_future<Result> fut = prom.get_future().share();
    };

    std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<Call>> map_; // map_ 存储的都是正在进行的调用
};

}


#endif