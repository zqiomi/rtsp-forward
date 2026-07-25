# RTSP Server 库架构设计文档

## 1. 需求分析

### 1.1 核心需求

| 需求编号 | 需求描述 |
| :--- | :--- |
| REQ-001 | 轻量级RTSP Server库，仅透传RTP包，不做任何音视频处理 |
| REQ-002 | 对外暴露纯C接口，兼容C/C++调用 |
| REQ-003 | 支持多个RTSP服务端实例，通过ID管理 |
| REQ-004 | C++11实现，禁用异常，仅使用RAII、虚函数等轻量特性 |
| REQ-005 | 外部通过input方式输入RTP包 |
| REQ-006 | 编译成静态库，提供测试demo |

### 1.2 非功能需求

| 需求编号 | 需求描述 |
| :--- | :--- |
| NFR-001 | 低内存占用，适合嵌入式场景 |
| NFR-002 | 高性能，支持10+并发会话 |
| NFR-003 | 资源自动管理，避免泄漏 |
| NFR-004 | 单线程模型，简化并发控制（V1.0） |

---

## 2. 架构设计

### 2.1 整体架构

```
┌─────────────────────────────────────────────────────────────────┐
│                      C API 层 (src/api/)                        │
│  rtsp_server_create() / destroy() / start() / stop() / ...     │
│  纯C接口，多实例ID管理，线程安全说明                              │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Server 管理层 (src/core/)                    │
│  RtspServerManager - 管理多个RtspServer实例                      │
│  RtspServer - 管理事件循环和Session生命周期                      │
│  Session - RTSP会话管理                                          │
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

### 2.2 分层职责

| 层级 | 目录 | 职责 |
| :--- | :--- | :--- |
| C API层 | `src/api/` | 纯C对外接口，多实例管理，错误码处理 |
| Server管理层 | `src/core/` | RtspServer核心逻辑，事件循环，Session管理 |
| 网络工具层 | `src/net/` | 事件循环抽象、Socket/Connection/Listener工具类 |
| 协议层 | `src/protocol/` | RTSP协议解析与构建 |
| RTP层 | `src/rtp/` | RTP包结构定义与透传（不解析不打包） |
| 缓冲区 | `src/buffer/` | 环形缓冲区实现（独立组件） |
| 工具 | `src/util/` | 日志、状态码、通用工具 |

---

## 3. 资源管理设计

### 3.1 设计原则

**核心原则**：采用 **RAII + 追踪模式**，资源生命周期由智能指针自动管理，ResourceManager 仅负责注册/追踪/泄漏检测，不主动释放资源。

```cpp
// 资源追踪接口
class Trackable {
public:
    virtual ~Trackable() = default;
    virtual const char* GetName() const = 0;
};
```

### 3.2 ResourceManager 资源追踪器

**文件**: `src/util/resource_manager.h`

```cpp
class ResourceManager {
public:
    ~ResourceManager();

    void Register(Trackable* resource, const char* name);
    void Unregister(Trackable* resource);
    void DumpLeaks();  // 检测并打印未注销的资源（泄漏检测）

    size_t GetResourceCount() const { return resources_.size(); }

private:
    struct ResourceEntry {
        Trackable* resource;
        const char* name;
    };
    std::vector<ResourceEntry> resources_;
};
```

**关键设计要点**：
- `Register()`: 注册资源到追踪列表
- `Unregister()`: 从追踪列表移除（由对象析构时调用）
- `DumpLeaks()`: 在程序退出或服务器销毁时调用，检测资源泄漏
- **不负责内存释放**：资源由智能指针（`std::unique_ptr`）管理

### 3.3 资源生命周期管理模式

```
创建资源:
    std::unique_ptr<Session> session = std::make_unique<Session>(...);
    resource_manager.Register(session.get(), "session");

使用资源:
    session->Process();

资源销毁:
    session.reset();  // 析构时自动调用 Unregister()
