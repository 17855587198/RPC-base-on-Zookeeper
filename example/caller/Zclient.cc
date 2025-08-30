#include "Zrpcapplication.h"
#include "../user.pb.h"
#include "Zrpccontroller.h"
#include <iostream>
#include <atomic>
#include <thread>
#include <chrono>
#include "ZrpcLogger.h"

// 前置声明
void send_request(int thread_id, std::atomic<int> &success_count, std::atomic<int> &fail_count);
void test_cache_service(int thread_id, std::atomic<int> &success_count, std::atomic<int> &fail_count);
void test_integrated_business(int thread_id, std::atomic<int> &success_count, std::atomic<int> &fail_count);

// 发送 RPC 请求的函数，模拟客户端调用远程服务
void send_request(int thread_id, std::atomic<int> &success_count, std::atomic<int> &fail_count) {
    // 创建一个 UserServiceRpc_Stub 对象，用于调用远程的 RPC 方法
    ZrpcChannel* channel = new ZrpcChannel(false);
    
    // 启用心跳功能
    channel->EnableHeartbeat(true);
    
    Kuser::UserServiceRpc_Stub stub(channel);

    // 设置 RPC 方法的请求参数
    Kuser::LoginRequest request;
    request.set_name("zhangsan");  // 设置用户名
    request.set_pwd("123456");    // 设置密码

    // 定义 RPC 方法的响应参数
    Kuser::LoginResponse response;
    Zrpccontroller controller;  // 创建控制器对象，用于处理 RPC 调用过程中的错误
    
    // 设置15秒超时
    controller.SetTimeout(15000);

    // 调用远程的 Login 方法
    stub.Login(&controller, &request, &response, nullptr);

    // 检查 RPC 调用是否成功
    if (controller.Failed()) {  // 如果调用失败
        std::cout << controller.ErrorText() << std::endl;  // 打印错误信息
        fail_count++;  // 失败计数加 1
    } else {  // 如果调用成功
        if (0 == response.result().errcode()) {  // 检查响应中的错误码
            std::cout << "rpc login response success:" << response.success() << std::endl;  // 打印成功信息
            success_count++;  // 成功计数加 1
        } else {  // 如果响应中有错误
            std::cout << "rpc login response error : " << response.result().errmsg() << std::endl;  // 打印错误信息
            fail_count++;  // 失败计数加 1
        }
    }

    //caller调用远程发布的rpc方法Register
    Kuser::RegisterRequest req;
    req.set_id(100000);
    req.set_name("Zrpc");
    req.set_pwd("1111111");
    Kuser::RegisterResponse rsp;
    stub.Register(nullptr,&req,&rsp,nullptr);
    //一次rpc调用完成,读取调用结果
     if (controller.Failed()) {  // 如果调用失败
        std::cout << controller.ErrorText() << std::endl;  // 打印错误信息
        fail_count++;  // 失败计数加 1
    } else {  // 如果调用成功
        if (0 == rsp.result().errcode()) {  // 检查响应中的错误码
            std::cout << "rpc register response success:" << rsp.success() << std::endl;  // 打印成功信息
            success_count++;  // 成功计数加 1
        } else {  // 如果响应中有错误
            std::cout << "rpc register response error : " << rsp.result().errmsg() << std::endl;  // 打印错误信息
            fail_count++;  // 失败计数加 1
        }
    }

    delete channel;
}

// 测试缓存服务的函数
void test_cache_service(int thread_id, std::atomic<int> &success_count, std::atomic<int> &fail_count) {
    LOG(INFO) << "Thread " << thread_id << " testing cache service...";
    
    // 创建缓存服务客户端
    ZrpcChannel* cache_channel = new ZrpcChannel(false);
    cache_channel->EnableHeartbeat(true);
    Kuser::CacheServiceRpc_Stub cache_stub(cache_channel);
    
    // 1. 测试设置缓存
    Kuser::CacheSetRequest set_request;
    set_request.set_key("user:thread_" + std::to_string(thread_id));
    set_request.set_value("{\"name\":\"TestUser" + std::to_string(thread_id) + "\",\"thread\":" + std::to_string(thread_id) + "}");  // 修正语法错误
    set_request.set_expire_seconds(300);  // 5分钟过期
    
    Kuser::ResultCode set_response;
    Zrpccontroller set_controller;
    set_controller.SetTimeout(5000);
    
    cache_stub.Set(&set_controller, &set_request, &set_response, nullptr);
    
    if (set_controller.Failed()) {
        LOG(ERROR) << "Thread " << thread_id << " cache set failed: " << set_controller.ErrorText();
        fail_count++;
    } else {
        LOG(INFO) << "Thread " << thread_id << " cache set success";
        success_count++;
    }
    
    // 2. 测试获取缓存
    Kuser::CacheGetRequest get_request;
    get_request.set_key("user:thread_" + std::to_string(thread_id));
    
    Kuser::CacheGetResponse get_response;
    Zrpccontroller get_controller;
    get_controller.SetTimeout(5000);
    
    cache_stub.Get(&get_controller, &get_request, &get_response, nullptr);
    
    if (get_controller.Failed()) {
        LOG(ERROR) << "Thread " << thread_id << " cache get failed: " << get_controller.ErrorText();
        fail_count++;
    } else {
        if (get_response.exists()) {
            LOG(INFO) << "Thread " << thread_id << " cache get success: " << get_response.value();
            success_count++;
        } else {
            LOG(WARNING) << "Thread " << thread_id << " cache key not found";
            fail_count++;
        }
    }
    
    // 3. 测试检查存在性
    Kuser::CacheExistsRequest exists_request;
    exists_request.set_key("user:thread_" + std::to_string(thread_id));
    
    Kuser::CacheExistsResponse exists_response;
    Zrpccontroller exists_controller;
    exists_controller.SetTimeout(5000);
    
    cache_stub.Exists(&exists_controller, &exists_request, &exists_response, nullptr);
    
    if (exists_controller.Failed()) {
        LOG(ERROR) << "Thread " << thread_id << " cache exists failed: " << exists_controller.ErrorText();
        fail_count++;
    } else {
        LOG(INFO) << "Thread " << thread_id << " cache exists: " << exists_response.exists();
        success_count++;
    }
    
    delete cache_channel;
}

