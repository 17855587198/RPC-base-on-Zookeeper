#ifndef _Zrpcapplication_H
#define _Zrpcapplication_H
#include "Zrpcconfig.h"
#include "Zrpcchannel.h" 
#include  "Zrpccontroller.h"
#include<mutex>
//Zrpc基础类，负责框架的一些初始化操作
class ZrpcApplication
{
    public:
    static void Init(int argc,char **argv);
    static ZrpcApplication & GetInstance();//单例类的普通成员函数通过实例对象调用，只有获取实例的方法是静态的
    static void deleteInstance();
    static Zrpcconfig& GetConfig();
    private:
    static Zrpcconfig m_config;
    static ZrpcApplication * m_application;//全局唯一单例访问对象
    static std::mutex m_mutex;
    ZrpcApplication(){}
    ~ZrpcApplication(){}
    ZrpcApplication(const ZrpcApplication&)=delete;
    ZrpcApplication(ZrpcApplication&&)=delete;
};
#endif 