```

### 3.4 Session 资源管理示例

```cpp
class Session : public Trackable {
public:
    Session(ResourceManager* rm) : rm_(rm) {
        rm_->Register(this, "session");
    }
    
    ~Session() override {
        rm_->Unregister(this);
    }

    const char* GetName() const override { return "session"; }

private:
    ResourceManager* rm_;
};
```

---

## 4. 核心类设计

### 4.1 C API 接口

**文件**: `include/rtsp_server.h`

#### 4.1.1 数据结构定义

```c
// 服务器配置结构体
typedef struct RtspServerConfig {
    int port;                    // 监听端口，默认554
    const char* sdp_content;     // SDP内容（必需）
    int max_sessions;            // 最大会话数，默认10
    size_t buffer_size;          // 每个连接的缓冲区大小，默认65536
    int connection_timeout_ms;   // 连接超时时间(ms)，默认30000
    int session_timeout_ms;      // 会话空闲超时时间(ms)，默认600000
    int rtp_timeout_ms;          // PLAY状态RTP超时时间(ms)，默认30000
} RtspServerConfig;

// RTP包结构体
typedef struct RtspRtpPacket {
    const uint8_t* data;         // RTP包数据指针（已包含完整RTP头）
    size_t len;                  // RTP包长度
    int stream_index;            // 流索引: 0=RTP, 1=RTCP
} RtspRtpPacket;

