#设置最低版本和项目名称
cmake_minimum_required(VERSION 3.0)
project(Zrpc)
set(CMAKE_BUILD_TYPE "Debug")

#设置全局的C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 创建输出目录
file(MAKE_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
file(MAKE_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)

# 设置项目可执行文件输出的路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
# 设置项目库文件输出的路径
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

#查找protobuf包
find_package(Protobuf REQUIRED)
if(NOT Protobuf_FOUND)
    message(FATAL_ERROR "Protobuf not found. Please install protobuf first.")
endif()
include_directories(${Protobuf_INCLUDE_DIRS})

# 查找ZooKeeper库
find_library(ZOOKEEPER_LIBRARY NAMES zookeeper_mt)
if(NOT ZOOKEEPER_LIBRARY)
    message(FATAL_ERROR "ZooKeeper library not found. Please install zookeeper development package.")
endif()

# 查找muduo库
find_library(MUDUO_NET_LIBRARY NAMES muduo_net)
find_library(MUDUO_BASE_LIBRARY NAMES muduo_base)
if(NOT MUDUO_NET_LIBRARY OR NOT MUDUO_BASE_LIBRARY)
    message(WARNING "Muduo libraries not found. Please ensure muduo is properly installed.")
endif()

# 查找glog库
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(GLOG libglog)
endif()
if(NOT GLOG_FOUND)
    find_library(GLOG_LIBRARY NAMES glog)
    if(GLOG_LIBRARY)
        set(GLOG_LIBRARIES ${GLOG_LIBRARY})
    else()
        message(FATAL_ERROR "glog library not found. Please install glog.")
    endif()
endif()

#设置全局链接库
set(LIBS
    protobuf
    pthread
    zookeeper_mt
    muduo_net
    muduo_base
    glog
)

# 添加编译选项
add_compile_options(-Wall -Wextra -std=c++11)

# 在Debug模式下添加调试信息
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0)
else()
    add_compile_options(-O2)
endif()

#添加子目录
add_subdirectory(src)
add_subdirectory(example)

# 打印配置信息
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Protobuf version: ${Protobuf_VERSION}")
message(STATUS "Output directory: ${PROJECT_SOURCE_DIR}/bin")
