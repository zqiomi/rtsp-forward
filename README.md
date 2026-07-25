# RTSP Server

轻量级RTSP Server库，支持一个输入多路转发。

## 功能特性

- RTSP协议标准方法支持（OPTIONS、DESCRIBE、SETUP、PLAY、PAUSE、TEARDOWN）
- RTP包透传（TCP模式）
- 一个输入，多路转发
- 纯C接口，兼容C/C++调用
- 单线程模型（V1.0）

## 编译

```bash
mkdir -p build && cd build
cmake ..
make
```

## 使用

```c
#include "rtsp_server.h"

void* server = NULL;
RtspServerConfig config = {
    .port = 554,
    .max_sessions = 10,
    .buffer_size = 65536,
};

// 创建服务器
rtsp_server_create(&server, &config);

// 设置SDP
rtsp_server_set_sdp(server, "v=0\r\n...");

// 启动
rtsp_server_start(server);
rtsp_server_run(server);
```

## API 接口

| 函数 | 功能 |
| :--- | :--- |
| `rtsp_server_create` | 创建服务器 |
| `rtsp_server_destroy` | 销毁服务器 |
| `rtsp_server_start` | 启动服务器 |
| `rtsp_server_stop` | 停止服务器 |
| `rtsp_server_run` | 运行事件循环 |
| `rtsp_server_send_rtp` | 发送RTP包 |
| `rtsp_server_set_sdp` | 设置SDP内容 |

## 文档

- [RTSP服务器需求文档](doc/RTSP服务器需求文档.md)
- [RTSP服务器实现文档](doc/RTSP服务器实现文档.md)