// 服务器统计信息结构体
typedef struct RtspServerStats {
    int active_sessions;         // 当前活跃会话数
    int total_connections;       // 总连接数
    int total_rtp_packets;       // 总RTP包转发数
    int total_errors;            // 总错误数
} RtspServerStats;
```

#### 4.1.2 API 函数列表

| 函数 | 功能 | 参数 | 返回值 |
| :--- | :--- | :--- | :--- |
| `rtsp_server_create()` | 创建服务器 | `config`: 配置 | server_id / 错误码 |
| `rtsp_server_destroy()` | 销毁服务器 | `server_id`: 服务器ID | 0 / 错误码 |
| `rtsp_server_start()` | 启动服务器 | `server_id`: 服务器ID | 0 / 错误码 |
| `rtsp_server_stop()` | 停止服务器 | `server_id`: 服务器ID | 0 / 错误码 |
| `rtsp_server_input_rtp()` | 输入RTP包（透传到所有活跃会话） | `server_id`, `packet`: RTP包 | 0 / 错误码 |
| `rtsp_server_poll()` | 执行事件循环 | `server_id`: 服务器ID, `timeout_ms`: 超时(ms) | 事件数 / 错误码 |
| `rtsp_server_get_active_sessions()` | 获取活跃会话数 | `server_id`: 服务器ID | 会话数 / 错误码 |
| `rtsp_server_get_stats()` | 获取服务器统计信息 | `server_id`: 服务器ID, `stats`: 输出统计信息 | 0 / 错误码 |
| `rtsp_server_update_sdp()` | 更新SDP内容 | `server_id`: 服务器ID, `sdp_content`: 新SDP | 0 / 错误码 |

### 4.2 线程安全策略（V1.0）

**核心策略**：单线程模型，所有 API 必须在同一线程调用。

| API | 线程安全 | 说明 |
| :--- | :--- | :--- |
| `rtsp_server_create()` | 线程安全 | 内部使用互斥锁保护全局服务器列表 |
| `rtsp_server_destroy()` | 线程安全 | 内部使用互斥锁保护全局服务器列表 |
| `rtsp_server_start()` | 不安全 | 必须在 poll 线程调用 |
| `rtsp_server_stop()` | 不安全 | 必须在 poll 线程调用 |
| `rtsp_server_poll()` | 不安全 | 必须在同一线程持续调用 |
| `rtsp_server_input_rtp()` | 不安全 | 必须在 poll 线程调用 |
| `rtsp_server_get_active_sessions()` | 不安全 | 必须在 poll 线程调用 |

**线程安全版本规划（V1.2）**：
- 在 RtspServer 中添加 `std::mutex` 保护共享数据
- 提供线程安全版本 API：`rtsp_server_input_rtp_thread_safe()`

### 4.3 RtspServerManager

**文件**: `src/core/server_manager.h`

| 方法 | 功能 | 参数 | 返回值 |
| :--- | :--- | :--- | :--- |
| `Create()` | 创建服务器实例 | `config`: 配置 | server_id |
| `Destroy()` | 销毁服务器实例 | `server_id`: 服务器ID | Status |
| `GetServer()` | 获取服务器指针 | `server_id`: 服务器ID | RtspServer* |
| `GetNextId()` | 获取下一个ID | - | int |

**线程安全**：内部使用 `std::mutex` 保护 `servers_` map。

### 4.4 RtspServer

**文件**: `src/core/server.h`

| 方法 | 功能 | 参数 | 返回值 |
| :--- | :--- | :--- | :--- |
| `Start()` | 启动服务器 | - | Status |
| `Stop()` | 停止服务器 | - | void |
| `Poll()` | 执行一次事件循环 | `timeout_ms`: 超时(ms) | int |
| `InputRtp()` | 输入RTP包 | `packet`: RTP包 | Status |
| `GetActiveSessions()` | 获取活跃会话数 | - | size_t |
| `GetId()` | 获取服务器ID | - | int |
| `UpdateSdp()` | 更新SDP内容 | `sdp_content`: 新SDP | Status |
| `GetStats()` | 获取统计信息 | `stats`: 输出统计信息 | void |

### 4.5 Session

**文件**: `src/core/session.h`

| 方法 | 功能 | 参数 | 返回值 |
| :--- | :--- | :--- | :--- |
| `Process()` | 处理会话事件 | - | Status |
| `ForwardRtp()` | 透传RTP包 | `packet`: RTP包 | Status |
| `IsPlaying()` | 是否在播放状态 | - | bool |
| `Close()` | 关闭会话 | - | void |
| `fd()` | 获取文件描述符 | - | int |
| `UpdateLastActiveTime()` | 更新最后活动时间 | - | void |
| `CheckTimeout()` | 检查是否超时 | - | bool |

### 4.6 RTSP 协议状态机

```
                    ┌─────────────┐
                    │   IDLE      │
                    │ (已连接)    │
                    └──────┬──────┘
                           │ OPTIONS/DESCRIBE
                           ▼
                    ┌─────────────┐
                    │   READY     │
                    │ (已准备)    │
                    └──────┬──────┘
                           │ SETUP
                           ▼
                    ┌─────────────┐
              ┌─────│   SETUPED   │─────┐
              │     │ (已建立)    │     │
              │     └─────────────┘     │
              │ PLAY                    │ TEARDOWN
              ▼                         ▼
       ┌─────────────┐           ┌─────────────┐
       │   PLAYING   │           │   CLOSED    │
       │ (播放中)    │───────────▶│ (已关闭)    │
       └──────┬──────┘  PAUSE/   └─────────────┘
              │         TEARDOWN
              │ PAUSE
              ▼
       ┌─────────────┐
       │   PAUSED    │
       │ (已暂停)    │
       └──────┬──────┘
              │ PLAY
              ▼
```

### 4.7 RTSP 方法处理

| RTSP 方法 | 状态检查 | 处理逻辑 | 响应码 |
| :--- | :--- | :--- | :--- |
| OPTIONS | 任意状态 | 返回支持的方法列表 | 200 OK |
| DESCRIBE | 任意状态 | 返回 SDP 内容 | 200 OK |
| SETUP | IDLE/READY | 解析 Transport 头，记录客户端端口/模式 | 200 OK |
| PLAY | SETUPED/PAUSED | 设置会话为 PLAYING 状态，开始转发 RTP | 200 OK |
| PAUSE | PLAYING | 设置会话为 PAUSED 状态，暂停 RTP 转发 | 200 OK |
| TEARDOWN | 非 CLOSED | 设置会话为 CLOSED 状态，关闭连接 | 200 OK |

### 4.8 Session ID 生成策略

```cpp
// 生成规则：时间戳 + 递增序号
// 格式：{timestamp}_{sequence}
// 示例：1620000000_001

