# RTSP Server 实现文档

## 1. 架构设计

### 1.1 整体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                      C API 层 (src/api/)                        │
│  rtsp_server_create() / destroy() / start() / stop() / ...     │
│  纯C接口，不透明指针管理                                         │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Server 核心层 (src/core/)                    │
│  RtspServer - 管理事件循环和Session生命周期                      │
│  RtspSession - RTSP会话管理                                     │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      网络工具层 (src/net/)                       │
│  EventLoop - 事件循环抽象接口                                    │
│  EpollLoop - epoll实现（Linux）                                  │
│  Socket - socket封装工具类                                       │
│  Connection - 连接封装工具类（带缓冲区）                          │
│  Listener - 监听socket封装工具类                                 │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    协议层 (src/protocol/)                        │
│  RtspParser - RTSP请求解析                                      │
│  RtspBuilder - RTSP响应构建                                     │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      RTP层 (src/rtp/)                           │
│  RtpPacket - RTP包结构（仅透传，不解析不打包）                    │
│  RtpForwarder - RTP包转发（直接透传到客户端）                      │
└─────────────────────────────────────────────────────────────────┘
```

### 1.2 分层职责

| 层级 | 目录 | 职责 |
| :--- | :--- | :--- |
| C API层 | `src/api/` | 纯C对外接口，不透明指针管理，错误码处理 |
| Server核心层 | `src/core/` | RtspServer核心逻辑，事件循环，Session管理 |
| 网络工具层 | `src/net/` | 事件循环抽象、Socket/Connection/Listener工具类 |
| 协议层 | `src/protocol/` | RTSP协议解析与构建 |
| RTP层 | `src/rtp/` | RTP包结构定义与透传（不解析不打包） |
| 缓冲区 | `src/buffer/` | 环形缓冲区实现（独立组件） |
| 工具 | `src/util/` | 日志、状态码、通用工具 |

## 2. 核心类设计

### 2.1 RtspServer

**文件**: `src/core/rtsp_server.h`

| 方法 | 功能 | 参数 | 返回值 |
| :--- | :--- | :--- | :--- |
| `Start()` | 启动服务器监听 | ip: 监听地址, port: 监听端口 | Status |
| `Stop()` | 停止服务器 | - | void |
| `Run()` | 运行事件循环（阻塞） | - | void |
| `BroadcastRtp()` | 广播RTP包到所有会话 | packet: RTP包 | Status |
| `SetSdp()` | 设置SDP内容 | sdp: SDP字符串 | void |
| `GetSdp()` | 获取SDP内容 | - | const std::string& |
| `is_running()` | 获取运行状态 | - | bool |
| `port()` | 获取端口 | - | int |

**成员变量**:
- `running_`: 服务器运行状态
- `port_`: 监听端口
- `sdp_`: SDP内容（外部设置）
- `event_loop_`: EpollLoop事件循环实例
- `listener_`: Listener监听实例
- `sessions_`: Session映射表（fd → unique_ptr<RtspSession>）

**事件回调**:
- `OnNewConnection(fd)`: 新连接事件
- `OnRead(fd)`: 读事件
- `OnWrite(fd)`: 写事件
- `OnError(fd)`: 错误事件

### 2.2 RtspSession

**文件**: `src/core/rtsp_session.h`

| 方法 | 功能 | 参数 | 返回值 |
| :--- | :--- | :--- | :--- |
| `ProcessData()` | 处理接收到的数据 | data: 数据内容 | Status |
| `ForwardRtp()` | 透传RTP包 | packet: RTP包 | Status |
| `Close()` | 关闭会话 | - | void |
| `connection()` | 获取连接 | - | Connection* |
| `state()` | 获取状态 | - | RtspSessionState |
| `session_id()` | 获取会话ID | - | const std::string& |

**成员变量**:
- `server_`: 服务器指针（用于获取SDP等配置）
- `conn_`: 连接指针
- `state_`: 会话状态
- `session_id_`: 会话ID
- `parser_`: RTSP解析器
- `builder_`: RTSP响应构建器
- `rtp_forwarder_`: RTP转发器
- `url_`: 请求URL
- `transport_`: Transport头内容

**状态枚举**:
```cpp
enum class RtspSessionState {
    kInit,          // 初始状态
    kOptionsSent,   // OPTIONS已发送
    kDescribeSent,  // DESCRIBE已发送
    kSetupSent,     // SETUP已发送
    kPlaying,       // 播放中
    kTeardown       // 已关闭
};
```

**请求处理方法**:
- `HandleOptions()`
- `HandleDescribe()`
- `HandleSetup()`
- `HandlePlay()`
- `HandlePause()`
- `HandleTeardown()`

### 2.3 C API 接口

**文件**: `include/rtsp_server.h`

| 函数 | 功能 | 参数 | 返回值 |
| :--- | :--- | :--- | :--- |
| `rtsp_server_create()` | 创建服务器 | - | RtspServer* |
| `rtsp_server_destroy()` | 销毁服务器 | server: 服务器句柄 | void |
| `rtsp_server_start()` | 启动服务器 | server, ip, port | 错误码 |
| `rtsp_server_stop()` | 停止服务器 | server | void |
| `rtsp_server_run()` | 运行事件循环 | server | void |
| `rtsp_server_send_rtp()` | 发送RTP包 | server, data, len, stream_index | 错误码 |
| `rtsp_server_set_sdp()` | 设置SDP内容 | server, sdp | 错误码 |
| `rtsp_server_is_running()` | 检查运行状态 | server | 0/1 |
| `rtsp_server_get_port()` | 获取端口 | server | 端口号 |

### 2.4 线程安全策略（V1.0）

**核心策略**：单线程模型，所有 API 必须在同一线程调用。

| API | 线程安全 | 说明 |
| :--- | :--- | :--- |
| `rtsp_server_create()` | 不安全 | 必须在poll线程调用 |
| `rtsp_server_destroy()` | 不安全 | 必须在poll线程调用 |
| `rtsp_server_start()` | 不安全 | 必须在poll线程调用 |
| `rtsp_server_stop()` | 不安全 | 必须在poll线程调用 |
| `rtsp_server_run()` | 不安全 | 阻塞调用，必须在poll线程调用 |
| `rtsp_server_send_rtp()` | 不安全 | 必须在poll线程调用 |
| `rtsp_server_set_sdp()` | 不安全 | 必须在poll线程调用 |

## 3. 网络工具层设计

### 3.1 EventLoop 抽象接口

**文件**: `src/net/event_loop.h`

```cpp
class EventLoop {
public:
    virtual ~EventLoop() = default;
    
