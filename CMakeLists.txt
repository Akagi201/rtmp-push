#
# @file CMakeLists.txt
# @author Akagi201
# @date 2014/10/10
#

cmake_minimum_required(VERSION 2.8.4)
project(rtmp_push)

set(SOURCE_FILES src/main.cpp src/CRtmpStream.cpp)

include_directories("/usr/local/include" "${PROJECT_SOURCE_DIR}/deps")

link_directories("/usr/local/lib" "${PROJECT_SOURCE_DIR}/deps/librtmp")

add_executable(rtmp_push ${SOURCE_FILES})

target_link_libraries(rtmp_push "librtmp.a")
