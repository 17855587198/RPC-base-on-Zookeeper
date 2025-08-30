#include "KrpcLogger.h"
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <atomic>

// 并发日志测试
void concurrent_log_test() {
    const int num_threads = 10;
    const int logs_per_thread = 1000;
    std::atomic<int> completed_threads(0);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 创建多个线程同时写日志
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([i, logs_per_thread, &completed_threads]() {
            for (int j = 0; j < logs_per_thread; ++j) {
                // 混合使用不同级别的日志
                switch (j % 4) {
                    case 0:
                        KrpcLogger::Info("Thread " + std::to_string(i) + " Info log " + std::to_string(j));
                        break;
                    case 1:
                        KrpcLogger::Warning("Thread " + std::to_string(i) + " Warning log " + std::to_string(j));
                        break;
                    case 2:
                        KrpcLogger::ERROR("Thread " + std::to_string(i) + " Error log " + std::to_string(j));
                        break;
                    case 3:
                        KrpcLogger::Info("Thread " + std::to_string(i) + " Info log " + std::to_string(j));
                        break;
                }
            }
            completed_threads++;
        });
    }
    
    // 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    int total_logs = num_threads * logs_per_thread;
    double logs_per_second = static_cast<double>(total_logs) / (duration.count() / 1000.0);
    
    std::cout << "并发日志测试结果：" << std::endl;
    std::cout << "线程数量: " << num_threads << std::endl;
    std::cout << "每线程日志数: " << logs_per_thread << std::endl;
    std::cout << "总日志数: " << total_logs << std::endl;
    std::cout << "执行时间: " << duration.count() << "ms" << std::endl;
    std::cout << "日志写入速率: " << logs_per_second << " logs/second" << std::endl;
}

int main() {
    // 初始化日志系统
    KrpcLogger::GetInstance().Init("ConcurrentLogTest");
    
    std::cout << "开始并发日志测试..." << std::endl;
    concurrent_log_test();
    
    return 0;
}