std::string GenerateSessionId() {
    static std::atomic<int> sequence{0};
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    int seq = sequence.fetch_add(1, std::memory_order_relaxed);
    return std::to_string(timestamp) + "_" + std::to_string(seq);
}
```

### 4.9 Transport 头解析

**支持的 Transport 类型**：
- `RTP/AVP/TCP`：TCP 模式，使用 interleaved frame
- `RTP/AVP/UDP`：UDP 模式，使用独立 RTP/RTCP 端口

**解析示例**：
```
Transport: RTP/AVP/TCP;interleaved=0-1
→ mode: TCP, rtp_channel: 0, rtcp_channel: 1

Transport: RTP/AVP/UDP;client_port=5000-5001
→ mode: UDP, rtp_port: 5000, rtcp_port: 5001
```

---

## 5. 网络工具层设计

### 5.1 EventLoop 抽象接口

**文件**: `src/net/event_loop.h`

```cpp
class EventLoop {
public:
    virtual ~EventLoop() = default;

    // 事件类型
    enum Event {
        kRead = 1 << 0,
        kWrite = 1 << 1,
    };

    virtual int AddFd(int fd, int events) = 0;
    virtual int ModifyFd(int fd, int events) = 0;
    virtual int RemoveFd(int fd) = 0;
    virtual int Wait(int timeout_ms, std::vector<struct EventResult>& results) = 0;
};

struct EventResult {
    int fd;
    int events;
};
```

### 5.2 EpollLoop（Linux 实现）

**文件**: `src/net/epoll_loop.h`

```cpp
class EpollLoop : public EventLoop {
public:
    EpollLoop();
    ~EpollLoop() override;

    int AddFd(int fd, int events) override;
    int ModifyFd(int fd, int events) override;
    int RemoveFd(int fd) override;
    int Wait(int timeout_ms, std::vector<struct EventResult>& results) override;

private:
    int epoll_fd_;
    std::vector<struct epoll_event> events_;
};
```

### 5.3 Socket 工具类

**文件**: `src/net/socket.h`

```cpp
class Socket {
public:
    Socket() : fd_(-1) {}
    ~Socket();

    // 创建socket
    bool Create(int domain, int type, int protocol);
    
    // 设置socket选项
    bool SetReuseAddr(bool enable);
    bool SetNonBlocking(bool enable);
    bool SetNoSigPipe(bool enable);
    bool SetTcpNoDelay(bool enable);
    
    // 绑定地址
    bool Bind(const char* addr, int port);
    
    // 监听
    bool Listen(int backlog);
    
    // 接受连接
    int Accept(struct sockaddr_in* addr);
    
    // 连接
    bool Connect(const char* addr, int port);
    
    // 发送数据
    ssize_t Send(const void* data, size_t len);
    
    // 接收数据
    ssize_t Recv(void* buf, size_t len);
    
    // 关闭socket
    void Close();
    
    // 获取文件描述符
    int fd() const { return fd_; }
    bool IsValid() const { return fd_ >= 0; }

private:
    int fd_;
};
```

### 5.4 Connection 工具类

**文件**: `src/net/connection.h`

```cpp
class Connection {
public:
    Connection(int fd, size_t buffer_size);
    ~Connection();

    // 接收数据到缓冲区
    ssize_t Recv();
    
    // 发送数据
    ssize_t Send(const void* data, size_t len);
    
    // 发送缓冲区数据
    ssize_t Flush();
    
