#include "CacheService.h"
#include "ZrpcLogger.h"
#include <iostream>
#include <shared_mutex>

CacheService::CacheService() {
    LOG(INFO) << "CacheService initialized";
    
    // 启动清理线程
    cleanup_thread_ = std::thread(&CacheService::CleanupExpiredKeys, this);
}

CacheService::~CacheService() {
    should_stop_cleanup_ = true;
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }
    LOG(INFO) << "CacheService destroyed";
}

void CacheService::Set(::google::protobuf::RpcController* controller,
                       const ::Kuser::CacheSetRequest* request,
                       ::Kuser::ResultCode* response,
                       ::google::protobuf::Closure* done) {
    
    std::string key = request->key();
    std::string value = request->value();
    int expire_seconds = request->expire_seconds();
    
    try {
        {
            std::unique_lock<std::shared_mutex> lock(cache_mutex_);
            cache_store_[key] = CacheEntry(value, expire_seconds);
        }
        
        response->set_errcode(0);
        response->set_errmsg("Success");
        
        total_operations_++;
        LOG(INFO) << "Cache SET: key=" << key << ", expire=" << expire_seconds << "s";
        
    } catch (const std::exception& e) {
        response->set_errcode(-1);
        response->set_errmsg("Cache set failed: " + std::string(e.what()));
        LOG(ERROR) << "Cache SET failed: " << e.what();
    }
    
    done->Run();
}

void CacheService::Get(::google::protobuf::RpcController* controller,
                       const ::Kuser::CacheGetRequest* request,
                       ::Kuser::CacheGetResponse* response,
                       ::google::protobuf::Closure* done) {
    
    std::string key = request->key();
    
    try {
        {
            std::shared_lock<std::shared_mutex> lock(cache_mutex_);
            auto it = cache_store_.find(key);
            
            if (it != cache_store_.end()) {
                if (!it->second.IsExpired()) {
                    // 缓存命中且未过期
                    response->mutable_result()->set_errcode(0);
                    response->mutable_result()->set_errmsg("Success");
                    response->set_value(it->second.value);
                    response->set_exists(true);
                    
                    if (!it->second.never_expire) {
                        auto expire_time = std::chrono::duration_cast<std::chrono::seconds>(
                            it->second.expire_time.time_since_epoch()).count();
                        response->set_expire_time(expire_time);
                    }
                    
                    hit_count_++;
                    LOG(INFO) << "Cache HIT: key=" << key;
                } else {
                    // 键已过期
                    response->mutable_result()->set_errcode(1);
                    response->mutable_result()->set_errmsg("Key expired");
                    response->set_exists(false);
                    miss_count_++;
                    LOG(INFO) << "Cache EXPIRED: key=" << key;
                }
            } else {
                // 缓存未命中
                response->mutable_result()->set_errcode(1);
                response->mutable_result()->set_errmsg("Key not found");
                response->set_exists(false);
                miss_count_++;
                LOG(INFO) << "Cache MISS: key=" << key;
            }
        }
        
        total_operations_++;
        
    } catch (const std::exception& e) {
        response->mutable_result()->set_errcode(-1);
        response->mutable_result()->set_errmsg("Cache get failed: " + std::string(e.what()));
        LOG(ERROR) << "Cache GET failed: " << e.what();
    }
    
    done->Run();
}

void CacheService::Delete(::google::protobuf::RpcController* controller,
                          const ::Kuser::CacheDeleteRequest* request,
                          ::Kuser::ResultCode* response,
                          ::google::protobuf::Closure* done) {
    
    std::string key = request->key();
    
    try {
        {
            std::unique_lock<std::shared_mutex> lock(cache_mutex_);
            auto it = cache_store_.find(key);
            
            if (it != cache_store_.end()) {
                cache_store_.erase(it);
                response->set_errcode(0);
                response->set_errmsg("Success");
                LOG(INFO) << "Cache DELETE: key=" << key;
            } else {
                response->set_errcode(1);
                response->set_errmsg("Key not found");
                LOG(INFO) << "Cache DELETE failed: key not found: " << key;
            }
        }
        
        total_operations_++;
        
    } catch (const std::exception& e) {
        response->set_errcode(-1);
        response->set_errmsg("Cache delete failed: " + std::string(e.what()));
        LOG(ERROR) << "Cache DELETE failed: " << e.what();
    }
    
    done->Run();
}

