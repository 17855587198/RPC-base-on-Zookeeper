#ifndef _Zrpcchannel_h_
#define _Zrpcchannel_h_
// 此类是继承自google::protobuf::RpcChannel
// 目的是为了给客户端进行方法调用的时候，统一接收的
#include <google/protobuf/service.h>
#include "zookeeperutil.h"
#include "ZrpcHeartbeat.h"
#include <mutex>

class ZrpcChannel : public google::protobuf::RpcChannel
{
public:
    ZrpcChannel(bool connectNow);
    virtual ~ZrpcChannel()
    {
    }
    void CallMethod(const ::google::protobuf::MethodDescriptor *method,
                    ::google::protobuf::RpcController *controller,
                    const ::google::protobuf::Message *request,
                    ::google::protobuf::Message *response,
                    ::google::protobuf::Closure *done) override; // override可以验证是否是虚函数
    
    // 新增：心跳相关功能
    void EnableHeartbeat(bool enable = true);
    bool IsHeartbeatEnabled() const;
    
private:
    int m_clientfd; // 存放客户端套接字
    std::string service_name;
    std::string m_ip;
    uint16_t m_port;
    std::string method_name;
    int m_idx; // 用来划分服务器ip和port的下标
    bool newConnect(const char *ip, uint16_t port);
    bool newConnectWithTimeout(const char *ip, uint16_t port, int timeout_ms);
    std::string QueryServiceHost(ZkClient *zkclient, std::string service_name, std::string method_name, int &idx);
    
    // 新增：心跳相关成员
    bool m_heartbeat_enabled;
    std::string m_service_key;
    mutable std::mutex m_mutex;
};
#endif