    // 从输入缓冲区读取一行
    bool ReadLine(std::string& line);
    
    // 获取缓冲区可读数据
    const char* GetReadBuffer() const;
    size_t GetReadBufferSize() const;
    
    // 消费缓冲区数据
    void Consume(size_t len);
    
    // 获取文件描述符
    int fd() const { return fd_; }
    
    // 是否可写
    bool IsWritable() const;
    
    // 关闭连接
    void Close();

private:
    int fd_;
    std::unique_ptr<RingBuffer> read_buffer_;
    std::unique_ptr<RingBuffer> write_buffer_;
};
```

### 5.5 Listener 工具类

**文件**: `src/net/listener.h`

```cpp
class Listener {
public:
    Listener();
    ~Listener();

    // 开始监听
    bool Listen(const char* addr, int port);
    
    // 接受新连接
    int Accept(struct sockaddr_in* client_addr);
    
    // 获取监听socket
    int fd() const { return socket_.fd(); }
    
    // 关闭监听
    void Close();

private:
    Socket socket_;
};
```

---

## 6. RTP 透传设计

### 6.1 设计理念

库的核心设计原则是**完全剥离音视频相关逻辑**，仅提供 RTP 包的透传功能：

- **不解析 RTP 包**：不读取 RTP 头中的任何字段（版本、负载类型、序列号、时间戳等）
- **不打包 RTP 包**：不将原始音视频数据封装成 RTP 包
- **不修改 RTP 包**：不对 RTP 包内容做任何修改，直接透传
- **不处理音视频**：不涉及编解码、时间戳同步、帧率控制等

### 6.2 RTP 包格式

```
用户输入的 RTP 包格式（完整的 RTP 数据）：
┌─────────────────────────────────────────────────────────┐
│  RTP Header (12 bytes) + RTP Payload (N bytes)         │
│  已包含完整的 RTP 头：版本、负载类型、序列号、时间戳等    │
└─────────────────────────────────────────────────────────┘

TCP 模式处理（RTP over RTSP）：
┌─────────────────────────────────────────────────────────┐
│  $ + channel(1 byte) + length(2 bytes) + RTP包原始数据  │
│  示例：$00 04 D0 [RTP数据...]                           │
│  仅添加RTSP interleaved frame头，不修改RTP内容           │
└─────────────────────────────────────────────────────────┘

UDP 模式处理：
┌─────────────────────────────────────────────────────────┐
│  直接透传                                               │
│  RTP包原始数据 → 直接发送到客户端RTP端口（不修改）         │
└─────────────────────────────────────────────────────────┘
```

### 6.3 RTP 包推送流程

```
rtsp_server_input_rtp(server_id, packet)
    │
    ▼
RtspServer::InputRtp(packet)
    │
    └──► 遍历所有活跃Session（仅PLAYING状态）
            │
            └──► Session::ForwardRtp(packet)
                    │
                    ├──► TCP模式: 添加interleaved frame头后发送
                    │         channel=packet.stream_index
                    │
                    └──► UDP模式: 直接发送到客户端RTP端口
```

### 6.4 职责边界

| 职责 | 库负责 | 用户负责 |
| :--- | :--- | :--- |
| RTSP 协议处理 | ✓ | |
| RTP 包透传 | ✓ | |
| RTP 包封装 | | ✓ |
| 音视频编解码 | | ✓ |
| 时间戳管理 | | ✓ |
| 帧率控制 | | ✓ |
| SDP 内容生成 | | ✓ |

---

## 7. 目录结构

```
src/
├── api/                          # C接口层
│   ├── rtsp_server_api.cc        # C接口实现
│   └── rtsp_server_api.h         # C接口内部头文件
├── core/                         # Server核心层
│   ├── server.cc                 # RtspServer实现
│   ├── server.h                  # RtspServer头文件
│   ├── server_manager.cc         # RtspServerManager实现
│   ├── server_manager.h          # RtspServerManager头文件
│   ├── session.cc                # Session实现
│   └── session.h                 # Session头文件
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
    ├── resource_manager.cc       # 资源追踪器实现
    ├── resource_manager.h        # 资源追踪器头文件
    └── common.h                  # 通用定义