void CacheService::Exists(::google::protobuf::RpcController* controller,
                          const ::Kuser::CacheExistsRequest* request,
                          ::Kuser::CacheExistsResponse* response,
                          ::google::protobuf::Closure* done) {
    
    std::string key = request->key();
    
    try {
        {
            std::shared_lock<std::shared_mutex> lock(cache_mutex_);
            auto it = cache_store_.find(key);
            
            bool exists = (it != cache_store_.end()) && !it->second.IsExpired();
            
            response->mutable_result()->set_errcode(0);
            response->mutable_result()->set_errmsg("Success");
            response->set_exists(exists);
        }
        
        total_operations_++;
        LOG(INFO) << "Cache EXISTS: key=" << key << ", exists=" << response->exists();
        
    } catch (const std::exception& e) {
        response->mutable_result()->set_errcode(-1);
        response->mutable_result()->set_errmsg("Cache exists check failed: " + std::string(e.what()));
        LOG(ERROR) << "Cache EXISTS failed: " << e.what();
    }
    
    done->Run();
}

void CacheService::BatchGet(::google::protobuf::RpcController* controller,
                            const ::Kuser::CacheBatchGetRequest* request,
                            ::Kuser::CacheBatchGetResponse* response,
                            ::google::protobuf::Closure* done) {
    
    try {
        {
            std::shared_lock<std::shared_mutex> lock(cache_mutex_);
            
            for (const auto& key : request->keys()) {
                auto* item = response->add_items();
                item->set_key(key);
                
                auto it = cache_store_.find(key);
                if (it != cache_store_.end() && !it->second.IsExpired()) {
                    item->set_value(it->second.value);
                    item->set_exists(true);
                    hit_count_++;
                } else {
                    item->set_exists(false);
                    miss_count_++;
                }
            }
        }
        
        response->mutable_result()->set_errcode(0);
        response->mutable_result()->set_errmsg("Success");
        total_operations_++;
        
        LOG(INFO) << "Cache BATCH_GET: " << request->keys_size() << " keys";
        
    } catch (const std::exception& e) {
        response->mutable_result()->set_errcode(-1);
        response->mutable_result()->set_errmsg("Batch get failed: " + std::string(e.what()));
        LOG(ERROR) << "Cache BATCH_GET failed: " << e.what();
    }
    
    done->Run();
}

void CacheService::GetStats(::google::protobuf::RpcController* controller,
                            const ::Kuser::CacheStatsRequest* request,
                            ::Kuser::CacheStatsResponse* response,
                            ::google::protobuf::Closure* done) {
    
    try {
        {
            std::shared_lock<std::shared_mutex> lock(cache_mutex_);
            
            response->mutable_result()->set_errcode(0);
            response->mutable_result()->set_errmsg("Success");
            response->set_total_keys(cache_store_.size());
            response->set_memory_usage(CalculateMemoryUsage());
            response->set_hit_count(hit_count_.load());
            response->set_miss_count(miss_count_.load());
            
            int64_t total_requests = hit_count_.load() + miss_count_.load();
            double hit_rate = total_requests > 0 ? 
                static_cast<double>(hit_count_.load()) / total_requests : 0.0;
            response->set_hit_rate(hit_rate);
        }
        
        LOG(INFO) << "Cache STATS: keys=" << response->total_keys() 
                  << ", hit_rate=" << response->hit_rate();
        
    } catch (const std::exception& e) {
        response->mutable_result()->set_errcode(-1);
        response->mutable_result()->set_errmsg("Get stats failed: " + std::string(e.what()));
        LOG(ERROR) << "Cache GET_STATS failed: " << e.what();
    }//计算命中率
    
    done->Run();
}

void CacheService::CleanupExpiredKeys() {
    while (!should_stop_cleanup_) {
        try {
            std::this_thread::sleep_for(std::chrono::milliseconds(CLEANUP_INTERVAL_MS));
            
            {
                std::unique_lock<std::shared_mutex> lock(cache_mutex_);
                
                auto it = cache_store_.begin();
                int cleaned_count = 0;
                
                while (it != cache_store_.end()) {
                    if (it->second.IsExpired()) {
                        it = cache_store_.erase(it);
                        cleaned_count++;
                    } else {
                        ++it;
                    }
                }
                
                if (cleaned_count > 0) {
                    LOG(INFO) << "Cleaned up " << cleaned_count << " expired cache keys";
                }
            }
            
        } catch (const std::exception& e) {
            LOG(ERROR) << "Cache cleanup error: " << e.what();
        }
    }
}

int64_t CacheService::CalculateMemoryUsage() const {
    int64_t total_size = 0;
    
    for (const auto& pair : cache_store_) {
        // 估算内存使用：key长度 + value长度 + 结构体开销
        total_size += pair.first.size() + pair.second.value.size() + sizeof(CacheEntry);
    }
    
    return total_size;
}
