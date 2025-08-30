#ifndef _Zrpc_CONNECTION_POOL_H_
#define _Zrpc_CONNECTION_POOL_H_

#include <string>
#include <memory>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>

// 连接对象
class ZrpcConnection {
public:
    ZrpcConnection(const std::string& host, uint16_t port);
    ~ZrpcConnection();
    
    // 连接操作
    bool Connect(int timeout_ms = 5000);
    void Close();
    bool IsValid() const;
    bool IsExpired() const;
    
    // 数据发送接收
    bool Send(const char* data, size_t len);
    bool Receive(char* buffer, size_t len);
    
    // 获取连接信息
    int GetSocket() const { return m_socket; }
    const std::string& GetHost() const { return m_host; }
    uint16_t GetPort() const { return m_port; }
    
    // 连接状态管理
    void UpdateLastUsed();
    std::chrono::steady_clock::time_point GetLastUsed() const;
    
private:
    std::string m_host;
    uint16_t m_port;
    int m_socket;
    std::atomic<bool> m_connected;
    std::chrono::steady_clock::time_point m_last_used;
    std::chrono::steady_clock::time_point m_created_time;
    
    // 连接超时设置（30分钟）
    static const int CONNECTION_TIMEOUT_SECONDS = 1800;
    
    bool ConnectInternal(int timeout_ms);
};

// 连接池配置
struct ConnectionPoolConfig {
    size_t max_connections = 100;      // 最大连接数
    size_t min_connections = 5;        // 最小连接数
    size_t initial_connections = 10;   // 初始连接数
    int connection_timeout_ms = 5000;  // 连接超时（毫秒）
    int idle_timeout_seconds = 300;    // 空闲超时（秒）
    int max_idle_time_seconds = 1800;  // 最大空闲时间（秒）
    bool enable_heartbeat = true;      // 是否启用心跳检测
    int heartbeat_interval_seconds = 60; // 心跳检测间隔（秒）
};

// 连接池
class ZrpcConnectionPool {
public:
    static ZrpcConnectionPool& GetInstance();
    
    // 初始化连接池
    bool Initialize(const ConnectionPoolConfig& config = ConnectionPoolConfig());
    void Shutdown();
    
    // 获取和归还连接
    std::shared_ptr<ZrpcConnection> GetConnection(const std::string& host, uint16_t port);
    void ReturnConnection(std::shared_ptr<ZrpcConnection> conn);
    
    // 连接池状态
    size_t GetTotalConnections(const std::string& endpoint);
    size_t GetIdleConnections(const std::string& endpoint);
    size_t GetActiveConnections(const std::string& endpoint);
    
    // 统计信息
    struct PoolStats {
        size_t total_pools = 0;
        size_t total_connections = 0;
        size_t active_connections = 0;
        size_t idle_connections = 0;
        size_t requests_served = 0;
        size_t connection_creates = 0;
        size_t connection_destroys = 0;
        double pool_hit_rate = 0.0;
    };
    PoolStats GetStats() const;
    
private:
    ZrpcConnectionPool() = default;
    ~ZrpcConnectionPool() = default;
    ZrpcConnectionPool(const ZrpcConnectionPool&) = delete;
    ZrpcConnectionPool& operator=(const ZrpcConnectionPool&) = delete;
    
    // 连接池管理
    struct EndpointPool {
        std::queue<std::shared_ptr<ZrpcConnection>> idle_connections;
        std::unordered_map<ZrpcConnection*, std::shared_ptr<ZrpcConnection>> active_connections;
        size_t total_connections = 0;
        std::mutex mutex;
        std::condition_variable cv;
        
        // 统计信息
        std::atomic<size_t> requests_served{0};
        std::atomic<size_t> connection_creates{0};
        std::atomic<size_t> connection_destroys{0};
        std::atomic<size_t> pool_hits{0};
        std::atomic<size_t> pool_misses{0};
    };
    
    std::unordered_map<std::string, std::unique_ptr<EndpointPool>> m_pools;
    mutable std::shared_mutex m_pools_mutex;
    
    ConnectionPoolConfig m_config;
    std::atomic<bool> m_shutdown{false};
    std::atomic<bool> m_initialized{false};
    
    // 后台清理线程
    std::thread m_cleanup_thread;
    std::thread m_heartbeat_thread;
    
    // 内部方法
    std::string MakeEndpoint(const std::string& host, uint16_t port);
    EndpointPool* GetOrCreatePool(const std::string& endpoint);
    std::shared_ptr<ZrpcConnection> CreateConnection(const std::string& host, uint16_t port);
    void CleanupIdleConnections();
    void HeartbeatCheck();
    void RunCleanupLoop();
    void RunHeartbeatLoop();
    bool IsConnectionValid(std::shared_ptr<ZrpcConnection> conn);
};

// RAII连接管理器
class ZrpcConnectionGuard {
public:
    ZrpcConnectionGuard(const std::string& host, uint16_t port);
    ~ZrpcConnectionGuard();
    
    std::shared_ptr<ZrpcConnection> GetConnection();
    bool IsValid() const;
    
private:
    std::shared_ptr<ZrpcConnection> m_connection;
    bool m_valid;
};

#endif // _Zrpc_CONNECTION_POOL_H_
