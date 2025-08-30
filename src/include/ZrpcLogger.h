#ifndef Zrpc_LOG_H
#define Zrpc_LOG_H
#include<glog/logging.h>
#include<string>
#include<mutex>
#include<atomic>

//采用RAII的思想，使用单例模式保证线程安全
class ZrpcLogger
{
public:
    // 获取单例实例
    static ZrpcLogger& GetInstance() {
        static ZrpcLogger instance;
        return instance;
    }
    
    // 初始化日志系统（线程安全）
    void Init(const char* argv0) {
        std::call_once(GetInitFlag(), [this, argv0]() {
            google::InitGoogleLogging(argv0);
            FLAGS_colorlogtostderr = true;  // 启用彩色日志
            FLAGS_logtostderr = true;       // 默认输出标准错误
            
            // 设置日志文件输出（可选）
            // FLAGS_log_dir = "./logs";
            // FLAGS_max_log_size = 100;  // 100MB per log file
            
            initialized_.store(true, std::memory_order_release);
        });
    }
    
    // 检查是否已初始化（使用 memory_order 保证可见性）
    bool IsInitialized() const {
        return initialized_.load(std::memory_order_acquire);
    }
    
    // 析构函数，自动清理glog
    ~ZrpcLogger() {
        if (initialized_.load(std::memory_order_acquire)) {
            google::ShutdownGoogleLogging();
        }
    }
    
    // 提供静态日志方法（线程安全）
    static void Info(const std::string &message) {
        // 检查是否已初始化（可选，但更安全）
        if (!GetInstance().IsInitialized()) {
            // 可以选择自动初始化或者输出警告
            // 这里选择继续执行，因为Glog有默认行为
        }
        LOG(INFO) << message;
    }
    
    static void Warning(const std::string &message) {
        if (!GetInstance().IsInitialized()) {
            // 同样的处理
        }
        LOG(WARNING) << message;
    }
    
    static void ERROR(const std::string &message) {
        if (!GetInstance().IsInitialized()) {
            // 同样的处理
        }
        LOG(ERROR) << message;
    }
    
    static void Fatal(const std::string& message) {
        if (!GetInstance().IsInitialized()) {
            // 同样的处理
        }
        LOG(FATAL) << message;
    }

private:
    // 私有构造函数
    ZrpcLogger() : initialized_(false) {}
    
    // 禁用拷贝构造函数和重载赋值函数
    ZrpcLogger(const ZrpcLogger&) = delete;
    ZrpcLogger& operator=(const ZrpcLogger&) = delete;
    
    // 线程安全的初始化控制
    static std::once_flag& GetInitFlag() {
        static std::once_flag init_flag;
        return init_flag;
    }
    std::atomic<bool> initialized_;
};

#endif