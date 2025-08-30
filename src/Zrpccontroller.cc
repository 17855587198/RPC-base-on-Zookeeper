#include "Zrpccontroller.h"

// 构造函数，初始化控制器状态
Zrpccontroller::Zrpccontroller() {
    m_failed = false;  // 初始状态为未失败
    m_errText = "";    // 错误信息初始为空
    m_timeout_ms = 15000;  // 默认超时时间15秒
    m_canceled = false;  // 初始未取消
}

// 重置控制器状态，将失败标志和错误信息清空
void Zrpccontroller::Reset() {
    m_failed = false;  // 重置失败标志
    m_errText = "";    // 清空错误信息
    m_canceled = false;  // 重置取消标志
}

// 判断当前RPC调用是否失败
bool Zrpccontroller::Failed() const {
    return m_failed;  // 返回失败标志
}

// 获取错误信息
std::string Zrpccontroller::ErrorText() const {
    return m_errText;  // 返回错误信息
}

// 设置RPC调用失败，并记录失败原因
void Zrpccontroller::SetFailed(const std::string &reason) {
    m_failed = true;   // 设置失败标志
    m_errText = reason; // 记录失败原因
}

// 新增：超时控制功能实现
void Zrpccontroller::SetTimeout(int timeout_ms) {
    m_timeout_ms = timeout_ms;
}

int Zrpccontroller::GetTimeout() const {
    return m_timeout_ms;
}

void Zrpccontroller::SetStartTime() {
    m_start_time = std::chrono::steady_clock::now();
}

bool Zrpccontroller::IsTimeout() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start_time);
    return duration.count() >= m_timeout_ms;
}

void Zrpccontroller::CheckTimeout() {
    if (IsTimeout()) {
        SetFailed("RPC call timeout after " + std::to_string(m_timeout_ms) + "ms");
    }
}

// 取消功能实现
void Zrpccontroller::StartCancel() {
    m_canceled = true;
    SetFailed("RPC call canceled");
}

bool Zrpccontroller::IsCanceled() const {
    return m_canceled;
}

void Zrpccontroller::NotifyOnCancel(google::protobuf::Closure* callback) {
    // 简单实现：如果已取消则立即执行回调
    if (m_canceled && callback) {
        callback->Run();
    }
}