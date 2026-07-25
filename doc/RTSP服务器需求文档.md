# RTSP Server 需求文档

## 1. 核心需求

| 需求编号 | 需求描述 | 优先级 |
| :--- | :--- | :--- |
| REQ-001 | 轻量级RTSP Server库，仅透传RTP包，不做任何音视频处理 | P0 |
| REQ-002 | 对外暴露纯C接口，兼容C/C++调用 | P0 |
| REQ-003 | 支持RTSP协议标准方法（OPTIONS、DESCRIBE、SETUP、PLAY、PAUSE、TEARDOWN） | P0 |
| REQ-004 | C++11实现，禁用异常，仅使用RAII、虚函数等轻量特性 | P0 |
| REQ-005 | **一个输入，多路转发**：外部通过input方式输入RTP包，同时透传到多个RTSP客户端（支持多个client并发接入取流） | P0 |
| REQ-006 | 支持最大会话数限制配置 | P1 |
| REQ-007 | 支持连接缓冲区大小配置 | P1 |
| REQ-008 | 编译成静态库，提供测试demo | P1 |

## 2. 非功能需求

| 需求编号 | 需求描述 | 优先级 |
| :--- | :--- | :--- |
| NFR-001 | 低内存占用，适合嵌入式场景 | P1 |
| NFR-002 | 高性能，支持10+并发会话 | P1 |
| NFR-003 | 资源自动管理，避免泄漏 | P1 |
| NFR-004 | 单线程模型，简化并发控制（V1.0） | P0 |
| NFR-005 | 所有API必须在同一线程调用 | P0 |

## 3. API 接口需求

### 3.1 数据结构

| 结构名 | 字段 | 类型 | 描述 |
| :--- | :--- | :--- | :--- |
| RtspErrorCode | - | enum | 错误码枚举 |
| RtspServerConfig | - | struct | 服务器配置结构体 |
| void* | - | void* | 服务器句柄（不透明指针，使用void*类型） |

### 3.2 配置结构体字段

| 字段名 | 类型 | 默认值 | 描述 |
| :--- | :--- | :--- | :--- |
| port | int | 554 | 监听端口 |
| ip | const char* | "0.0.0.0" | 监听地址 |
| max_sessions | int | 10 | 最大并发会话数 |
| buffer_size | size_t | 65536 | 每个连接的缓冲区大小 |
| sdp_content | const char* | NULL | SDP内容（可选，可通过API动态设置） |

### 3.3 API 函数

| 函数名 | 功能描述 | 参数 | 返回值 |
| :--- | :--- | :--- | :--- |
| rtsp_server_create | 创建RTSP服务器实例 | config: 配置结构体（可为NULL，使用默认配置） | 服务器句柄(void*)/NULL |
| rtsp_server_destroy | 销毁服务器实例 | server: 服务器句柄 | void |
| rtsp_server_start | 启动服务器监听 | server: 服务器句柄 | 错误码 |
| rtsp_server_stop | 停止服务器 | server: 服务器句柄 | void |
| rtsp_server_run | 运行事件循环（阻塞） | server: 服务器句柄 | void |
| rtsp_server_send_rtp | 发送RTP包到所有播放会话 | server, data, len, stream_index | 错误码 |
| rtsp_server_set_sdp | 设置SDP内容 | server, sdp | 错误码 |
| rtsp_server_is_running | 检查服务器是否运行 | server: 服务器句柄 | 0/1 |
| rtsp_server_get_port | 获取监听端口 | server: 服务器句柄 | 端口号 |

### 3.3 错误码

| 错误码 | 含义 |
| :--- | :--- |
| RTSP_OK | 操作成功 |
| RTSP_ERROR | 通用错误 |
| RTSP_INVALID_ARGUMENT | 参数无效 |
| RTSP_NETWORK_ERROR | 网络错误 |
| RTSP_CLOSED | 连接已关闭 |
| RTSP_BUFFER_FULL | 缓冲区满 |
| RTSP_PARSE_ERROR | 解析错误 |
| RTSP_NOT_IMPLEMENTED | 未实现 |
| RTSP_TIMEOUT | 超时 |
| RTSP_LIMIT_EXCEEDED | 超出限制 |

## 4. RTSP 协议需求

### 4.1 支持的方法