include/
└── rtsp_server.h                 # 对外头文件（纯C）

demo/
└── demo.c                        # C语言测试demo

doc/
└── RTSP_SERVER_DESIGN.md         # 架构设计文档

CMakeLists.txt                    # 编译配置
```

---

## 8. 资源生命周期

### 8.1 服务器创建流程

```
rtsp_server_create(config)
    │
    ▼
RtspServerManager::Create()
    │
    ├──► 分配 server_id（线程安全）
    ├──► 创建 ResourceManager（用于泄漏检测）
    ├──► 创建 EventLoop (EpollLoop)
    ├──► 创建 Listener（注册到ResourceManager）
    ├──► 创建 RtspServer（注册到ResourceManager）
    └──► 保存到 servers_ map
```

### 8.2 会话创建流程

```
RtspServer::Poll(timeout_ms)
    │
    ├──► EventLoop::Wait(timeout_ms)
    │       │
    │       └──► 检测到新连接事件
    │               │
    │               └──► Listener::Accept()
    │                       │
    │                       ├──► 创建 Socket（设置SO_NOSIGPIPE、TCP_NODELAY）
    │                       ├──► 创建 Connection（带读写缓冲区）
    │                       └──► 创建 Session（注册到ResourceManager）
    │                               │
    │                               └──► 加入 sessions_ map
    │
    └──► 处理已有会话事件
            │
            └──► Session::Process()
```

### 8.3 服务器销毁流程

```
rtsp_server_destroy(server_id)
    │
    ▼
RtspServerManager::Destroy()
    │
    ├──► RtspServer::Stop()
    │       │
    │       └──► 关闭所有Session
    │               │
    │               └──► 每个Session析构时自动触发Unregister()
    │
    ├──► ResourceManager::DumpLeaks()
    │       │
    │       └──► 检测并打印未注销的资源（泄漏检测）
    │
    └──► 从 servers_ map 移除（线程安全）
```

---

## 9. 状态码设计

### 9.1 C API 错误码

| 错误码 | 含义 |
| :--- | :--- |
| `RTSP_SERVER_OK` | 操作成功 |
| `RTSP_SERVER_ERR_PARAM` | 参数无效 |
| `RTSP_SERVER_ERR_MEM` | 内存分配失败 |
| `RTSP_SERVER_ERR_NET` | 网络错误 |
| `RTSP_SERVER_ERR_EXIST` | 服务器已存在 |
| `RTSP_SERVER_ERR_NOT_FOUND` | 服务器不存在 |
| `RTSP_SERVER_ERR_STATE` | 状态错误（如已启动/已停止） |
| `RTSP_SERVER_ERR_LIMIT` | 超出最大会话数限制 |
| `RTSP_SERVER_ERR_TIMEOUT` | 连接/会话超时 |
| `RTSP_SERVER_ERR_BUFFER_FULL` | 缓冲区满 |
| `RTSP_SERVER_ERR_PARSE` | RTSP解析错误 |
| `RTSP_SERVER_ERR_METHOD_NOT_SUPPORTED` | 不支持的RTSP方法 |

### 9.2 内部 Status 类

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

---

## 10. 编译与部署

### 10.1 CMakeLists.txt 结构

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
    src/api/rtsp_server_api.cc
    src/core/server.cc
    src/core/server_manager.cc
    src/core/session.cc
    src/net/epoll_loop.cc
    src/net/socket.cc
    src/net/connection.cc
    src/net/listener.cc
    src/protocol/rtsp_parser.cc
    src/protocol/rtsp_builder.cc
    src/rtp/rtp_forwarder.cc
    src/buffer/ring_buffer.cc
    src/util/log.cc
    src/util/resource_manager.cc
)

add_executable(rtsp_server_demo
    demo/demo.c
)

target_link_libraries(rtsp_server_demo
    rtsp_server
)

# 安装目标
install(TARGETS rtsp_server DESTINATION lib)
install(FILES include/rtsp_server.h DESTINATION include)
```