// 新增：集成业务测试 - 用户登录 + 会话缓存
void test_integrated_business(int thread_id, std::atomic<int> &success_count, std::atomic<int> &fail_count) {
    LOG(INFO) << "Thread " << thread_id << " testing integrated business...";
    
    // 创建用户服务和缓存服务客户端
    ZrpcChannel* user_channel = new ZrpcChannel(false);
    user_channel->EnableHeartbeat(true);
    Kuser::UserServiceRpc_Stub user_stub(user_channel);
    
    ZrpcChannel* cache_channel = new ZrpcChannel(false);
    cache_channel->EnableHeartbeat(true);
    Kuser::CacheServiceRpc_Stub cache_stub(cache_channel);
    
    std::string username = "user_" + std::to_string(thread_id);
    std::string session_key = "session:" + username;
    
    // 1. 首先检查会话缓存
    Kuser::CacheExistsRequest exists_request;
    exists_request.set_key(session_key);
    
    Kuser::CacheExistsResponse exists_response;
    Zrpccontroller exists_controller;
    exists_controller.SetTimeout(3000);
    
    cache_stub.Exists(&exists_controller, &exists_request, &exists_response, nullptr);
    
    if (!exists_controller.Failed() && exists_response.exists()) {
        // 会话存在，直接使用缓存
        LOG(INFO) << "Thread " << thread_id << " found cached session for " << username;
        success_count++;
    } else {
        // 2. 会话不存在，进行用户登录
        Kuser::LoginRequest login_request;
        login_request.set_name(username);
        login_request.set_pwd("password123");
        
        Kuser::LoginResponse login_response;
        Zrpccontroller login_controller;
        login_controller.SetTimeout(5000);
        
        user_stub.Login(&login_controller, &login_request, &login_response, nullptr);
        
        if (!login_controller.Failed() && login_response.success()) {
            // 3. 登录成功，创建会话缓存
            std::string session_token = "token_" + username + "_" + std::to_string(time(nullptr));
            
            Kuser::CacheSetRequest set_request;
            set_request.set_key(session_key);
            set_request.set_value(session_token);
            set_request.set_expire_seconds(1800);  // 30分钟过期
            
            Kuser::ResultCode set_response;
            Zrpccontroller set_controller;
            set_controller.SetTimeout(3000);
            
            cache_stub.Set(&set_controller, &set_request, &set_response, nullptr);
            
            if (!set_controller.Failed()) {
                LOG(INFO) << "Thread " << thread_id << " login success and session cached for " << username;
                success_count += 2;  // 登录成功 + 缓存成功
            } else {
                LOG(ERROR) << "Thread " << thread_id << " failed to cache session: " << set_controller.ErrorText();
                success_count++;  // 只有登录成功
                fail_count++;
            }
        } else {
            LOG(ERROR) << "Thread " << thread_id << " login failed for " << username;
            fail_count++;
        }
    }
    
    // 4. 测试用户资料缓存场景 - 使用真实的用户服务
    uint32_t user_id = 1000 + thread_id;
    std::string profile_key = "profile:" + std::to_string(user_id);
    
    // 检查用户资料是否在缓存中
    Kuser::CacheGetRequest profile_get_request;
    profile_get_request.set_key(profile_key);
    
    Kuser::CacheGetResponse profile_get_response;
    Zrpccontroller profile_get_controller;
    profile_get_controller.SetTimeout(3000);
    
    cache_stub.Get(&profile_get_controller, &profile_get_request, &profile_get_response, nullptr);
    
    if (!profile_get_controller.Failed() && profile_get_response.exists()) {
        LOG(INFO) << "Thread " << thread_id << " found cached profile for user " << user_id;
        success_count++;
    } else {
        // 缓存中没有，调用用户服务查询
        Kuser::GetUserProfileRequest user_profile_request;
        user_profile_request.set_user_id(user_id);
        
        Kuser::GetUserProfileResponse user_profile_response;
        Zrpccontroller user_profile_controller;
        user_profile_controller.SetTimeout(5000);
        
        user_stub.GetUserProfile(&user_profile_controller, &user_profile_request, &user_profile_response, nullptr);
        
        if (!user_profile_controller.Failed() && user_profile_response.result().errcode() == 0) {
            // 查询成功，缓存用户资料
            std::string profile_data = user_profile_response.profile_data();
            
            Kuser::CacheSetRequest profile_set_request;
            profile_set_request.set_key(profile_key);
            profile_set_request.set_value(profile_data);
            profile_set_request.set_expire_seconds(600);  // 10分钟过期
            
            Kuser::ResultCode profile_set_response;
            Zrpccontroller profile_set_controller;
            profile_set_controller.SetTimeout(3000);
            
            cache_stub.Set(&profile_set_controller, &profile_set_request, &profile_set_response, nullptr);
            
            if (!profile_set_controller.Failed()) {
                LOG(INFO) << "Thread " << thread_id << " queried and cached profile for user " << user_id;
                success_count += 2;  // 查询成功 + 缓存成功
            } else {
                LOG(ERROR) << "Thread " << thread_id << " failed to cache profile: " << profile_set_controller.ErrorText();
                success_count++;  // 只有查询成功
                fail_count++;
            }
        } else {
            LOG(ERROR) << "Thread " << thread_id << " failed to get user profile: " << 
                (user_profile_controller.Failed() ? user_profile_controller.ErrorText() : user_profile_response.result().errmsg());
            fail_count++;
        }
    }
    
    delete user_channel;
    delete cache_channel;
}

