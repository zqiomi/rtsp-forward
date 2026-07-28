cmake_minimum_required(VERSION 3.10)
project(rtsp_forward
    VERSION 1.2.0
    DESCRIPTION "轻量级RTSP转发服务器库"
    LANGUAGES CXX)

# ===== 编译选项 =====
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 默认调试模式：开启 -g 与 AddressSanitizer
set(CMAKE_CXX_FLAGS_DEBUG "-Wall -Wextra -Werror -fno-exceptions -g -fno-omit-frame-pointer")
# Release模式：优化编译，去除调试符号
set(CMAKE_CXX_FLAGS_RELEASE "-Wall -Wextra -Werror -fno-exceptions -O2 -DNDEBUG")
# Release with Debug Info：优化编译但保留调试符号
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-Wall -Wextra -Werror -fno-exceptions -O2 -g -DNDEBUG")

# AddressSanitizer（仅Debug模式）
# set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address")
# set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -fsanitize=address")

# ===== 构建选项 =====
option(BUILD_SHARED_LIBS "构建动态库" ON)
option(BUILD_STATIC_LIBS "构建静态库" ON)
option(BUILD_DEMO "构建示例程序" ON)

# 标准安装路径（支持 CMAKE_INSTALL_PREFIX 配置）
include(GNUInstallDirs)

# 头文件路径
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR})  # version.h 生成在此

# log_kit 依赖（优先查找已安装版本，支持子目录方式集成）
if(NOT TARGET log_kit_shared AND NOT TARGET log_kit_static)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../log-kit
                     ${CMAKE_CURRENT_BINARY_DIR}/log-kit)
endif()

# ===== 版本头文件（从 project(VERSION) 注入）=====
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/version.h"
    @ONLY
)

# ===== 源文件列表 =====
set(RTSP_FORWARD_SOURCES
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
    src/api/rtsp_forward.cc
)

# ===== 动态库 =====
if(BUILD_SHARED_LIBS)
    add_library(rtsp_forward_shared SHARED ${RTSP_FORWARD_SOURCES})
    set_target_properties(rtsp_forward_shared PROPERTIES
        OUTPUT_NAME rtsp_forward
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
    target_link_libraries(rtsp_forward_shared PUBLIC log_kit_shared)
    install(TARGETS rtsp_forward_shared
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()

# ===== 静态库 =====
if(BUILD_STATIC_LIBS)
    add_library(rtsp_forward_static STATIC ${RTSP_FORWARD_SOURCES})
    set_target_properties(rtsp_forward_static PROPERTIES
        OUTPUT_NAME rtsp_forward
        POSITION_INDEPENDENT_CODE ON
    )
    target_link_libraries(rtsp_forward_static PUBLIC log_kit_static)
    install(TARGETS rtsp_forward_static
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()

# ===== 安装头文件 =====
install(FILES include/rtsp_forward.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# ===== pkg-config 文件 =====
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/rtsp_forward.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/rtsp_forward.pc"
    @ONLY
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/rtsp_forward.pc"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# ===== Demo 程序 =====
if(BUILD_DEMO)
    find_package(Threads REQUIRED)
    add_executable(rtsp_forward_demo demo/main.cc)
    if(BUILD_SHARED_LIBS)
        target_link_libraries(rtsp_forward_demo rtsp_forward_shared Threads::Threads)
    else()
        target_link_libraries(rtsp_forward_demo rtsp_forward_static Threads::Threads)
    endif()
    install(TARGETS rtsp_forward_demo
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

# ===== CPack 打包配置 =====
set(CPACK_PACKAGE_NAME "rtsp_forward")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_CONTACT "zqiomi@gmail.com")
set(CPACK_PACKAGE_VENDOR "rtsp_forward")

set(CPACK_GENERATOR "TGZ")
set(CPACK_SOURCE_GENERATOR "TGZ")

include(CPack)

# ===== 安装摘要 =====
message(STATUS "========== rtsp_forward ${PROJECT_VERSION} ==========")
message(STATUS "动态库: ${BUILD_SHARED_LIBS}")
message(STATUS "静态库: ${BUILD_STATIC_LIBS}")
message(STATUS "示例程序: ${BUILD_DEMO}")
message(STATUS "安装路径: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "库目录: ${CMAKE_INSTALL_LIBDIR}")
message(STATUS "头文件目录: ${CMAKE_INSTALL_INCLUDEDIR}")
message(STATUS "==========================================")
