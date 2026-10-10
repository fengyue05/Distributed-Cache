#include "cache/register.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>

#include <etcd/KeepAlive.hpp>
#include <etcd/v3/Transaction.hpp>


namespace Litguidyo
{
bool EtcdRegistry::Register(const std::string& svc_name, std::string addr)
{
    std::string local_ip = GetLocalIP();
    if (local_ip.empty())
    {
        return false;
    }
    // 如果地址只写了端口，就在前面补上本机 IP
    if (!addr.empty() && addr[0] == ':')
    {
        addr = local_ip + addr;
    }
    key_ = "/services/" + svc_name + "/" + addr; 
    
    // 创建租约
    // 向 etcd 申请一个 TTL 为 10 秒的租约，并等待申请结果，这个操作是异步操作，但是因为调用了get（）变成了阻塞等待
    auto lease_resp = etcd_client_->leasegrant(10).get();
    if (!lease_resp.is_ok())
    {
        return false;
    }
    // 从 etcd 的响应中取出租约 ID
    lease_id_ = lease_resp.value().lease();

    // 注册服务
    auto is_ok = etcd_client_->put(key_, addr, lease_id_).get();
    if (!is_ok.is_ok())
    {
        return false;
    }

    // 启动续约线程
    keepalive_thread_ = std::thread{[this] {this->KeepAliveLoop();}};
    return true;
}

void EtcdRegistry::Unregister()
{
    is_stop_ = true;
    if (keepalive_thread_.joinable())
    {
        keepalive_thread_.join();
    }
    if (lease_id_ > 0)
    {
        auto is_ok = etcd_client_->leaserevoke(lease_id_).wait();
        std::cout << "lease" << lease_id_ << "revoked successfully" << std::endl;
    }
}

// 得到一个外地可以访问的地址
auto EtcdRegistry::GetLocalIP() -> std::string 
{
    /*
    这行声明了一个指向 ifaddrs 结构体的指针，通常用来获取 Linux 本机的网络接口信息。
    它包含接口名称、地址，以及指向下一条信息的指针等：
    ifa_name   // 接口名，例如 eth0、lo
    ifa_addr   // 接口地址
    ifa_next   // 下一条记录，形成链表
    */
    struct ifaddrs* ifaddr;
    // 因为 getifaddrs() 获取的就是本机网络接口的地址
    // 所以遍历出来的地址本来就属于本机，不是因为“非 127”才判断为本机地址
    if (getifaddrs(&ifaddr) == -1)
    {
        return "";
    }
    std::string ip;
    for (struct ifaddrs* ifa = ifaddr; ifa; ifa = ifa->ifa_next)
    {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
        {
            continue;
        }
        char buf[INET_ADDRSTRLEN];
        void* addr_ptr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
        // 这个信息是存储在网络里面的，把二进制形式的 IP 地址转换成点分十进制
        inet_ntop(AF_INET, addr_ptr, buf, INET_ADDRSTRLEN);
        std::string candidate{buf};
        if (candidate != "127.0.0.1")
        {
            ip = candidate;
            break;
        }
    }
    freeifaddrs(ifaddr);
    return ip;
}

void EtcdRegistry::KeepAliveLoop()
{
    while(!is_stop_)
    {
        etcd::KeepAlive keepalive{*etcd_client_, 10, lease_id_};
        try 
        {
            keepalive.Check();
        }
        catch (const std::exception& e)
        {
            std::cerr << "keepalive exception :" << e.what() << std::endl;
            keepalive.Cancel();
        } catch (...)
        {
            keepalive.Cancel();
        }

        // 等待间隔 （租约时间的1/3）
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    std::cout << "KeepAlive loop exited for lease {}" << lease_id_ << std::endl;
}
}