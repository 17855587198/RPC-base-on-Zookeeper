#ifndef _Zrpcprovider_H__
#define _Zrpcprovider_H__
#include "google/protobuf/service.h"
#include "zookeeperutil.h"
#include<muduo/net/TcpServer.h>
#include<muduo/net/EventLoop.h>
#include<muduo/net/InetAddress.h>
#include<muduo/net/TcpConnection.h>
#include<google/protobuf/descriptor.h>
#include<functional>
#include<string>
#include<unordered_map>
/*
框架提供的专门用于发布rpc服务对象的网络对象类
*/
class ZrpcProvider
{
public:
    //这里是提供给外部使用的，可以发布rpc方法的函数接口。
    void NotifyService(google::protobuf::Service* service);//使用基类指针接受任意的继承子类服务对象指针(// 这里是框架提供给外部使用的，可以发布rpc方法的函数接口)
      ~ZrpcProvider();
    //启动rpc服务节点，开始提供rpc远程网络调用服务
    void Run();
    
    // 新增：心跳相关功能
    void EnableHeartbeatResponse(bool enable = true);
    bool IsHeartbeatResponseEnabled() const;
    
private:
    muduo::net::EventLoop event_loop;
    struct ServiceInfo
    {
        google::protobuf::Service* service;
        std::unordered_map<std::string, const google::protobuf::MethodDescriptor*> method_map;
    };
    std::unordered_map<std::string, ServiceInfo>service_map;//保存服务对象和rpc方法
    
    void OnConnection(const muduo::net::TcpConnectionPtr& conn);
    void OnMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buffer, muduo::Timestamp receive_time);
    void SendRpcResponse(const muduo::net::TcpConnectionPtr& conn, google::protobuf::Message* response);
    
    // 新增：心跳处理
    void HandleHeartbeat(const muduo::net::TcpConnectionPtr& conn);
    bool IsHeartbeatRequest(const std::string& data);
    
    // 新增：心跳响应开关
    bool m_heartbeat_response_enabled;
};
#endif 







