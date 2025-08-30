#include <iostream>
#include <string>
#include "../user.pb.h"
#include "Zrpcapplication.h"
#include "Zrpcprovider.h"
#include "CacheService.h"
#include "UserService.h"  // 使用新的UserService头文件

int main(int argc, char **argv) {
    // 调用框架的初始化操作，解析命令行参数并加载配置文件
    ZrpcApplication::Init(argc, argv);

    // 创建一个 RPC 服务提供者对象
    ZrpcProvider provider;

    // 先注册CacheService，确保UserService可以连接到它
    std::cout << "Registering CacheService..." << std::endl;
    provider.NotifyService(new CacheService());
    
    // 稍微延迟，确保CacheService完全初始化
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 注册集成了自动缓存功能的UserService
    std::cout << "Registering UserService with integrated cache..." << std::endl;
    provider.NotifyService(new UserService());

    std::cout << "RPC服务启动成功，提供以下服务:" << std::endl;
    std::cout << "- UserService: Login, Register, SumtoN, GetUserProfile (带自动缓存)" << std::endl;
    std::cout << "- CacheService: Set, Get, Delete, Exists, BatchGet, GetStats" << std::endl;
    std::cout << "- 缓存集成: UserService现在会自动使用CacheService进行数据缓存" << std::endl;

    // 启动 RPC 服务节点，进入阻塞状态，等待远程的 RPC 调用请求
    provider.Run();

    return 0;
}