| 方法 | 处理逻辑 | 响应码 |
| :--- | :--- | :--- |
| OPTIONS | 返回支持的方法列表 | 200 OK |
| DESCRIBE | 返回SDP内容 | 200 OK |
| SETUP | 解析Transport头，记录传输参数 | 200 OK |
| PLAY | 设置会话为播放状态，开始转发RTP | 200 OK |
| PAUSE | 设置会话为暂停状态，暂停RTP转发 | 200 OK |
| TEARDOWN | 设置会话为关闭状态，关闭连接 | 200 OK |

### 4.2 状态机

```
                    ┌─────────────┐
                    │   INIT      │
                    │ (初始状态)   │
                    └──────┬──────┘
                           │ OPTIONS
                           ▼
                    ┌─────────────┐
                    │ OPTIONS_SENT│
                    │ (OPTIONS已发)│
                    └──────┬──────┘
                           │ DESCRIBE
                           ▼
                    ┌─────────────┐
                    │DESCRIBE_SENT│
                    │ (DESCRIBE已发)│
                    └──────┬──────┘
                           │ SETUP
                           ▼
                    ┌─────────────┐
              ┌─────│ SETUP_SENT  │─────┐
              │     │ (SETUP已发)  │     │
              │     └─────────────┘     │
              │ PLAY                    │ TEARDOWN
              ▼                         ▼
       ┌─────────────┐           ┌─────────────┐
       │   PLAYING   │           │  TEARDOWN   │
       │ (播放中)    │───────────▶│  (已关闭)   │
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

### 4.3 Transport 头支持

- **RTP/AVP/TCP**：TCP模式，使用interleaved frame
- **RTP/AVP/UDP**：UDP模式，使用独立RTP/RTCP端口（V1.4规划）

### 4.4 Session ID 生成规则

- 格式：`{timestamp}_{sequence}`
- 示例：`1620000000_001`

## 5. RTP 透传需求

### 5.1 核心原则

- **不解析**：不读取RTP头中的任何字段（版本、负载类型、序列号、时间戳等）
- **不打包**：不将原始音视频数据封装成RTP包
- **不修改**：不对RTP包内容做任何修改，直接透传
- **不处理**：不涉及编解码、时间戳同步、帧率控制等

### 5.2 RTP包格式

用户输入的RTP包必须包含完整的RTP头（12字节）+ RTP负载。

### 5.3 职责边界

| 职责 | 库负责 | 用户负责 |
| :--- | :--- | :--- |
| RTSP协议处理 | ✓ | |
| RTP包透传 | ✓ | |
| RTP包封装 | | ✓ |
| 音视频编解码 | | ✓ |
| 时间戳管理 | | ✓ |
| 帧率控制 | | ✓ |
| SDP内容生成 | | ✓ |

## 6. 网络编程需求

- Socket创建时设置`SO_NOSIGPIPE`（防止客户端断开时触发SIGPIPE）
- TCP连接建立后设置`TCP_NODELAY`（禁用Nagle算法，降低RTP延迟）
- 所有socket设置为非阻塞模式
- 读取操作检查`EAGAIN/EWOULDBLOCK`错误

## 7. 安全性需求

| 风险点 | 风险等级 | 缓解措施 |
| :--- | :--- | :--- |
| 缓冲区溢出 | 高 | 使用固定大小缓冲区，检查写入长度 |
| 连接耗尽 | 中 | 设置最大连接数限制 |
| DoS攻击 | 中 | 实现连接超时、会话超时、RTP超时机制 |
| 恶意请求 | 中 | 验证RTSP协议格式，检查方法支持 |
| SIGPIPE | 中 | 设置SO_NOSIGPIPE或忽略SIGPIPE信号 |

## 8. 代码规范需求

### 8.1 命名规范

| 类型 | 规范 | 示例 |
| :--- | :--- | :--- |
| C API函数 | `rtsp_server_`前缀 + snake_case | `rtsp_server_create` |
| C++类 | PascalCase | `RtspServer` |
| C++函数/变量 | snake_case | `process_request` |
| 常量 | `k`前缀 + PascalCase | `kMaxSessions` |
| 枚举值 | `k`前缀 + PascalCase | `kRead` |

### 8.2 资源管理规范

- 使用`std::unique_ptr`管理资源生命周期，禁止裸指针管理
- 资源生命周期由RAII自动管理

### 8.3 错误处理规范

- C API返回整数错误码
- C++内部使用`Status`类
- 所有可能失败的函数必须检查返回值

## 9. 编译需求

- C++11标准
- 禁用异常（-fno-exceptions）
- 编译产物：静态库`librtsp_server.a`
- 提供测试demo程序