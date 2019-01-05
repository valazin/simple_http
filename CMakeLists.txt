cmake_minimum_required(VERSION 2.8)

project(untitled0.3)
add_executable(${PROJECT_NAME} "main.cpp" "application.cpp" "http/server.cpp" "http/worker.cpp" "http/request.cpp")

target_link_libraries(${PROJECT_NAME} "pthread")
