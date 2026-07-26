# RTSP转发

轻量级RTSP转发库，支持一个输入多路转发。

## 功能特性

- RTSP协议标准方法支持（OPTIONS、DESCRIBE、SETUP、PLAY、PAUSE、TEARDOWN）
- RTP包透传（TCP模式）
- 一个输入，多路转发
- 纯C接口，兼容C/C++调用
- 双线程模型（V1.0）
- 运行统计信息查询（V1.1）
- 连接/会话空闲超时自动清理（V1.1）

## 编译

```bash
mkdir -p build && cd build
cmake ..
make
```

## 快速测试

运行 demo（默认 RTSP 端口 8554，RTP 接收端口 5004）：

```bash
./build/rtsp_forward_demo
# 也可指定端口：./build/rtsp_forward_demo [rtsp_port] [rtp_port]
```

**1. ffmpeg 推流**（另开终端，SDP 已内置为 H264/90000）：

```bash
# 测试图源
ffmpeg -re -f lavfi -i testsrc=size=640x480:rate=30 \
       -c:v libx264 -g 30 -f rtp rtp://127.0.0.1:5004

# 或推送视频文件
ffmpeg -re -i input.mp4 -an -c:v libx264 -g 30 \
       -f rtp rtp://127.0.0.1:5004
```

**2. VLC 拉流**：

打开网络流 `rtsp://localhost:8554`，或命令行：

```bash
vlc rtsp://localhost:8554
```

传输模式为 TCP interleaved，VLC 默认即支持。Ctrl+C 停止 demo 时会打印会话统计信息。

## 使用

```c
#include "rtsp_forward.h"

void* server = NULL;
RtspForwardConfig config = {
    .port = 554,
    .max_sessions = 10,
    .buffer_size = 65536,
    .connection_timeout_sec = 30,
    .session_timeout_sec = 60,
};

// 创建服务器
rtsp_forward_create(&server, &config);

// 设置SDP
rtsp_forward_set_sdp(server, "v=0\r\n...");

// 启动
rtsp_forward_start(server);
rtsp_forward_run(server);
```

## API 接口

| 函数 | 功能 |
| :--- | :--- |
| `rtsp_forward_create` | 创建服务器 |
| `rtsp_forward_destroy` | 销毁服务器 |
| `rtsp_forward_start` | 启动服务器 |
| `rtsp_forward_stop` | 停止服务器 |
| `rtsp_forward_run` | 运行事件循环 |
| `rtsp_forward_send_rtp` | 发送RTP包 |
| `rtsp_forward_set_sdp` | 设置SDP内容 |
| `rtsp_forward_get_info` | 获取服务器信息（配置、状态、统计） |

## 文档

- [RTSP转发需求文档](doc/RTSP转发需求文档.md)
- [RTSP转发实现文档](doc/RTSP转发实现文档.md)
