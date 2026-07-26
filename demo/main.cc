#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <thread>

#include "rtsp_forward.h"

static void* g_server = NULL;
static std::atomic<int> g_running{1};

void sigint_handler(int sig)
{
    (void)sig;
    printf("\nReceived SIGINT, stopping...\n");
    g_running = 0;
    if (g_server)
    {
        RtspForwardInfo info;
        if (rtsp_forward_get_info(g_server, &info) == RTSP_OK)
        {
            printf("=== Server Info ===\n");
            printf("  Port:               %d\n", info.port);
            printf("  Running:            %d\n", info.running);
            printf("  Uptime:             %llu seconds\n", (unsigned long long)info.uptime_sec);
            printf("  Total connections:  %llu\n", (unsigned long long)info.total_connections);
            printf("  Active sessions:    %d\n", info.active_sessions);
            printf("  Playing sessions:   %d\n", info.playing_sessions);
            printf("  Timed out sessions: %llu\n", (unsigned long long)info.timed_out_sessions);
        }
        rtsp_forward_stop(g_server);
    }
}

/**
 * 流线程：从 UDP 端口接收 ffmpeg 发来的 RTP 包，转发给所有 RTSP 客户端。
 * 这体现了双线程模型：主线程跑 RTSP 事件循环，流线程调用 rtsp_forward_send_rtp。
 */
void stream_thread(int rtp_port)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("stream_thread: socket");
        return;
    }

    // 设置接收超时，以便周期性检查 g_running 标志
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(rtp_port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        perror("stream_thread: bind");
        close(sock);
        return;
    }

    printf("[stream] Listening for RTP on UDP port %d\n", rtp_port);

    uint8_t buf[65536];
    while (g_running)
    {
        ssize_t len = recv(sock, buf, sizeof(buf), 0);
        if (len > 0)
        {
            rtsp_forward_send_rtp(g_server, buf, (size_t)len, 0);
        }
    }

    close(sock);
    printf("[stream] Exited\n");
}

int main(int argc, char* argv[])
{
    int port = 8554;
    int rtp_port = 5004;

    if (argc > 1)
    {
        port = atoi(argv[1]);
    }
    if (argc > 2)
    {
        rtp_port = atoi(argv[2]);
    }

    printf("  RTSP Forward Lib %s\n", rtsp_forward_version_string());
    printf("  RTSP port: %d  (for VLC/clients)\n", port);
    printf("  RTP  port: %d  (for ffmpeg input)\n", rtp_port);
    printf("\n");

    // 创建服务器
    RtspForwardConfig config = {
        .port = port,
        .ip = "0.0.0.0",
        .max_sessions = 2,
        .buffer_size = 65536,
        .sdp_content = NULL,
        .connection_timeout_sec = 30,
        .session_timeout_sec = 60,
    };

    int ret = rtsp_forward_create(&g_server, &config);
    if (ret != RTSP_OK)
    {
        printf("Failed to create server: %d\n", ret);
        return -1;
    }

    // SDP：H264/90000，匹配 ffmpeg libx264 RTP 输出
    const char* sdp =
        "v=0\r\n"
        "o=- 0 0 IN IP4 0.0.0.0\r\n"
        "s=RTSP Stream\r\n"
        "c=IN IP4 0.0.0.0\r\n"
        "t=0 0\r\n"
        "m=video 0 RTP/AVP 96\r\n"
        "a=rtpmap:96 H264/90000\r\n"
        "a=fmtp:96 packetization-mode=1\r\n";
    ret = rtsp_forward_set_sdp(g_server, sdp);
    if (ret != RTSP_OK)
    {
        printf("Failed to set SDP: %d\n", ret);
        rtsp_forward_destroy(g_server);
        return -1;
    }

    ret = rtsp_forward_start(g_server);
    if (ret != RTSP_OK)
    {
        printf("Failed to start server: %d\n", ret);
        rtsp_forward_destroy(g_server);
        return -1;
    }

    // 启动流线程（接收 ffmpeg RTP，转发给客户端）
    std::thread stream_tid(stream_thread, rtp_port);

    printf("========================================\n");
    printf("Server ready. Test steps:\n\n");
    printf("  1. Feed RTP with ffmpeg (test pattern):\n");
    printf("     ffmpeg -re -f lavfi -i testsrc=size=640x480:rate=30 \\\n");
    printf("            -c:v libx264 -g 30 -f rtp rtp://127.0.0.1:%d\n\n", rtp_port);
    printf("     Or with a video file:\n");
    printf("     ffmpeg -re -i input.mp4 -an -c:v libx264 -g 30 \\\n");
    printf("            -f rtp rtp://127.0.0.1:%d\n\n", rtp_port);
    printf("  2. Open in VLC: rtsp://localhost:%d\n\n", port);
    printf("========================================\n");
    printf("Running... Press Ctrl+C to stop.\n");

    signal(SIGINT, sigint_handler);

    // 主线程运行 RTSP 事件循环（阻塞）
    ret = rtsp_forward_run(g_server);

    g_running = 0;
    stream_tid.join();

    printf("Server stopped\n");
    rtsp_forward_destroy(g_server);

    return 0;
}
