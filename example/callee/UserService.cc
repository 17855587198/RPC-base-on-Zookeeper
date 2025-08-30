#include "UserService.h"
#include "ZrpcLogger.h"
#include "Zrpccontroller.h"
#include <iostream>
#include <chrono>
#include <thread>

UserService::UserService() : cache_enabled_(false) {
    LOG(INFO) << "UserService initializing with integrated cache...";
    InitializeCacheClient();
}

UserService::~UserService() {
    LOG(INFO) << "UserService destroyed";
}

void UserService::InitializeCacheClient() {
    try {
        // 创建缓存服务的内部客户端连接
        cache_channel_ = std::make_unique<ZrpcChannel>(false);
        cache_stub_ = std::make_unique<Kuser::CacheServiceRpc_Stub>(cache_channel_.get());
        cache_enabled_ = true;
        
        LOG(INFO) << "UserService cache client initialized successfully";
    } catch (const std::exception& e) {
        LOG(ERROR) << "Failed to initialize cache client: " << e.what();
        cache_enabled_ = false;
    }
}

// 本地业务方法保持不变
bool UserService::Login(std::string name, std::string pwd) {
    std::cout << "doing local service: Login" << std::endl;
    std::cout << "name:" << name << " pwd:" << pwd << std::endl;  
    return true;
}

bool UserService::Register(uint32_t id, std::string name, std::string pwd) {
    std::cout << "doing local service: Register" << std::endl;
    std::cout << "id:" << id << " name:" << name << " pwd:" << pwd << std::endl;  
    return true;
}

int UserService::SumtoN(int n) {
    std::cout << "doing local service: SumtoN" << std::endl;
    std::cout << "n: " << n << std::endl;
    
    int sum = 0;
    for (int i = 1; i <= n; i++) {
        sum += i;
    }
    return sum;
}

std::string UserService::GetUserProfile(uint32_t user_id) {
    std::cout << "doing local service: GetUserProfile" << std::endl;
    std::cout << "user_id: " << user_id << std::endl;
    
    // 模拟数据库查询延迟
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    return "{\"id\":" + std::to_string(user_id) + 
           ",\"name\":\"User" + std::to_string(user_id) + 
           "\",\"age\":25,\"email\":\"user" + std::to_string(user_id) + "@example.com\"}";
}

bool UserService::CreateLoginSession(const std::string& username, const std::string& token) {
    std::cout << "creating login session for: " << username << std::endl;
    return true;
}

// ============ 带自动缓存的RPC接口实现 ============

void UserService::Login(::google::protobuf::RpcController* controller,
                        const ::Kuser::LoginRequest* request,
                        ::Kuser::LoginResponse* response,
                        ::google::protobuf::Closure* done) {
    std::string name = request->name();
    std::string pwd = request->pwd();
    
    LOG(INFO) << "Login request for user: " << name;
    
    // 1. 首先检查用户会话缓存
    if (cache_enabled_ && CheckUserSession(name)) {
        LOG(INFO) << "Found valid session in cache for user: " << name;
        
        // 直接返回登录成功
        Kuser::ResultCode *code = response->mutable_result();
        code->set_errcode(0);
        code->set_errmsg("Login from cached session");
        response->set_success(true);
        
        done->Run();
        return;
    }
    
    // 2. 缓存未命中，执行实际登录验证
    bool login_result = Login(name, pwd);
    
    // 3. 登录成功后，创建会话缓存
    if (login_result && cache_enabled_) {
        std::string session_token = "session_" + name + "_" + std::to_string(time(nullptr));
        CacheUserSession(name, session_token, 1800); // 30分钟有效期
        
        LOG(INFO) << "Login successful, session cached for user: " << name;
    }
    
    // 设置响应
    Kuser::ResultCode *code = response->mutable_result();
    code->set_errcode(0);
    code->set_errmsg("");
    response->set_success(login_result);
    
    done->Run();
}

