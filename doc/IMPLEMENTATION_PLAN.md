# RTSP Server 实现计划

## 项目概述

本项目是一个轻量级 RTSP Server 库，采用 C++11 实现，对外暴露纯 C 接口，仅透传 RTP 包，不做音视频处理。

## 实现策略

采用**自下而上**的实现策略，按依赖关系分阶段实现，每阶段完成后确保编译通过。

## 阶段划分

### Phase 1: 基础工具层 (util + buffer)

**目标**：实现最底层的工具类和缓冲区，为上层提供基础能力。

**文件清单**：
| 文件 | 说明 | 状态 |
| :--- | :--- | :--- |
| `src/util/common.h` | 通用类型定义、宏 | 待实现 |
| `src/util/status.h` | Status 类定义 | 待实现 |
| `src/util/log.h` | 日志接口 | 待实现 |
| `src/util/log.cc` | 日志实现 | 待实现 |
| `src/util/resource_manager.h` | 资源追踪器接口 | 待实现 |
| `src/util/resource_manager.cc` | 资源追踪器实现 | 待实现 |
| `src/buffer/ring_buffer.h` | 环形缓冲区接口 | 待实现 |
| `src/buffer/ring_buffer.cc` | 环形缓冲区实现 | 待实现 |

**依赖关系**：无外部依赖

**编译验证**：
```bash
mkdir -p build && cd build
cmake ..
make
```

---

### Phase 2: 网络工具层 (net)

**目标**：实现事件循环抽象和网络工具类。

**文件清单**：
| 文件 | 说明 | 状态 |
| :--- | :--- | :--- |
| `src/net/event_loop.h` | EventLoop 抽象接口 | 待实现 |
| `src/net/epoll_loop.h` | EpollLoop 接口 | 待实现 |
| `src/net/epoll_loop.cc` | EpollLoop 实现 | 待实现 |
| `src/net/socket.h` | Socket 工具类接口 | 待实现 |
| `src/net/socket.cc` | Socket 工具类实现 | 待实现 |
| `src/net/connection.h` | Connection 工具类接口 | 待实现 |
| `src/net/connection.cc` | Connection 工具类实现 | 待实现 |
| `src/net/listener.h` | Listener 工具类接口 | 待实现 |
| `src/net/listener.cc` | Listener 工具类实现 | 待实现 |

**依赖关系**：util + buffer

**编译验证**：
```bash
cd build && cmake .. && make
```

---

### Phase 3: 协议层 + RTP层 (protocol + rtp)

**目标**：实现 RTSP 协议解析/构建和 RTP 透传功能。

**文件清单**：
| 文件 | 说明 | 状态 |
| :--- | :--- | :--- |
| `src/protocol/rtsp_parser.h` | RTSP 请求解析接口 | 待实现 |
| `src/protocol/rtsp_parser.cc` | RTSP 请求解析实现 | 待实现 |
| `src/protocol/rtsp_builder.h` | RTSP 响应构建接口 | 待实现 |
| `src/protocol/rtsp_builder.cc` | RTSP 响应构建实现 | 待实现 |
| `src/rtp/rtp_packet.h` | RTP 包结构定义 | 待实现 |
| `src/rtp/rtp_forwarder.h` | RTP 转发器接口 | 待实现 |
| `src/rtp/rtp_forwarder.cc` | RTP 转发器实现 | 待实现 |

**依赖关系**：util + buffer + net

**编译验证**：
```bash
cd build && cmake .. && make
```

---

### Phase 4: 核心层 (core)

**目标**：实现 RtspServer、Session 和服务器管理。

**文件清单**：
| 文件 | 说明 | 状态 |
| :--- | :--- | :--- |
| `src/core/session.h` | Session 接口 | 待实现 |
| `src/core/session.cc` | Session 实现 | 待实现 |
| `src/core/server.h` | RtspServer 接口 | 待实现 |
| `src/core/server.cc` | RtspServer 实现 | 待实现 |
| `src/core/server_manager.h` | RtspServerManager 接口 | 待实现 |
| `src/core/server_manager.cc` | RtspServerManager 实现 | 待实现 |

**依赖关系**：util + buffer + net + protocol + rtp

**编译验证**：
```bash
cd build && cmake .. && make
```

---

### Phase 5: API 层 + Demo

**目标**：实现 C API 接口和测试 Demo。

**文件清单**：
| 文件 | 说明 | 状态 |
| :--- | :--- | :--- |
| `include/rtsp_server.h` | 对外 C 接口头文件 | 待实现 |
| `src/api/rtsp_server_api.h` | C API 内部头文件 | 待实现 |
| `src/api/rtsp_server_api.cc` | C API 实现 | 待实现 |
| `demo/demo.c` | C 语言测试 Demo | 待实现 |

**依赖关系**：util + buffer + net + protocol + rtp + core

**编译验证**：
```bash
cd build && cmake .. && make
```

---

## 错误处理约定

### C++ 内部
- 所有可能失败的函数返回 `Status` 对象
- `Status` 包含错误码和可选的错误信息
- 使用 `LOG_ERROR` 记录错误日志

### C API
- 所有函数返回整数错误码（负数表示错误）
- 错误码定义在 `include/rtsp_server.h` 中

## CMake 配置策略

使用单个 `CMakeLists.txt` 文件，逐步添加源文件：
1. Phase 1: 添加 util + buffer 源文件
2. Phase 2: 添加 net 源文件
3. Phase 3: 添加 protocol + rtp 源文件
4. Phase 4: 添加 core 源文件
5. Phase 5: 添加 api 源文件和 demo

## 代码规范

- C++ 类：PascalCase
- C++ 函数/变量：snake_case
- 常量：kPrefix + PascalCase
- C API 函数：rtsp_server_ 前缀 + snake_case
- 禁用异常（-fno-exceptions）
- 使用 RAII 管理资源
- 所有头文件使用 include guard

## 编译选项

```cmake
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")
set(CMAKE_CXX_STANDARD 11)
```

## 平台要求

- Linux (epoll)
- C++11 兼容编译器
- CMake 3.10+

## 进度追踪

| 阶段 | 目标文件数 | 已完成 | 状态 |
| :--- | :--- | :--- | :--- |
| Phase 1 | 8 | 0 | 未开始 |
| Phase 2 | 8 | 0 | 未开始 |
| Phase 3 | 6 | 0 | 未开始 |
| Phase 4 | 6 | 0 | 未开始 |
| Phase 5 | 4 | 0 | 未开始 |
| **总计** | **32** | **0** | **未开始** |
