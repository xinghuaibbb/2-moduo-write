cmake_minimum_required(VERSION 3.2)

# 设置项目名称
project(ModuoWrite)

# 设置调试信息
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 ")  

set(LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)


# 添加源文件
aux_source_directory(. SRC_LIST)

add_library(ModuoWrite SHARED ${SRC_LIST})

