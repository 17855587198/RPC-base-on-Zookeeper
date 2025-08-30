#获取服务端的源文件
file(GLOB Client_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)

#获取protobuf生成的.cc
file(GLOB PROTO_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../*.pb.cc)

#创建服务端可执行文件
add_executable(client ${Client_SRCS} ${PROTO_SRCS})

#链接必要的库
target_link_libraries(client zrpc_core ${LIBS})

# 设置编译选项
target_compile_options(client PRIVATE -std=c++11 -Wall)

# 设置 client 可执行文件输出目录
set_target_properties(client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

# 确保包含必要的头文件路径
target_include_directories(client PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/include)