    enum Event {
        kRead = 1 << 0,
        kWrite = 1 << 1,
    };
    
    virtual int AddFd(int fd, int events) = 0;
    virtual int ModifyFd(int fd, int events) = 0;
    virtual int RemoveFd(int fd) = 0;
    virtual int Wait(int timeout_ms, std::vector<struct EventResult>& results) = 0;
};
```

### 3.2 EpollLoop（Linux实现）

**文件**: `src/net/epoll_loop.h`

| 方法 | 功能 | 参数 | 返回值 |
| :--- | :--- | :--- | :--- |
| `AddFd()` | 添加文件描述符到epoll | fd, events | int |
| `ModifyFd()` | 修改文件描述符事件 | fd, events | int |
| `RemoveFd()` | 移除文件描述符 | fd | int |
| `Wait()` | 等待事件 | timeout_ms, results | int |

**成员变量**:
- `epoll_fd_`: epoll文件描述符
- `events_`: epoll事件数组

### 3.3 Socket 工具类

**文件**: `src/net/socket.h`

| 方法 | 功能 | 参数 | 返回值 |
| :--- | :--- | :--- | :--- |
| `Create()` | 创建socket | domain, type, protocol | bool |
| `SetReuseAddr()` | 设置SO_REUSEADDR | enable | bool |
| `SetNonBlocking()` | 设置非阻塞 | enable | bool |
| `SetNoSigPipe()` | 设置SO_NOSIGPIPE | enable | bool |
| `SetTcpNoDelay()` | 设置TCP_NODELAY | enable | bool |
| `Bind()` | 绑定地址 | addr, port | bool |
| `Listen()` | 开始监听 | backlog | bool |
| `Accept()` | 接受连接 | addr | int |
| `Connect()` | 连接服务器 | addr, port | bool |
| `Send()` | 发送数据 | data, len | ssize_t |
| `Recv()` | 接收数据 | buf, len | ssize_t |
| `Close()` | 关闭socket | - | void |
| `fd()` | 获取文件描述符 | - | int |
| `IsValid()` | 检查有效性 | - | bool |

### 3.4 Connection 工具类

**文件**: `src/net/connection.h`

| 方法 | 功能 | 参数 | 返回值 |
| :--- | :--- | :--- | :--- |
| `Recv()` | 接收数据到缓冲区 | - | ssize_t |
| `Send()` | 发送数据 | data, len | ssize_t |
| `Flush()` | 发送缓冲区数据 | - | ssize_t |
| `ReadLine()` | 读取一行 | line | bool |
| `GetReadBuffer()` | 获取读缓冲区 | - | const char* |
| `GetReadBufferSize()` | 获取读缓冲区大小 | - | size_t |
| `Consume()` | 消费缓冲区数据 | len | void |
| `fd()` | 获取文件描述符 | - | int |
| `IsWritable()` | 是否可写 | - | bool |
| `Close()` | 关闭连接 | - | void |

**成员变量**:
- `fd_`: 文件描述符
- `read_buffer_`: 读环形缓冲区
- `write_buffer_`: 写环形缓冲区

### 3.5 Listener 工具类

**文件**: `src/net/listener.h`

| 方法 | 功能 | 参数 | 返回值 |
| :--- | :--- | :--- | :--- |
| `Listen()` | 开始监听 | addr, port | bool |
| `Accept()` | 接受新连接 | client_addr | int |
| `fd()` | 获取监听socket | - | int |
| `Close()` | 关闭监听 | - | void |

**成员变量**:
- `socket_`: Socket实例

## 4. RTP 透传实现

### 4.1 RtpPacket

**文件**: `src/rtp/rtp_packet.h`

```cpp
struct RtpPacket {
    const uint8_t* data;   // RTP包数据指针（已包含完整RTP头）
    size_t len;            // RTP包长度
    int stream_index;      // 流索引: 0=RTP, 1=RTCP
};
```

### 4.2 RtpForwarder

**文件**: `src/rtp/rtp_forwarder.h`

| 方法 | 功能 | 参数 | 返回值 |
| :--- | :--- | :--- | :--- |
| `Forward()` | 转发RTP包 | packet, connection | Status |

### 4.3 RTP包推送流程

```
rtsp_server_send_rtp(server, data, len, stream_index)
    │
    ▼
