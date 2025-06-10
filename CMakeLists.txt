cmake_minimum_required(VERSION 3.5)

# 设置项目名称
project(hzhmuduo)

# 设置调试信息
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++14 -fPIC")  

# 区分 LIBRARY_OUTPUT_DIRECTORY 和 LIBRARY_OUTPUT_PATH
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)


# 添加源文件
aux_source_directory(. SRC_LIST)

add_library(hzhmuduo SHARED ${SRC_LIST})

