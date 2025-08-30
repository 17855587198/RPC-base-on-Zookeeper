#include "Zrpcapplication.h"
#include "../user.pb.h"
#include "Zrpccontroller.h"
#include "ZrpcLogger.h"
#include <iostream>
#include <memory>

// 综合测试：用户服务 + 缓存服务集成应用
class IntegratedClient {
private:
    std::unique_ptr<ZrpcChannel> user_channel_;
    std::unique_ptr<ZrpcChannel> cache_channel_;
    std::unique_ptr<Kuser::UserServiceRpc_Stub> user_stub_;
    std::unique_ptr<Kuser::CacheServiceRpc_Stub> cache_stub_;
    
public:
    IntegratedClient() {
        // 初始化用户服务连接
        user_channel_ = std::make_unique<ZrpcChannel>(false);
        user_channel_->EnableHeartbeat(true);
        user_stub_ = std::make_unique<Kuser::UserServiceRpc_Stub>(user_channel_.get());
        
        // 初始化缓存服务连接
        cache_channel_ = std::make_unique<ZrpcChannel>(false);
        cache_channel_->EnableHeartbeat(true);
        cache_stub_ = std::make_unique<Kuser::CacheServiceRpc_Stub>(cache_channel_.get());
    }
    
    // 带缓存的用户登录
    bool LoginWithCache(const std::string& username, const std::string& password) {
        LOG(INFO) << "=== 用户登录（带缓存） ===";
        
        // 1. 先检查缓存中是否有用户会话
        std::string session_key = "session:" + username;
        if (CheckUserSession(session_key)) {
            LOG(INFO) << "用户 " << username << " 已有有效会话，直接登录成功";
            return true;
        }
        
        // 2. 调用用户服务进行登录验证
        Kuser::LoginRequest request;
        request.set_name(username);
        request.set_pwd(password);
        
        Kuser::LoginResponse response;
        Zrpccontroller controller;
        controller.SetTimeout(10000);
        
        user_stub_->Login(&controller, &request, &response, nullptr);
        
        if (controller.Failed()) {
            LOG(ERROR) << "登录RPC调用失败: " << controller.ErrorText();
            return false;
        }
        
        if (response.result().errcode() == 0 && response.success()) {
            // 3. 登录成功，创建用户会话缓存
            std::string session_token = "token_" + username + "_" + std::to_string(time(nullptr));
            CacheUserSession(session_key, session_token, 1800); // 30分钟有效期
            
            LOG(INFO) << "用户 " << username << " 登录成功，会话已缓存";
            return true;
        } else {
            LOG(ERROR) << "登录失败: " << response.result().errmsg();
            return false;
        }
    }
    
    // 带缓存的用户信息查询
    std::string GetUserProfileWithCache(uint32_t user_id) {
        LOG(INFO) << "=== 获取用户信息（带缓存） ===";
        
        std::string cache_key = "profile:" + std::to_string(user_id);
        
        // 1. 先从缓存中查询
        std::string cached_profile = GetFromCache(cache_key);
        if (!cached_profile.empty()) {
            LOG(INFO) << "从缓存中获取用户 " << user_id << " 的信息: " << cached_profile;
            return cached_profile;
        }
        
        // 2. 缓存未命中，调用用户服务查询
        Kuser::GetUserProfileRequest request;
        request.set_user_id(user_id);
        
        Kuser::GetUserProfileResponse response;
        Zrpccontroller controller;
        controller.SetTimeout(5000);
        
        user_stub_->GetUserProfile(&controller, &request, &response, nullptr);
        
        if (controller.Failed()) {
            LOG(ERROR) << "获取用户资料RPC调用失败: " << controller.ErrorText();
            return "";
        }
        
        if (response.result().errcode() == 0) {
            std::string profile = response.profile_data();
            
            // 3. 将查询结果缓存起来
            SetToCache(cache_key, profile, 600); // 10分钟缓存
            
            LOG(INFO) << "从用户服务查询用户 " << user_id << " 信息并已缓存: " << profile;
            return profile;
        } else {
            LOG(ERROR) << "获取用户资料失败: " << response.result().errmsg();
            return "";
        }
    }
    