RtspServer::BroadcastRtp(packet)
    │
    └──► 遍历所有Session（仅PLAYING状态）
            │
            └──► RtspSession::ForwardRtp(packet)
                    │
                    └──► RtpForwarder::Forward(packet, conn)
                            │
                            └──► TCP模式: 添加interleaved frame头后发送
                                  channel=packet.stream_index
```

### 4.4 TCP模式处理（RTP over RTSP）

```
用户输入的RTP包格式：
┌─────────────────────────────────────────────────────────┐
│  RTP Header (12 bytes) + RTP Payload (N bytes)         │
└─────────────────────────────────────────────────────────┘

发送时添加interleaved frame头：
┌─────────────────────────────────────────────────────────┐
│  $ + channel(1 byte) + length(2 bytes) + RTP包原始数据  │
│  示例：$00 04 D0 [RTP数据...]                           │
└─────────────────────────────────────────────────────────┘
```

## 5. Doxygen 注释规范

### 5.1 头文件注释
每个头文件必须包含文件级注释，说明文件功能：

```cpp
/**
 * @file xxx.h
 * @brief 文件功能简要描述
 *
 * 文件功能详细描述，可以包含使用说明、设计思路等。
 */
```

### 5.2 枚举注释
枚举类型及其成员必须添加注释：

```cpp
/**
 * @brief 错误码定义
 */
