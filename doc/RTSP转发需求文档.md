# RTSP 转发需求文档

## 1. 核心需求

| 需求编号 | 需求描述 | 优先级 | 状态 |
| :--- | :--- | :--- | :--- |
| REQ-001 | 轻量级RTSP转发库，仅透传RTP包，不做任何音视频处理 | P0 | ✅ |
| REQ-002 | 对外暴露纯C接口，兼容C/C++调用 | P0 | ✅ |
| REQ-003 | 支持RTSP协议标准方法（OPTIONS、DESCRIBE、SETUP、PLAY、PAUSE、TEARDOWN、GET_PARAMETER、SET_PARAMETER） | P0 | ✅ |
| REQ-004 | C++11实现，禁用异常，仅使用RAII、虚函数等轻量特性 | P0 | ✅ |
| REQ-005 | 一个输入，多路转发：外部输入RTP包，同时透传到多个RTSP客户端 | P0 | ✅ |
| REQ-006 | 支持最大会话数限制配置 | P1 | ✅ |
| REQ-007 | 支持连接缓冲区大小配置 | P1 | ✅ |
| REQ-008 | 编译成静态/动态库，提供测试demo | P1 | ✅ |
| REQ-009 | 支持UDP传输模式 | P1 | ✅ |
| REQ-010 | 支持连接/会话空闲超时自动清理 | P1 | ✅ |

## 2. 非功能需求

| 需求编号 | 需求描述 | 优先级 | 状态 |
| :--- | :--- | :--- | :--- |
| NFR-001 | 低内存占用，适合嵌入式场景 | P1 | ✅ |
| NFR-002 | 高性能，支持10+并发会话 | P1 | ✅ |
| NFR-003 | 资源自动管理，避免泄漏 | P1 | ✅ |
| NFR-004 | 支持双线程模型：主线程事件循环 + 码流输入线程 | P0 | ✅ |
| NFR-005 | RTP转发接口线程安全，可在非主线程调用 | P0 | ✅ |

## 3. API 接口

### 3.1 数据结构

| 结构名 | 描述 |
| :--- | :--- |
| `RtspErrorCode` | 错误码枚举 |
| `RtspForwardConfig` | 服务器配置结构体 |
| `RtspForwardInfo` | 服务器信息结构体 |
| `RtspLogLevel` | 日志级别枚举 |

### 3.2 API 函数

| 函数名 | 功能描述 |
| :--- | :--- |
| `rtsp_forward_create` | 创建服务器实例 |
| `rtsp_forward_destroy` | 销毁服务器实例 |
| `rtsp_forward_start` | 启动服务器监听 |
| `rtsp_forward_stop` | 停止服务器 |
| `rtsp_forward_run` | 运行事件循环（阻塞） |
| `rtsp_forward_send_rtp` | 发送RTP包到所有播放会话 |
| `rtsp_forward_set_sdp` | 设置SDP内容 |
| `rtsp_forward_get_info` | 获取服务器信息（配置、状态、统计） |
| `rtsp_forward_set_log_level` | 设置日志级别 |
| `rtsp_forward_get_log_level` | 获取日志级别 |
| `rtsp_forward_version_string` | 获取版本号字符串 |

## 4. 错误码

| 错误码 | 值 | 含义 |
| :--- | :--- | :--- |
| RTSP_OK | 0 | 操作成功 |
| RTSP_ERROR | -1 | 通用错误 |
| RTSP_INVALID_ARGUMENT | -2 | 参数无效 |
| RTSP_NETWORK_ERROR | -3 | 网络错误 |
| RTSP_CLOSED | -4 | 连接已关闭 |
| RTSP_BUFFER_FULL | -5 | 缓冲区满 |
| RTSP_PARSE_ERROR | -6 | 协议解析错误 |
| RTSP_NOT_IMPLEMENTED | -7 | 功能未实现 |
| RTSP_TIMEOUT | -8 | 操作超时 |
| RTSP_LIMIT_EXCEEDED | -9 | 超出限制 |
| RTSP_OUT_OF_MEMORY | -10 | 内存分配失败 |
| RTSP_ALREADY_STARTED | -11 | 服务器已启动 |
| RTSP_NOT_STARTED | -12 | 服务器未启动 |
| RTSP_UNSUPPORTED_TRANSPORT | -13 | 不支持的传输协议 |
| RTSP_INTERNAL_ERROR | -14 | 内部错误 |

## 5. RTSP 协议支持

### 5.1 支持的方法

| 方法 | 响应码 |
| :--- | :--- |
| OPTIONS | 200 OK |
| DESCRIBE | 200 OK |
| SETUP | 200 OK / 461 Unsupported Transport |
| PLAY | 200 OK |
| PAUSE | 200 OK |
| TEARDOWN | 200 OK |
| GET_PARAMETER | 200 OK |
| SET_PARAMETER | 200 OK |

### 5.2 Transport 头支持

- **RTP/AVP/TCP**：TCP模式，使用interleaved frame
- **RTP/AVP**：UDP模式，使用独立RTP/RTCP端口

## 6. RTP 透传原则

- **不解析**：不读取RTP头中的任何字段
- **不打包**：不将原始音视频数据封装成RTP包
- **不修改**：不对RTP包内容做任何修改，直接透传
- **不处理**：不涉及编解码、时间戳同步、帧率控制等

## 7. 版本历史

| 版本 | 功能 | 日期 |
| :--- | :--- | :--- |
| V1.0 | 基础功能：RTSP协议处理、RTP透传(TCP)、C API、双线程模型 | 2026-07 |
| V1.1 | 统计信息、超时机制、资源管理优化 | 2026-07 |
| V1.2 | UDP传输支持、错误处理优化、日志级别控制、ReadLine性能优化 | 2026-07 |
