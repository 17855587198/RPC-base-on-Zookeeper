#ifndef _CACHE_SERVICE_H_
#define _CACHE_SERVICE_H_

#include "../user.pb.h"
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <string>
#include <atomic>

// 缓存项结构
struct CacheEntry {
    std::string value;
    std::chrono::steady_clock::time_point expire_time;
    bool never_expire;
    
    CacheEntry() : never_expire(true) {}
    
    CacheEntry(const std::string& val, int expire_seconds) 
        : value(val), never_expire(expire_seconds == 0) {
        if (!never_expire) {
            expire_time = std::chrono::steady_clock::now() + 
                         std::chrono::seconds(expire_seconds);
        }
    }
    
    bool IsExpired() const {
        if (never_expire) return false;
        return std::chrono::steady_clock::now() > expire_time;
    }
};

// 分布式缓存服务实现
class CacheService : public Kuser::CacheServiceRpc {
private:
    std::unordered_map<std::string, CacheEntry> cache_store_;
    mutable std::shared_mutex cache_mutex_;  // 读写锁，支持多读单写
    
    // 统计信息
    std::atomic<int64_t> hit_count_{0};
    std::atomic<int64_t> miss_count_{0};
    std::atomic<int64_t> total_operations_{0};
    
    // 清理过期键的间隔（毫秒）
    static const int CLEANUP_INTERVAL_MS = 60000;  // 1分钟
    std::thread cleanup_thread_;
    std::atomic<bool> should_stop_cleanup_{false};
    
public:
    CacheService();
    ~CacheService();
    
    // 实现protobuf生成的虚函数
    void Set(::google::protobuf::RpcController* controller,
             const ::Kuser::CacheSetRequest* request,
             ::Kuser::ResultCode* response,
             ::google::protobuf::Closure* done) override;
             
    void Get(::google::protobuf::RpcController* controller,
             const ::Kuser::CacheGetRequest* request,
             ::Kuser::CacheGetResponse* response,
             ::google::protobuf::Closure* done) override;
             
    void Delete(::google::protobuf::RpcController* controller,
                const ::Kuser::CacheDeleteRequest* request,
                ::Kuser::ResultCode* response,
                ::google::protobuf::Closure* done) override;
                
    void Exists(::google::protobuf::RpcController* controller,
                const ::Kuser::CacheExistsRequest* request,
                ::Kuser::CacheExistsResponse* response,
                ::google::protobuf::Closure* done) override;
                
    void BatchGet(::google::protobuf::RpcController* controller,
                  const ::Kuser::CacheBatchGetRequest* request,
                  ::Kuser::CacheBatchGetResponse* response,
                  ::google::protobuf::Closure* done) override;
                  
    void GetStats(::google::protobuf::RpcController* controller,
                  const ::Kuser::CacheStatsRequest* request,
                  ::Kuser::CacheStatsResponse* response,
                  ::google::protobuf::Closure* done) override;

private:
    // 清理过期键的后台线程
    void CleanupExpiredKeys();
    
    // 计算内存使用量（估算）
    int64_t CalculateMemoryUsage() const;
    
    // 移除过期的键
    void RemoveExpiredKey(const std::string& key);
};

#endif // _CACHE_SERVICE_H_