typedef enum RtspErrorCode
{
    RTSP_OK = 0,              /**< 操作成功 */
    RTSP_ERROR = -1,          /**< 通用错误 */
} RtspErrorCode;
```

### 5.3 结构体注释
结构体及其字段必须添加注释：

```cpp
/**
 * @brief 服务器配置结构体
 *
 * 用于创建服务器时传递配置参数。
 */
typedef struct RtspServerConfig
{
    int port;                /**< 监听端口，默认554 */
    const char* ip;          /**< 监听地址，默认"0.0.0.0" */
} RtspServerConfig;
```

### 5.4 函数注释
所有对外暴露的函数必须添加完整的 doxygen 注释：

```cpp
/**
 * @brief 函数功能描述
 *
 * 函数详细描述，可以包含实现原理、注意事项等。
 *
 * @param param1 参数1说明
 * @param param2 参数2说明
 * @return 返回值说明
 *
 * @code
 * // 使用示例
 * int ret = function_name(param1, param2);
 * @endcode
 *
 * @note 注意事项
 */
return_type function_name(type param1, type param2);
```

### 5.5 C++ 类注释
C++ 类及其成员函数必须添加注释：

```cpp
/**
 * @brief 类功能描述
 */
class RtspServer
{
public:
    /**
     * @brief 构造函数
     *
     * @param param 参数说明
     */
    RtspServer(int param);
};
```

### 5.6 注释语言
- 对外接口头文件（`include/rtsp_server.h`）使用中文注释
- 内部实现文件（`src/` 目录下）使用中文注释
- 保持注释语言一致性

## 6. 目录结构

```
src/
├── api/                          # C接口层
│   ├── rtsp_server_api.cc        # C接口实现
├── core/                         # Server核心层
│   ├── rtsp_server.cc            # RtspServer实现
│   ├── rtsp_server.h             # RtspServer头文件
│   ├── rtsp_session.cc           # RtspSession实现
│   └── rtsp_session.h            # RtspSession头文件
├── net/                          # 网络工具层
│   ├── event_loop.h              # EventLoop抽象接口
│   ├── epoll_loop.cc             # EpollLoop实现
│   ├── epoll_loop.h              # EpollLoop头文件
│   ├── socket.cc                 # Socket工具类实现
│   ├── socket.h                  # Socket工具类头文件
│   ├── connection.cc             # Connection工具类实现
│   ├── connection.h              # Connection工具类头文件
│   ├── listener.cc               # Listener工具类实现
│   └── listener.h                # Listener工具类头文件
├── protocol/                     # RTSP协议层
│   ├── rtsp_parser.cc            # RTSP请求解析
│   ├── rtsp_parser.h             # RTSP解析头文件
│   ├── rtsp_builder.cc           # RTSP响应构建
│   └── rtsp_builder.h            # RTSP构建头文件
├── rtp/                          # RTP层（仅透传）
│   ├── rtp_packet.h              # RTP包结构定义
│   ├── rtp_forwarder.cc          # RTP包转发器
│   └── rtp_forwarder.h           # RTP包转发器头文件
├── buffer/                       # 缓冲区层（独立组件）
│   ├── ring_buffer.cc            # 环形缓冲区实现
│   └── ring_buffer.h             # 环形缓冲区头文件
└── util/                         # 工具层
    ├── log.cc                    # 日志实现
    ├── log.h                     # 日志头文件
    ├── status.h                  # 错误码定义
    ├── resource_manager.cc       # 资源追踪器（保留但未使用）
    ├── resource_manager.h        # 资源追踪器头文件
    └── common.h                  # 通用定义