int main(int argc, char **argv) {
    // 初始化 RPC 框架，解析命令行参数并加载配置文件
    ZrpcApplication::Init(argc, argv);

    // 初始化日志系统（线程安全）
    ZrpcLogger::GetInstance().Init("MyRPC");

    const int thread_count = 10;       // 并发线程数
    const int requests_per_thread = 20; // 每个线程发送的请求数（减少以便更好观察）

    std::vector<std::thread> threads;  // 存储线程对象的容器
    std::atomic<int> success_count(0); // 成功请求的计数器
    std::atomic<int> fail_count(0);    // 失败请求的计数器

    // 检查命令行参数，决定测试模式
    std::string test_mode = "integrated";  // 默认集成测试
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--cache") {
            test_mode = "cache";
        } else if (arg == "--user") {
            test_mode = "user";
        } else if (arg == "--integrated") {
            test_mode = "integrated";
        }
    }

    LOG(INFO) << "Running test mode: " << test_mode;

    auto start_time = std::chrono::high_resolution_clock::now();  // 记录测试开始时间

    // 启动多线程进行并发测试
    for (int i = 0; i < thread_count; i++) {
        if (test_mode == "cache") {
            // 纯缓存服务测试
            threads.emplace_back([i, &success_count, &fail_count, requests_per_thread]() {
                for (int j = 0; j < requests_per_thread; j++) {
                    test_cache_service(i, success_count, fail_count);
                }
            });
        } else if (test_mode == "user") {
            // 纯用户服务测试
            threads.emplace_back([i, &success_count, &fail_count, requests_per_thread]() {
                for (int j = 0; j < requests_per_thread; j++) {
                    send_request(i, success_count, fail_count);
                }
            });
        } else {
            // 集成业务测试（默认）
            threads.emplace_back([i, &success_count, &fail_count, requests_per_thread]() {
                for (int j = 0; j < requests_per_thread; j++) {
                    test_integrated_business(i, success_count, fail_count);
                }
            });
        }
    }

    // 等待所有线程执行完毕
    for (auto &t : threads) {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();  // 记录测试结束时间
    std::chrono::duration<double> elapsed = end_time - start_time;  // 计算测试耗时

    // 输出统计结果
    LOG(INFO) << "=== " << test_mode << " Test Results ===";
    LOG(INFO) << "Total requests: " << thread_count * requests_per_thread;  // 总请求数
    LOG(INFO) << "Success count: " << success_count;  // 成功请求数
    LOG(INFO) << "Fail count: " << fail_count;  // 失败请求数
    LOG(INFO) << "Elapsed time: " << elapsed.count() << " seconds";  // 测试耗时
    LOG(INFO) << "QPS: " << (thread_count * requests_per_thread) / elapsed.count();  // 计算 QPS（每秒请求数）
    
    if (test_mode == "integrated") {
        LOG(INFO) << "=== 集成测试说明 ===";
        LOG(INFO) << "1. 测试用户登录 + 会话缓存";
        LOG(INFO) << "2. 测试用户资料查询 + 缓存";
        LOG(INFO) << "3. 演示缓存命中和缓存失效场景";
        LOG(INFO) << "4. 验证分布式服务协同工作";
    }

    return 0;
}