void UserService::Register(::google::protobuf::RpcController* controller,
                          const ::Kuser::RegisterRequest* request,
                          ::Kuser::RegisterResponse* response,
                          ::google::protobuf::Closure* done) {
    uint32_t id = request->id();
    std::string name = request->name();
    std::string pwd = request->pwd();
    
    LOG(INFO) << "Register request for user: " << name << " (ID: " << id << ")";
    
    // 执行注册逻辑
    bool register_result = Register(id, name, pwd);
    
    // 注册成功后，清理可能存在的旧缓存
    if (register_result && cache_enabled_) {
        InvalidateUserCache(id);
        LOG(INFO) << "Registration successful, old cache invalidated for user ID: " << id;
    }
    
    // 设置响应
    Kuser::ResultCode *code = response->mutable_result();
    code->set_errcode(0);
    code->set_errmsg("");
    response->set_success(register_result);
    
    done->Run();
}

void UserService::SumtoN(::google::protobuf::RpcController* controller,
                         const ::Kuser::SumToNRequest* request,
                         ::Kuser::SumToNResponse* response,
                         ::google::protobuf::Closure* done) {
    int n = request->n();
    
    // 检查计算结果缓存
    std::string cache_key = "sum_1_to_" + std::to_string(n);
    
    if (cache_enabled_) {
        Kuser::CacheGetRequest cache_request;
        cache_request.set_key(cache_key);
        
        Kuser::CacheGetResponse cache_response;
        Zrpccontroller cache_controller;
        cache_controller.SetTimeout(3000);
        
        cache_stub_->Get(&cache_controller, &cache_request, &cache_response, nullptr);
        
        if (!cache_controller.Failed() && cache_response.exists()) {
            // 缓存命中
            int cached_result = std::atoi(cache_response.value().c_str());
            
            Kuser::ResultCode *code = response->mutable_result();
            code->set_errcode(0);
            code->set_errmsg("Result from cache");
            response->set_sum(cached_result);
            
            LOG(INFO) << "SumtoN cache hit for n=" << n << ", result=" << cached_result;
            done->Run();
            return;
        }
    }
    
    // 缓存未命中，执行计算
    int sum_result = SumtoN(n);
    
    // 将计算结果缓存
    if (cache_enabled_) {
        Kuser::CacheSetRequest cache_set_request;
        cache_set_request.set_key(cache_key);
        cache_set_request.set_value(std::to_string(sum_result));
        cache_set_request.set_expire_seconds(3600); // 1小时缓存
        
        Kuser::ResultCode cache_set_response;
        Zrpccontroller cache_set_controller;
        cache_set_controller.SetTimeout(3000);
        
        cache_stub_->Set(&cache_set_controller, &cache_set_request, &cache_set_response, nullptr);
        
        if (!cache_set_controller.Failed()) {
            LOG(INFO) << "SumtoN result cached for n=" << n << ", result=" << sum_result;
        }
    }
    
    // 设置响应
    Kuser::ResultCode *code = response->mutable_result();
    code->set_errcode(0);
    code->set_errmsg("");
    response->set_sum(sum_result);
    
    done->Run();
}

void UserService::GetUserProfile(::google::protobuf::RpcController* controller,
                                 const ::Kuser::GetUserProfileRequest* request,
                                 ::Kuser::GetUserProfileResponse* response,
                                 ::google::protobuf::Closure* done) {
    uint32_t user_id = request->user_id();
    
    LOG(INFO) << "GetUserProfile request for user ID: " << user_id;
    
    // 1. 先尝试从缓存获取用户资料
    std::string cached_profile = GetUserProfileFromCache(user_id);
    if (!cached_profile.empty()) {
        // 缓存命中
        Kuser::ResultCode *code = response->mutable_result();
        code->set_errcode(0);
        code->set_errmsg("Profile from cache");
        response->set_profile_data(cached_profile);
        response->set_from_cache(true);
        
        LOG(INFO) << "GetUserProfile cache hit for user ID: " << user_id;
        done->Run();
        return;
    }
    
    // 2. 缓存未命中，查询数据库
    std::string profile_data = GetUserProfile(user_id);
    
    // 3. 将查询结果缓存
    if (cache_enabled_) {
        CacheUserProfile(user_id, profile_data, 600); // 10分钟缓存
        LOG(INFO) << "GetUserProfile result cached for user ID: " << user_id;
    }
    
    // 设置响应
    Kuser::ResultCode *code = response->mutable_result();
    code->set_errcode(0);
    code->set_errmsg("");
    response->set_profile_data(profile_data);
    response->set_from_cache(false);
    
    done->Run();
}

