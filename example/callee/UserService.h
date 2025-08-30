#ifndef _USER_SERVICE_H_
#define _USER_SERVICE_H_

#include "../user.pb.h"
#include "Zrpcchannel.h"
#include <string>
#include <memory>

// 用户服务实现 - 集成自动缓存功能
class UserService : public Kuser::UserServiceRpc {
private:
    // 内置CacheService客户端
    std::unique_ptr<ZrpcChannel> cache_channel_;
    std::unique_ptr<Kuser::CacheServiceRpc_Stub> cache_stub_;
    bool cache_enabled_;
    
public:
    UserService();
    ~UserService();
    
    // 本地业务方法
    bool Login(std::string name, std::string pwd);
    bool Register(uint32_t id, std::string name, std::string pwd);
    int SumtoN(int n);
    std::string GetUserProfile(uint32_t user_id);
    bool CreateLoginSession(const std::string& username, const std::string& token);

    // RPC 接口实现（带自动缓存）
    void Login(::google::protobuf::RpcController* controller,
              const ::Kuser::LoginRequest* request,
              ::Kuser::LoginResponse* response,
              ::google::protobuf::Closure* done) override;
              
    void Register(::google::protobuf::RpcController* controller,
                  const ::Kuser::RegisterRequest* request,
                  ::Kuser::RegisterResponse* response,
                  ::google::protobuf::Closure* done) override;
                  
    void SumtoN(::google::protobuf::RpcController* controller,
                const ::Kuser::SumToNRequest* request,
                ::Kuser::SumToNResponse* response,
                ::google::protobuf::Closure* done) override;
                
    void GetUserProfile(::google::protobuf::RpcController* controller,
                        const ::Kuser::GetUserProfileRequest* request,
                        ::Kuser::GetUserProfileResponse* response,
                        ::google::protobuf::Closure* done) override;

private:
    // 缓存辅助方法
    bool CheckUserSession(const std::string& username);
    void CacheUserSession(const std::string& username, const std::string& token, int expire_seconds);
    std::string GetUserProfileFromCache(uint32_t user_id);
    void CacheUserProfile(uint32_t user_id, const std::string& profile_data, int expire_seconds);
    void InvalidateUserCache(uint32_t user_id);
    
    // 缓存键生成
    std::string MakeSessionKey(const std::string& username);
    std::string MakeProfileKey(uint32_t user_id);
    
    // 初始化内部缓存客户端
    void InitializeCacheClient();
};

#endif // _USER_SERVICE_H_
