#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "rtsp_server.h"

static void* g_server = NULL;

void sigint_handler(int sig)
{
    (void)sig;
    printf("Received SIGINT, stopping server...\n");
    if (g_server)
    {
        rtsp_server_stop(g_server);
    }
}

int main(int argc, char* argv[])
{
    int port = 8554;
    int ret;

    if (argc > 1)
    {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535)
        {
            printf("Invalid port: %d\n", port);
            return -1;
        }
    }

    printf("RTSP Server Demo\n");
    printf("Creating server...\n");

    // 使用配置结构体创建服务器
    RtspServerConfig config = {
        .port = port,
        .ip = "0.0.0.0",
        .max_sessions = 10,
        .buffer_size = 65536,
        .sdp_content = NULL  // 后续通过 rtsp_server_set_sdp 设置
    };

    ret = rtsp_server_create(&g_server, &config);
    if (ret != RTSP_OK)
    {
        printf("Failed to create server: %d\n", ret);
        return -1;
    }

    // 设置 SDP 内容
    const char* sdp =
        "v=0\r\n"
        "o=- 0 0 IN IP4 0.0.0.0\r\n"
        "s=RTSP Stream\r\n"
        "c=IN IP4 0.0.0.0\r\n"
        "t=0 0\r\n"
        "m=video 0 RTP/AVP 96\r\n"
        "a=rtpmap:96 H264/90000\r\n";
    ret = rtsp_server_set_sdp(g_server, sdp);
    if (ret != RTSP_OK)
    {
        printf("Failed to set SDP: %d\n", ret);
        rtsp_server_destroy(g_server);
        return -1;
    }

    printf("Starting server on port %d...\n", port);
    ret = rtsp_server_start(g_server);
    if (ret != RTSP_OK)
    {
        printf("Failed to start server: %d\n", ret);
        rtsp_server_destroy(g_server);
        return -1;
    }

    printf("Server started successfully\n");
    printf("Use VLC or other RTSP client to connect to: rtsp://localhost:%d\n", port);

    // Register signal handler for graceful shutdown
    signal(SIGINT, sigint_handler);

    printf("Running event loop...\n");
    ret = rtsp_server_run(g_server);
    if (ret != RTSP_OK)
    {
        printf("Event loop exited with error: %d\n", ret);
    }

    printf("Server stopped\n");
    rtsp_server_destroy(g_server);

    return 0;
}
