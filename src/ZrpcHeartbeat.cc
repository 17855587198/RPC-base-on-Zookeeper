#include "ZrpcHeartbeat.h"
#include "ZrpcLogger.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <iostream>

ZrpcHeartbeat& ZrpcHeartbeat::GetInstance() {
    static ZrpcHeartbeat instance;
    return instance;
}

ZrpcHeartbeat::ZrpcHeartbeat() : m_running(false), m_stop_requested(false) {}

ZrpcHeartbeat::~ZrpcHeartbeat() {
    Stop();
}

void ZrpcHeartbeat::Start() {
    if (m_running.exchange(true)) {
        return;  // 已经在运行
    }
    
    m_stop_requested = false;
    m_heartbeat_thread = std::thread(&ZrpcHeartbeat::HeartbeatWorker, this);
    LOG(INFO) << "ZrpcHeartbeat started";
}

void ZrpcHeartbeat::Stop() {
    if (!m_running.exchange(false)) {
        return;  // 未在运行
    }
    
    m_stop_requested = true;
    m_cv.notify_all();
    
    if (m_heartbeat_thread.joinable()) {
        m_heartbeat_thread.join();
    }
    
    LOG(INFO) << "ZrpcHeartbeat stopped";
}

void ZrpcHeartbeat::RegisterService(const std::string& service_key, 
                                  const std::string& ip, 
                                  uint16_t port,
                                  int timeout_ms) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    ServiceInfo info;
    info.ip = ip;
    info.port = port;
    info.timeout_ms = timeout_ms;
    info.last_heartbeat = std::chrono::steady_clock::now();
    
    m_services[service_key] = info;
    LOG(INFO) << "Service registered for heartbeat: " << service_key << " at " << ip << ":" << port;
}

void ZrpcHeartbeat::UnregisterService(const std::string& service_key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_services.find(service_key);
    if (it != m_services.end()) {
        m_services.erase(it);
        LOG(INFO) << "Service unregistered from heartbeat: " << service_key;
    }
}

bool ZrpcHeartbeat::IsServiceAvailable(const std::string& service_key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_services.find(service_key);
    // 如果服务在列表中，说明还未超时，即可用
    return it != m_services.end();
}

void ZrpcHeartbeat::TriggerHeartbeat(const std::string& service_key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_services.find(service_key);
    if (it != m_services.end()) {
        bool result = DoHeartbeat(it->second.ip, it->second.port);
        
        if (result) {
            it->second.last_heartbeat = std::chrono::steady_clock::now();
        } else {
            LOG(WARNING) << "Heartbeat failed for service: " << service_key;
            // 不立即删除，让定时检测器处理
        }
    }
}

void ZrpcHeartbeat::SetHeartbeatCallback(std::function<bool(const std::string&, const std::string&, uint16_t)> callback) {
    m_heartbeat_callback = callback;
}

void ZrpcHeartbeat::HeartbeatWorker() {
    while (m_running && !m_stop_requested) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto now = std::chrono::steady_clock::now();
            
            // 使用迭代器以便安全删除长期不可用的服务
            for (auto it = m_services.begin(); it != m_services.end(); ) {
                const std::string& service_key = it->first;
                ServiceInfo& info = it->second;
                
                // 检查是否超时（15秒直接删除）
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - info.last_heartbeat);
                if (elapsed.count() >= info.timeout_ms) {
                    LOG(WARNING) << "Service " << service_key << " removed due to timeout";
                    it = m_services.erase(it);
                    continue;
                }
                
                // 执行心跳检测
                bool result;
                if (m_heartbeat_callback) {
                    // 使用自定义回调
                    result = m_heartbeat_callback(service_key, info.ip, info.port);
                } else {
                    // 使用默认心跳检测 - 这里有错误！
                    result = DoHeartbeat(info.ip, info.port);  // 修正：缺少DoHeartbeat调用
                }
                
                if (result) {
                    info.last_heartbeat = now;
                } else {
                    LOG(WARNING) << "Heartbeat failed for service: " << service_key << " at " << info.ip << ":" << info.port;
                    // 不立即删除，让超时检测处理
                }
                
                ++it;
            }
        }
        
        // 等待下一次心跳检测 - 这里体现了5秒间隔
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait_for(lock, std::chrono::seconds(HEARTBEAT_INTERVAL), [this] { return m_stop_requested.load(); });
    }
}

bool ZrpcHeartbeat::DoHeartbeat(const std::string& ip, uint16_t port) {
    return CreateHeartbeatConnection(ip, port);
}

bool ZrpcHeartbeat::CreateHeartbeatConnection(const std::string& ip, uint16_t port) {
    // 创建socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        return false;
    }
    
    // 设置超时
    struct timeval timeout;
    timeout.tv_sec = 3;  // 3秒超时
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    // 设置服务器地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());
    
    // 尝试连接
    bool success = false;
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0) {
        success = true;
    }
    
    close(sockfd);
    return success;
}
