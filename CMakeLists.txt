cmake_minimum_required(VERSION 2.8)

#set (CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")

project(simple_http)
add_executable(${PROJECT_NAME}
    "main.cpp"
    "http/string.cpp"
    "http/request.cpp"
    "http/worker.cpp"
    "http/server.cpp"
    "api/api.cpp"
    "api/hls_context.h"
    "hls/chunk.h"
    "hls_live_storage/hls_live_storage.cpp")

target_link_libraries(${PROJECT_NAME} "pthread")