### 10.2 编译命令

```bash
mkdir -p build && cd build
cmake ..
make
```

### 10.3 产物

| 产物 | 路径 | 说明 |
| :--- | :--- | :--- |
| 静态库 | `build/librtsp_server.a` | RTSP Server库 |
| 头文件 | `include/rtsp_server.h` | 对外接口 |
| demo | `build/rtsp_server_demo` | 测试程序 |

---

## 11. API 使用示例

### 11.1 C语言示例（完整）

```c
#include <rtsp_server.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

static int g_running = 1;
static int g_server_id = -1;

void sig_handler(int sig) {
    printf("Received signal %d, shutting down...\n", sig);
    g_running = 0;
}

int main() {
    // 注册信号处理
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    // 配置服务器
    RtspServerConfig config = {
        .port = 554,
        .sdp_content = "v=0\r\n"
                       "o=- 0 0 IN IP4 0.0.0.0\r\n"
                       "s=RTSP Server\r\n"
                       "c=IN IP4 0.0.0.0\r\n"
                       "t=0 0\r\n"
                       "m=video 0 RTP/AVP 96\r\n"
                       "a=rtpmap:96 H264/90000\r\n"
                       "a=control:track0\r\n",
        .max_sessions = 10,
        .buffer_size = 65536,
        .connection_timeout_ms = 30000,
        .session_timeout_ms = 600000,
        .rtp_timeout_ms = 30000
    };

    // 创建服务器
    g_server_id = rtsp_server_create(&config);
    if (g_server_id < 0) {
        printf("Failed to create server: %d\n", g_server_id);
        return 1;
    }
    printf("Server created with ID: %d\n", g_server_id);

    // 启动服务器
    int ret = rtsp_server_start(g_server_id);
    if (ret != 0) {
        printf("Failed to start server: %d\n", ret);
        return 1;
    }
    printf("Server started on port %d\n", config.port);

    // 事件循环
    while (g_running) {
        // 处理RTSP事件，超时100ms
        ret = rtsp_server_poll(g_server_id, 100);
        if (ret < 0) {
            printf("Poll error: %d\n", ret);
            break;
        }

        // 模拟输入RTP包（实际应用中从编码器获取）
        // uint8_t rtp_data[1024];
        // size_t rtp_len = get_rtp_from_encoder(rtp_data, sizeof(rtp_data));
        // if (rtp_len > 0) {
        //     RtspRtpPacket packet = {
        //         .data = rtp_data,
        //         .len = rtp_len,
        //         .stream_index = 0
        //     };
        //     rtsp_server_input_rtp(g_server_id, &packet);
        // }

        // 获取统计信息
        RtspServerStats stats;
        rtsp_server_get_stats(g_server_id, &stats);
        printf("\rActive sessions: %d | Total RTP: %d", 
               stats.active_sessions, stats.total_rtp_packets);
        fflush(stdout);
    }

    // 停止服务器
    printf("\nStopping server...\n");
    rtsp_server_stop(g_server_id);
    
    // 销毁服务器
    rtsp_server_destroy(g_server_id);
    printf("Server destroyed.\n");

    return 0;
}
```

---

## 12. 代码规范

### 12.1 命名规范

| 类型 | 规范 | 示例 |
| :--- | :--- | :--- |
| C API函数 | `rtsp_server_`前缀 + snake_case | `rtsp_server_create` |
| C++类 | PascalCase | `RtspServer` |
| C++函数/变量 | snake_case | `process_request` |
| 常量 | `k`前缀 + PascalCase | `kMaxSessions` |
| 枚举值 | `k`前缀 + PascalCase | `kRead` |