// ============ 缓存辅助方法实现 ============

bool UserService::CheckUserSession(const std::string& username) {
    if (!cache_enabled_) return false;
    
    std::string session_key = MakeSessionKey(username);
    
    Kuser::CacheExistsRequest request;
    request.set_key(session_key);
    
    Kuser::CacheExistsResponse response;
    Zrpccontroller controller;
    controller.SetTimeout(3000);
    
    cache_stub_->Exists(&controller, &request, &response, nullptr);
    
    return !controller.Failed() && response.exists();
}

void UserService::CacheUserSession(const std::string& username, const std::string& token, int expire_seconds) {
    if (!cache_enabled_) return;
    
    std::string session_key = MakeSessionKey(username);
    
    Kuser::CacheSetRequest request;
    request.set_key(session_key);
    request.set_value(token);
    request.set_expire_seconds(expire_seconds);
    
    Kuser::ResultCode response;
    Zrpccontroller controller;
    controller.SetTimeout(3000);
    
    cache_stub_->Set(&controller, &request, &response, nullptr);
    
    if (controller.Failed()) {
        LOG(ERROR) << "Failed to cache user session: " << controller.ErrorText();
    }
}

std::string UserService::GetUserProfileFromCache(uint32_t user_id) {
    if (!cache_enabled_) return "";
    
    std::string profile_key = MakeProfileKey(user_id);
    
    Kuser::CacheGetRequest request;
    request.set_key(profile_key);
    
    Kuser::CacheGetResponse response;
    Zrpccontroller controller;
    controller.SetTimeout(3000);
    
    cache_stub_->Get(&controller, &request, &response, nullptr);
    
    if (!controller.Failed() && response.exists()) {
        return response.value();
    }
    
    return "";
}

void UserService::CacheUserProfile(uint32_t user_id, const std::string& profile_data, int expire_seconds) {
    if (!cache_enabled_) return;
    
    std::string profile_key = MakeProfileKey(user_id);
    
    Kuser::CacheSetRequest request;
    request.set_key(profile_key);
    request.set_value(profile_data);
    request.set_expire_seconds(expire_seconds);
    
    Kuser::ResultCode response;
    Zrpccontroller controller;
    controller.SetTimeout(3000);
    
    cache_stub_->Set(&controller, &request, &response, nullptr);
    
    if (controller.Failed()) {
        LOG(ERROR) << "Failed to cache user profile: " << controller.ErrorText();
    }
}

void UserService::InvalidateUserCache(uint32_t user_id) {
    if (!cache_enabled_) return;
    
    std::string profile_key = MakeProfileKey(user_id);
    
    Kuser::CacheDeleteRequest request;
    request.set_key(profile_key);
    
    Kuser::ResultCode response;
    Zrpccontroller controller;
    controller.SetTimeout(3000);
    
    cache_stub_->Delete(&controller, &request, &response, nullptr);
    
    if (controller.Failed()) {
        LOG(WARNING) << "Failed to invalidate user cache: " << controller.ErrorText();
    }
}

std::string UserService::MakeSessionKey(const std::string& username) {
    return "session:" + username;
}

std::string UserService::MakeProfileKey(uint32_t user_id) {
    return "profile:" + std::to_string(user_id);
}
