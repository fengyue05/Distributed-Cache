#ifndef _SRC_INCLUDE_CACHE_CONSISTENT_HASH_H_
#define _SRC_INCLUDE_CACHE_CONSISTENT_HASH_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <fmt/core.h>

namespace Litguidyo
{
uint32_t Crc32IEEE(const std::string_view& data);

// 一致性哈希的配置
struct HashConfig
{
    int replicas;  // 每个真实结点对应的虚拟结点个数
    int min_replicas;
    int max_replicas;
    std::function<uint32_t(const std::string_view&)> hash_func; 
    double load_balance_threadshold; // 负载均衡值，超过此值触发虚拟节点调整
};

const HashConfig DefaultConfig = {10, 10, 200, 
            [](const std::string_view& data) -> uint32_t {return Crc32IEEE(data);},
             0.25};
 
// 虚拟结点用数字表示，真实结点用字符表示
class ConsistentHashMap
{
public:
    explicit ConsistentHashMap(HashConfig cfg = DefaultConfig);

    ~ConsistentHashMap();

    [[nodiscard]] bool Add(const std::vector<std::string_view>& nodes);
    [[nodiscard]] bool Remove(const std::string_view& node);
    // 获取结点
    [[nodiscard]] auto Get(const std::string_view& key) -> std::string_view;
    // 获取负载统计信息
    [[nodiscard]] auto GetStarts() -> std::unordered_map<std::string, double>;

private:
    // 添加结点的虚拟结点
    void AddNode(const std::string_view& node, int replicas);

    // 检查并且重新平衡虚拟结点
    void CheckAndRebalance();

    // 重新平衡结点
    void RebalanceNodes();

    // 启动负载均衡器线程
    void StartBalancer();

    mutable std::shared_mutex mutex_; // mutable让 mutex_ 可以在 const 成员函数里被修改

    HashConfig config_;

    std::vector<uint32_t> keys_; // 哈希环（没有真实结点）
    std::unordered_map<uint32_t, std::string_view> hash_map_;  // 哈希环（虚拟结点）到真实结点的映射
    std::unordered_map<std::string_view, int> node_replicas_;  // 每个真实节点对应多少个虚拟节点
    std::unordered_map<std::string_view, std::atomic<long long>> node_counts_;  // 节点被选中了多少次，即请求计数
    std::atomic<long long> total_requests_; // 总请求数
 
    std::thread balancer_thread_; // 负载均衡器的线程
    std::atomic<bool> is_balancer_stop_; // 负载均衡器线程停止的标志
};

}

#endif