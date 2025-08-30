#ifndef _ZrpcHeartbeat_H
#define _ZrpcHeartbeat_H

#include <string>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <functional>

// 心跳保活管理器
class ZrpcHeartbeat {
public:
    static ZrpcHeartbeat& GetInstance();
    
    // 启动心跳检测
    void Start();
    
    // 停止心跳检测
    void Stop();
    
    // 注册需要心跳检测的服务
    void RegisterService(const std::string& service_key, 
                        const std::string& ip, 
                        uint16_t port,
                        int timeout_ms = 15000);
    
    // 取消注册服务
    void UnregisterService(const std::string& service_key);
    
    // 检查服务是否可用
    bool IsServiceAvailable(const std::string& service_key);
    
    // 手动触发心跳检测
    void TriggerHeartbeat(const std::string& service_key);
    
    // 设置心跳检测回调
    void SetHeartbeatCallback(std::function<bool(const std::string&, const std::string&, uint16_t)> callback);

private:
    ZrpcHeartbeat();
    ~ZrpcHeartbeat();
    
    // 禁止拷贝和赋值
    ZrpcHeartbeat(const ZrpcHeartbeat&) = delete;
    ZrpcHeartbeat& operator=(const ZrpcHeartbeat&) = delete;
    
    // 心跳检测线程函数
    void HeartbeatWorker();
    
    // 执行单个服务的心跳检测
    bool DoHeartbeat(const std::string& ip, uint16_t port);
    
    // 创建简单的心跳连接
    bool CreateHeartbeatConnection(const std::string& ip, uint16_t port);

    struct ServiceInfo {
        std::string ip;
        uint16_t port;
        int timeout_ms;
        std::chrono::steady_clock::time_point last_heartbeat;
        // 移除 is_available 字段，因为超时即删除
    };
    
    std::unordered_map<std::string, ServiceInfo> m_services;
    std::thread m_heartbeat_thread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_running;
    std::atomic<bool> m_stop_requested;
    
    // 心跳检测间隔（秒）
    static const int HEARTBEAT_INTERVAL = 5;
    
    // 自定义心跳检测回调
    std::function<bool(const std::string&, const std::string&, uint16_t)> m_heartbeat_callback;
};

#endif
