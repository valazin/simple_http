cmake_minimum_required(VERSION 2.8)

#set (CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -lstdc++fs")

project(simple_http)
add_executable(${PROJECT_NAME}
    "main.cpp"
    "settings/settings.cpp"
    "settings/http_server_settings.h"
    "settings/live_hls_settings.h"
    "settings/archive_hls_settings.h"
    "utility/filesystem.cpp"
    "utility/datetime.cpp"
    "http/string.cpp"
    "http/uri.cpp"
    "http/request.cpp"
    "http/worker.cpp"
    "http/server.cpp"
    "api/api.cpp"
    "api/hls_context.h"
    "hls/chunk.h"
    "hls_live_storage/hls_live_storage.cpp"
    "hls_live_storage/hls_live_storage_error.cpp"
    "hls_archive_storage/hls_arhive_storage.cpp"
    "hls_archive_storage/hls_chunk_info.h"
    "hls_archive_storage/chunk_info_serializer.cpp"
    "hls_archive_storage/hls_chunk_info_repository.cpp"
    "hls_archive_storage/hls_arhive_playlist_generator.cpp"
    )

target_include_directories(${PROJECT_NAME} PRIVATE
    "/usr/include"
    "/usr/include/mongocxx/v_noabi"
    "/usr/include/bsoncxx/v_noabi"
    )

target_link_libraries(${PROJECT_NAME}
    "pthread"
    "glog"
    "mongoc-1.0"
    "bson-1.0"
    "mongocxx"
    "bsoncxx"
    "config++"
    )