include/
└── rtsp_server.h                 # 对外头文件（纯C）

demo/
└── main.cc                       # C++测试demo

doc/
├── RTSP_SERVER_REQUIREMENTS.md   # 需求文档（不可修改）
└── RTSP_SERVER_IMPLEMENTATION.md # 实现文档（可修改）

CMakeLists.txt                    # 编译配置
```

## 6. 资源生命周期

### 6.1 服务器创建流程

```
rtsp_server_create()
    │
    ▼
new RtspServer()
    │
    ├──► event_loop_ (EpollLoop) 构造
    └──► listener_ (Listener) 构造
```

### 6.2 会话创建流程

```
RtspServer::Run()
    │
    ├──► event_loop_.Wait(timeout_ms)
    │       │
    │       └──► 检测到新连接事件
    │               │
    │               └──► listener_.Accept()
    │                       │
    │                       ├──► 创建Socket（设置SO_NOSIGPIPE、TCP_NODELAY、非阻塞）
    │                       ├──► 创建Connection（带读写缓冲区）
    │                       └──► 创建RtspSession
    │                               │
    │                               └──► sessions_[fd] = std::make_unique<RtspSession>(conn)
    │
    └──► 处理已有会话事件
            │
            └──► session->ProcessData(data)
```

### 6.3 服务器销毁流程

```
rtsp_server_destroy(server)
    │
    ▼
RtspServer::~RtspServer()
    │
    ├──► sessions_.clear() → 所有Session析构
    │       │
    │       └──► Session析构 → Connection析构 → Socket.Close()
    │
    ├──► listener_.Close()
    │
    └──► event_loop_ 析构
```

## 7. 状态码设计

### 7.1 C API 错误码

| 错误码 | 值 | 含义 |
| :--- | :--- | :--- |
| RTSP_OK | 0 | 操作成功 |
| RTSP_ERROR | -1 | 通用错误 |
| RTSP_INVALID_ARGUMENT | -2 | 参数无效 |
| RTSP_NETWORK_ERROR | -3 | 网络错误 |
| RTSP_CLOSED | -4 | 连接已关闭 |
| RTSP_BUFFER_FULL | -5 | 缓冲区满 |
| RTSP_PARSE_ERROR | -6 | 解析错误 |
| RTSP_NOT_IMPLEMENTED | -7 | 未实现 |
| RTSP_TIMEOUT | -8 | 超时 |
| RTSP_LIMIT_EXCEEDED | -9 | 超出限制 |

### 7.2 内部 Status 类

**文件**: `src/util/status.h`

| 状态码 | 含义 |
| :--- | :--- |
| `kOk` | 操作成功 |
| `kError` | 通用错误 |
| `kInvalidArgument` | 参数无效 |
| `kNetworkError` | 网络错误 |
| `kClosed` | 连接已关闭 |
| `kBufferFull` | 缓冲区满 |
| `kParseError` | 解析错误 |
| `kNotImplemented` | 未实现 |
| `kTimeout` | 超时 |
| `kLimitExceeded` | 超出限制 |

## 8. 编译与部署

### 8.1 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.10)
project(rtsp_server VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(src)
include_directories(include)

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

install(TARGETS rtsp_server DESTINATION lib)
install(FILES include/rtsp_server.h DESTINATION include)

add_executable(rtsp_server_demo demo/main.cc)
target_link_libraries(rtsp_server_demo rtsp_server)
```

### 8.2 编译命令

