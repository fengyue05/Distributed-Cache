#ifndef _SRC_INCLUDE_CACHE_SERVER_H_
#define _SRC_INCLUDE_CACHE_SERVER_H_

#include <chrono>
#include <functional>
#include <grpcpp/impl/codegen/server_context.h>
#include <grpcpp/security/server_credentials.h>
#include <string>
#include <vector>
#include <grpcpp/grpcpp.h>
#include <etcd/Client.hpp>

#include "cache/register.h"
#include "kcache.grpc.pb.h"
#include "kcache.pb.h"
namespace Litguidyo
{
// 服务器的配置类，构造函数已经是默认配置
struct ServerOptions
{
    std::vector<std::string> etcd_endpoints; // etcd 服务的地址列表
    std::chrono::milliseconds dial_timeout;  // 建立连接的超时时间
    int max_msg_size;  // 允许的最大消息大小
    bool tls;   // 是否启用 TLS，用于加密通信
    std::string cert_file; // TLS 证书文件的路径，用于提供身份凭据
    std::string key_file;  // 与证书配套的私钥文件路径

    ServerOptions() 
        : etcd_endpoints({"http://127.0.0.1:2379"})
        , dial_timeout(std::chrono::seconds(5))
        , max_msg_size(4 << 20) // 4MB
        , tls(false)
    {}
};

using ServerOption = std::function<void(ServerOptions*)>;

// 把 etcd 地址列表包装成一个配置操作，之后用它修改 ServerOptions
/*
使用方法是先赋值endpoints，然后拿到相对应的函数对象，再把我们的ServerOptions对象传入这个函数
*/
inline auto WithEtcdEndPoints(const std::vector<std::string>& endpoints) -> ServerOption
{
    return [endpoints](ServerOptions* o) {o->etcd_endpoints = endpoints;};
}

inline auto WithDialTimeout(std::chrono::milliseconds timeout) -> ServerOption
{
    return [timeout](ServerOptions* o) { o->dial_timeout = timeout;};
}

inline auto WithTLS(const std::string& certfile, const std::string& keyFile) -> ServerOption
{
    return [certfile, keyFile](ServerOptions* o)
    {
        o->tls = true;
        o->cert_file = certfile;
        o->key_file = keyFile;
    };
}

class CacheServer final : public pb::Cache::Service
{
public:
    CacheServer(const std::string& addr, const std::string& svc_name, ServerOptions opts = ServerOptions{});
    ~CacheServer() = default;
    /*
    context	 保存本次 RPC 调用的信息，例如客户端地址、截止时间和取消状态	context->IsCancelled()
    request	 客户端发来的请求，这里包含要查询的缓存键	request->key()
    response 服务端填写的响应，gRPC 会将它发送给客户端	response->set_value("缓存内容")
    */
    auto Get(grpc::ServerContext* context, const pb::GetRequest* request, pb::GetResponse* response) -> grpc::Status override;
    auto Set(grpc::ServerContext* context, const pb::SetRequest* request, pb::SetResponse* response) -> grpc::Status override;
    auto Delete(grpc::ServerContext* context, const pb::GetRequest* request, pb::DeleteResponse* response) -> grpc::Status override;

    auto Invalidate(grpc::ServerContext* context, const pb::GetRequest* request, pb::InvalidateResponse* response) -> grpc::Status override;

    void Start();

    void Stop();

private:
    auto LoadTLSCredentials(const std::string& cert_file, const std::string& key_file)
        -> std::shared_ptr<grpc::ServerCredentials>;

    std::string addr_;
    std::string svc_name_;

    std::unique_ptr<grpc::Server> grpc_server_;
    std::unique_ptr<EtcdRegistry> etcd_register_;

    std::atomic<bool> is_stop_;
    ServerOptions opts_;
};

}

#endif
