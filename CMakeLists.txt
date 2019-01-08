cmake_minimum_required(VERSION 2.8)

set (CMAKE_CXX_STANDARD 17)

project(untitled0.3)
add_executable(${PROJECT_NAME}
    "main.cpp"
    "http/string.cpp"
    "http/request.cpp"
    "http/worker.cpp"
    "http/server.cpp"
    "application.cpp")

target_link_libraries(${PROJECT_NAME} "pthread")
