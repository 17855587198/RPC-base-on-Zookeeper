#ifndef _Zrpccontroller_H
#define _Zrpccontroller_H

#include<google/protobuf/service.h>
#include<string>
#include<chrono>
#include<atomic>
//用于描述RPC调用的控制器
//其主要作用是跟踪RPC方法调用的状态、错误信息并提供控制功能(如取消调用)。
class Zrpccontroller:public google::protobuf::RpcController
{
public:
 Zrpccontroller();
 void Reset();
 bool Failed() const;
std::string ErrorText() const;
void SetFailed(const std::string &reason);

//目前未实现具体的功能
void StartCancel();
bool IsCanceled() const;
void NotifyOnCancel(google::protobuf::Closure* callback);

// 新增：超时控制功能
void SetTimeout(int timeout_ms);
int GetTimeout() const;
bool IsTimeout() const;
void SetStartTime();
void CheckTimeout();

private:
 bool m_failed;//RPC方法执行过程中的状态
 std::string m_errText;//RPC方法执行过程中的错误信息
 
 // 新增：超时控制相关成员
 int m_timeout_ms;  // 超时时间（毫秒）
 std::chrono::steady_clock::time_point m_start_time;  // 开始时间
 std::atomic<bool> m_canceled;  // 取消标志
};

#endif