# 除了可以直接使用CMAKE_CURRENT_SOURCE_DIR的相对路径，也可以使用get_filename_component来获取当前目录的绝对路径
#这样就可以保证CMake 的某些操作可能需要使用绝对路径，尤其是跨目录的构建场景。绝对路径可以避免相对路径带来的混淆和潜在问题。
#动态调整目录结构：如果项目目录层次较深，或者使用子模块或外部依赖，绝对路径可以方便地定位正确的文件，而不用担心路径拼接或依赖 CMake 的当前工作目录。

#示例：
#get_filename_component(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR} ABSOLUTE)
#file(GLOB_RECURSE SRC_FILES ${SRC_DIR}/*.cc)
#获取当前目录下的所有源文件
file(GLOB SRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)

# 检查是否有protobuf生成的文件
file(GLOB PROTO_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.pb.cc)
if(PROTO_SRCS)
    message(STATUS "Found protobuf generated files: ${PROTO_SRCS}")
else()
    message(WARNING "No protobuf generated files found in src directory")
endif()

# 创建静态库
add_library(zrpc_core STATIC ${SRC_FILES} ${PROTO_SRCS})

# 设置库的属性
set_target_properties(zrpc_core PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
)

# 链接protobuf库
target_link_libraries(zrpc_core PUBLIC protobuf)

# 链接glog库
if(GLOG_FOUND)
    target_link_libraries(zrpc_core PUBLIC ${GLOG_LIBRARIES})
    target_include_directories(zrpc_core PUBLIC ${GLOG_INCLUDE_DIRS})
else()
    target_link_libraries(zrpc_core PUBLIC glog)
endif()

# 链接其他必要的库
target_link_libraries(zrpc_core PUBLIC 
    pthread
    zookeeper_mt
    muduo_net
    muduo_base
)

# 设置头文件的路径
target_include_directories(zrpc_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${Protobuf_INCLUDE_DIRS}
)

# 添加编译选项
target_compile_options(zrpc_core PRIVATE -std=c++11 -Wall -Wextra)

# 导出库符号（如果需要创建共享库）
# set_target_properties(zrpc_core PROPERTIES 
#     CXX_VISIBILITY_PRESET hidden
#     VISIBILITY_INLINES_HIDDEN ON
# )

message(STATUS "zrpc_core library configured successfully")