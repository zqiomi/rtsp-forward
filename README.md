# RTSP转发

轻量级RTSP转发库，支持一个输入多路转发。

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
#include "rtsp_forward.h"

void* server = NULL;
RtspForwardConfig config = {
    .port = 554,
    .max_sessions = 10,
    .buffer_size = 65536,
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

## 文档

- [RTSP转发需求文档](doc/RTSP转发需求文档.md)
- [RTSP转发实现文档](doc/RTSP转发实现文档.md)