    // 用户注册（清理相关缓存）
    bool RegisterWithCacheInvalidation(uint32_t user_id, const std::string& username, const std::string& password) {
        LOG(INFO) << "=== 用户注册（缓存失效） ===";
        
        // 1. 调用注册服务
        Kuser::RegisterRequest request;
        request.set_id(user_id);
        request.set_name(username);
        request.set_pwd(password);
        
        Kuser::RegisterResponse response;
        Zrpccontroller controller;
        controller.SetTimeout(10000);
        
        user_stub_->Register(&controller, &request, &response, nullptr);
        
        if (controller.Failed()) {
            LOG(ERROR) << "注册RPC调用失败: " << controller.ErrorText();
            return false;
        }
        
        if (response.result().errcode() == 0 && response.success()) {
            // 2. 注册成功，清理可能存在的缓存
            std::string profile_key = "profile:" + std::to_string(user_id);
            DeleteFromCache(profile_key);
            
            LOG(INFO) << "用户 " << username << " 注册成功，相关缓存已清理";
            return true;
        } else {
            LOG(ERROR) << "注册失败: " << response.result().errmsg();
            return false;
        }
    }
    
    // 获取缓存统计信息
    void ShowCacheStats() {
        LOG(INFO) << "=== 缓存统计信息 ===";
        
        Kuser::CacheStatsRequest request;
        Kuser::CacheStatsResponse response;
        Zrpccontroller controller;
        controller.SetTimeout(5000);
        
        cache_stub_->GetStats(&controller, &request, &response, nullptr);
        
        if (controller.Failed()) {
            LOG(ERROR) << "获取缓存统计失败: " << controller.ErrorText();
            return;
        }
        
        LOG(INFO) << "总键数: " << response.total_keys();
        LOG(INFO) << "内存使用: " << response.memory_usage() << " bytes";
        LOG(INFO) << "命中次数: " << response.hit_count();
        LOG(INFO) << "未命中次数: " << response.miss_count();
        LOG(INFO) << "命中率: " << (response.hit_rate() * 100) << "%";
    }
    
private:
    // 检查用户会话
    bool CheckUserSession(const std::string& session_key) {
        Kuser::CacheExistsRequest request;
        request.set_key(session_key);
        
        Kuser::CacheExistsResponse response;
        Zrpccontroller controller;
        controller.SetTimeout(3000);
        
        cache_stub_->Exists(&controller, &request, &response, nullptr);
        
        return !controller.Failed() && response.exists();
    }
    
    // 缓存用户会话
    void CacheUserSession(const std::string& key, const std::string& token, int expire_seconds) {
        Kuser::CacheSetRequest request;
        request.set_key(key);
        request.set_value(token);
        request.set_expire_seconds(expire_seconds);
        
        Kuser::ResultCode response;
        Zrpccontroller controller;
        controller.SetTimeout(3000);
        
        cache_stub_->Set(&controller, &request, &response, nullptr);
    }
    
    // 从缓存获取数据
    std::string GetFromCache(const std::string& key) {
        Kuser::CacheGetRequest request;
        request.set_key(key);
        
        Kuser::CacheGetResponse response;
        Zrpccontroller controller;
        controller.SetTimeout(3000);
        
        cache_stub_->Get(&controller, &request, &response, nullptr);
        
        if (!controller.Failed() && response.exists()) {
            return response.value();
        }
        return "";
    }
    
    // 设置缓存
    void SetToCache(const std::string& key, const std::string& value, int expire_seconds) {
        Kuser::CacheSetRequest request;
        request.set_key(key);
        request.set_value(value);
        request.set_expire_seconds(expire_seconds);
        
        Kuser::ResultCode response;
        Zrpccontroller controller;
        controller.SetTimeout(3000);
        
        cache_stub_->Set(&controller, &request, &response, nullptr);
    }
    
    // 从缓存删除
    void DeleteFromCache(const std::string& key) {
        Kuser::CacheDeleteRequest request;
        request.set_key(key);
        
        Kuser::ResultCode response;
        Zrpccontroller controller;
        controller.SetTimeout(3000);
        
        cache_stub_->Delete(&controller, &request, &response, nullptr);
    }
};

int main(int argc, char** argv) {
    // 初始化RPC框架
    ZrpcApplication::Init(argc, argv);
    ZrpcLogger::GetInstance().Init("IntegratedClient");
    
    LOG(INFO) << "启动综合业务测试...";
    
    IntegratedClient client;
    
    // 测试场景1：用户登录（带缓存）
    client.LoginWithCache("zhangsan", "123456");
    client.LoginWithCache("zhangsan", "123456"); // 第二次应该命中缓存
    
    // 测试场景2：用户信息查询（带缓存）
    client.GetUserProfileWithCache(1001);
    client.GetUserProfileWithCache(1001); // 第二次应该命中缓存
    
    // 测试场景3：用户注册（缓存失效）
    client.RegisterWithCacheInvalidation(1002, "lisi", "654321");
    
    // 测试场景4：查看缓存统计
    client.ShowCacheStats();
    
    LOG(INFO) << "综合业务测试完成！";
    
    return 0;
}