```bash
mkdir -p build && cd build
cmake ..
make
```

### 8.3 产物

| 产物 | 路径 | 说明 |
| :--- | :--- | :--- |
| 静态库 | `build/librtsp_server.a` | RTSP Server库 |
| 头文件 | `include/rtsp_server.h` | 对外接口 |
| demo | `build/rtsp_server_demo` | 测试程序 |

## 9. 扩展性设计

| 扩展点 | 设计方式 |
| :--- | :--- |
| 支持新RTSP方法 | 在RtspSession中添加处理分支 |
| 支持RTP over UDP | 在RtpForwarder中添加UDP发送逻辑 |
| 支持认证 | 添加auth模块，在RtspSession中集成 |
| 日志级别控制 | 在log.h中添加级别配置 |
| 支持TLS | 添加tls模块，在Socket中集成 |
| 跨平台支持 | 实现EventLoop的其他平台实现（kqueue/IOCP） |

## 10. 改进点规划

### 10.1 线程安全支持（V1.2）

- 在 `RtspServer` 中添加 `std::mutex` 保护 `sessions_`
- 提供线程安全版本 API：`rtsp_server_send_rtp_thread_safe()`
- 使用读写锁优化读多写少场景

### 10.2 UDP 传输支持（V1.4）

- 在 `RtspSession` 中添加 UDP socket 管理
- 在 `RtspParser` 中解析 Transport 头的 client_port
- 在 `RtpForwarder` 中添加 UDP 发送逻辑

### 10.3 TLS 支持（V1.5）

- 添加 `src/net/tls_socket.h`，封装 TLS socket
- 在 `Connection` 中支持 TLS 握手
- 在 API 中添加证书路径配置

### 10.4 版本规划

| 版本 | 功能 | 状态 |
| :--- | :--- | :--- |
| V1.0 | 基础功能：RTSP协议处理、RTP透传(TCP)、C API、单线程、SDP动态更新、配置结构体、最大会话数限制 | 开发中 |
| V1.1 | 统计信息、超时机制 | 规划中 |
| V1.2 | 线程安全支持 | 规划中 |
| V1.3 | 错误处理优化、日志级别控制 | 规划中 |
| V1.4 | UDP传输支持 | 规划中 |
| V1.5 | TLS支持 | 规划中 |

## 11. 设计变更记录

| 变更编号 | 变更内容 | 原因 | 版本 |
| :--- | :--- | :--- | :--- |
| CHG-001 | 移除RtspServerManager，改为不透明指针 | 简化实现，单实例已满足需求 | V1.0 |
| CHG-002 | 移除ResourceManager追踪机制 | RAII+unique_ptr已足够保证资源安全 | V1.0 |
| CHG-003 | 简化API，移除配置结构体 | 减少复杂度，核心参数通过函数传递 | V1.0 |
| CHG-004 | Session状态机改为阶段式 | 更精细的状态追踪 | V1.0 |
| CHG-005 | 移除resource_manager文件 | 未被使用，代码冗余 | V1.0 |
| CHG-006 | 添加SDP外部设置机制 | 支持动态设置SDP内容，满足实际使用需求 | V1.0 |
| CHG-007 | 修复OnNewConnection双重删除风险 | 使用unique_ptr管理资源所有权 | V1.0 |
| CHG-008 | API句柄改为void*类型 | 更通用的不透明指针，避免头文件依赖 | V1.0 |
| CHG-009 | 重新添加配置结构体RtspServerConfig | 支持最大会话数、缓冲区大小等配置参数 | V1.0 |
| CHG-010 | rtsp_server_start移除ip/port参数 | 配置统一在create时指定，简化API | V1.0 |
| CHG-011 | 添加rtsp_server_get_active_sessions API | 支持查询当前活跃会话数 | V1.0 |
| CHG-012 | 添加Doxygen注释规范 | 提升代码文档质量，支持自动生成文档 | V1.0 |