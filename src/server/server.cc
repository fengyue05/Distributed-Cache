#include "cache/server.h"

#include "cache/cachegroup.h"
#include "kcache.pb.h"
#include <grpcpp/impl/channel_argument_option.h>
#include <grpcpp/impl/codegen/status.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server_builder.h>
#include <stdexcept>

namespace Litguidyo
{
CacheServer::CacheServer(const std::string& addr, const std::string& svc_name, ServerOptions opts)
    : addr_(addr), svc_name_(svc_name), opts_(opts)
    {
        // 创建etcd住粗器，传入的是etcd的地址 
        etcd_register_ = std::make_unique<EtcdRegistry>(opts_.etcd_endpoints[0]);
        if (!etcd_register_->Register(svc_name, addr_))
        {
            throw std::runtime_error("[cache] failed to register service with etcd");
        }
    }

auto CacheServer::Get(grpc::ServerContext* context, const pb::GetRequest* request, pb::GetResponse* response) -> grpc::Status
{
    // 先找到对应的组
    auto group = GetCacheGroup(request->group());
    if (!group)
    {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "Group not found");
    }
    // 在组里面再得到对应的值
    auto value = group->Get(request->key());
    if (!value)
    {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "Key not found");
    }
    response->set_value(value->ToString());
    return grpc::Status::OK;
}

auto CacheServer::Set(grpc::ServerContext* context, const pb::SetRequest* request, pb::SetResponse* response) -> grpc::Status
{
    auto group = GetCacheGroup(request->group());
    if (!group)
    {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "Group not found");
    }

    bool is_set = group->Set(request->key(), request->value());
    response->set_success(is_set);
    return grpc::Status::OK;
}

auto Delete(grpc::ServerContext* context, const pb::GetRequest* request, pb::DeleteResponse* response) -> grpc::Status
{
    auto group = GetCacheGroup(request->group());
    if (!group)
    {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "Group not found");
    }
    bool is_delete = group->Delete(request->key());
    response->set_success(is_delete);
    return grpc::Status::OK;
}

auto Invalidate(grpc::ServerContext* context, const pb::GetRequest* request, pb::InvalidateResponse* response) -> grpc::Status
{
    auto group = GetCacheGroup(request->group());
    if (!group)
    {
        return grpc::Status(grpc::StatusCode::NOT_FOUND, "Group not found");
    }
    // RPC 调用总是来自其他的结点，调用InvalidateFromPeer避免循环传播
    // 删除这个组里面的key，之后就无法找到这个key
    bool is_invalidate = group->InvalidateFromPeer(request->key());
    response->set_success(is_invalidate);
    return grpc::Status::OK;
}


void CacheServer::Start()
{
    try 
    {
        // 配置gRPC服务器的选项,把配置信息赋值给builder，然后通过builder构建gRPC_server
        grpc::ServerBuilder builder;

        builder.SetMaxReceiveMessageSize(opts_.max_msg_size);
        builder.SetMaxSendMessageSize(opts_.max_msg_size);

        // 配置TLS或非安全连接
        if (opts_.tls)
        {
            auto creds = LoadTLSCredentials(opts_.cert_file, opts_.key_file);
            if (!creds)
            {
                throw std::runtime_error("Failed to load TLS credentials");
            }
            // 绑定监听端口
            builder.AddListeningPort(addr_, creds);
        }
        else 
        {
            // grpc::InsecureServerCredentials() 是一个函数，返回用于非 TLS 连接的服务器凭据
            builder.AddListeningPort(addr_, grpc::InsecureServerCredentials());
        }

        // 启用默认健康检查服务
        // 健康检查服务就是让其他程序询问："这个服务器现在能正常提供服务吗"
        grpc::EnableDefaultHealthCheckService(true);
        builder.SetOption(grpc::MakeChannelArgumentOption(GRPC_ARG_KEEPALIVE_TIME_MS, 30000));
        builder.SetOption(grpc::MakeChannelArgumentOption(GRPC_ARG_KEEPALIVE_TIMEOUT_MS, 5000));

        // 注册服务
        // 把当前对象注册为 RPC 请求的处理者，让 gRPC 知道收到请求后该调用谁的方法
        // 因为我们继承了CacheServer，但是重写了里面的很多方法，所以需要重新注册我的服务器实现
        // this传入的是我们实现的CacheServer
        builder.RegisterService(this);

        // 构建并启动服务器
        grpc_server_ = builder.BuildAndStart();
        if (!grpc_server_)
        {
            throw std::runtime_error("Failed to build and start gRPC server");
        }

        // 从已经创建的 gRPC 服务器中，获取健康检查服务的指针
        auto health_service = grpc_server_->GetHealthCheckService();
        if (health_service)
        {
            health_service->SetServingStatus(svc_name_, true);
        }

        is_stop_ = false;

        std::cout << "gRPC Server start success at " << addr_ << std::endl;

        grpc_server_->Wait();
    }
    catch (const std::exception& e)
    {
        printf("Failed to start gRPC Server: %s\n", e.what());
        throw;
    }
}

void CacheServer::Stop()
{
    is_stop_ = true;
    if (etcd_register_)
    {
        // 释放etcd的注册器
        etcd_register_->Unregister();
        etcd_register_.reset();
    }
    if (grpc_server_)
    {
        // 关闭gRPC服务
        grpc_server_->Shutdown();
        grpc_server_.reset();
    }
    std::cout << "gRPC Server " << addr_ << "stopped" << std::endl;
}
}
