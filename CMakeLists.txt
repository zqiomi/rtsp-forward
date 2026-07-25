cmake_minimum_required(VERSION 3.10)
project(rtsp_server VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(src)
include_directories(include)

# Phase 1: util + buffer
# Phase 2: net
# Phase 3: protocol + rtp
# Phase 4: core
# Phase 5: api + demo
add_library(rtsp_server STATIC
    src/util/log.cc
    src/buffer/ring_buffer.cc
    src/net/epoll_loop.cc
    src/net/socket.cc
    src/net/connection.cc
    src/net/listener.cc
    src/protocol/rtsp_parser.cc
    src/protocol/rtsp_builder.cc
    src/rtp/rtp_forwarder.cc
    src/core/rtsp_session.cc
    src/core/rtsp_server.cc
    src/api/rtsp_server_api.cc
)

# 安装目标
install(TARGETS rtsp_server DESTINATION lib)
install(FILES include/rtsp_server.h DESTINATION include)

# Demo 程序
add_executable(rtsp_server_demo demo/main.cc)
target_link_libraries(rtsp_server_demo rtsp_server)