### 12.2 资源管理规范

- 所有需要追踪的对象继承自 `Trackable` 接口
- 对象构造时注册到 `ResourceManager`，析构时自动注销
- 使用 `std::unique_ptr` 管理资源生命周期，禁止裸指针管理
- `ResourceManager` 仅做追踪和泄漏检测，不负责内存释放

### 12.3 错误处理规范

- C API 返回整数错误码
- C++内部使用 `Status` 类
- 所有可能失败的函数必须检查返回值
- 使用 `LOG_ERROR` 记录错误信息

### 12.4 网络编程规范

- Socket 创建时设置 `SO_NOSIGPIPE`（防止客户端断开时触发 SIGPIPE）
- TCP 连接建立后设置 `TCP_NODELAY`（禁用 Nagle 算法，降低 RTP 延迟）
- 所有 socket 设置为非阻塞模式
- 读取操作检查 `EAGAIN/EWOULDBLOCK` 错误

---

## 13. 安全性考虑

| 风险点 | 风险等级 | 缓解措施 |
| :--- | :--- | :--- |
| 缓冲区溢出 | 高 | 使用固定大小缓冲区，检查写入长度 |
| 连接耗尽 | 中 | 设置最大连接数限制 |
| DoS攻击 | 中 | 实现连接超时、会话超时、RTP超时机制 |
| 恶意请求 | 中 | 验证RTSP协议格式，检查方法支持 |
| 资源泄漏 | 中 | 使用ResourceManager追踪，智能指针管理 |
| SIGPIPE | 中 | 设置SO_NOSIGPIPE或忽略SIGPIPE信号 |

---

## 14. 扩展性设计

| 扩展点 | 设计方式 |
| :--- | :--- |
| 支持新RTSP方法 | 在Session中添加处理分支 |
| 支持RTP over UDP | 在RtpForwarder中添加UDP发送逻辑 |
| 支持认证 | 添加auth模块，在Session中集成 |
| 日志级别控制 | 在log.h中添加级别配置 |
| 支持TLS | 添加tls模块，在Socket中集成 |
| 跨平台支持 | 实现EventLoop的其他平台实现（kqueue/IOCP） |

---

## 15. 改进点规划

### 15.1 线程安全支持（V1.2）

**当前状态**：V1.0 采用单线程模型，所有 API 必须在同一线程调用。

**改进方案**：
- 在 `RtspServer` 中添加 `std::mutex` 保护 `sessions_` 和统计信息
- 提供线程安全版本 API：`rtsp_server_input_rtp_thread_safe()`
- 使用读写锁优化读多写少场景

### 15.2 UDP 传输支持（V1.4）

**当前状态**：V1.0 仅支持 TCP 模式。

**改进方案**：
- 在 `RtspServerConfig` 中添加 `enable_udp` 配置项
- 在 `Session` 中添加 UDP socket 管理
- 在 `RtspParser` 中解析 Transport 头的 client_port
- 在 `RtpForwarder` 中添加 UDP 发送逻辑

### 15.3 TLS 支持（V1.5）

**改进方案**：
- 添加 `src/net/tls_socket.h`，封装 TLS socket
- 在 `Connection` 中支持 TLS 握手
- 在 `RtspServerConfig` 中添加证书路径配置

### 15.4 版本规划

| 版本 | 功能 | 状态 |
| :--- | :--- | :--- |
| V1.0 | 基础功能：RTSP协议处理、RTP透传(TCP)、C API、单实例、单线程 | 开发中 |
| V1.1 | SDP动态更新、统计信息、超时机制 | 规划中 |
| V1.2 | 线程安全支持 | 规划中 |
| V1.3 | 错误处理优化、日志级别控制 | 规划中 |
| V1.4 | UDP传输支持 | 规划中 |
| V1.5 | TLS支持 | 规划中 |
