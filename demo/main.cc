#include "../include/rtsp_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

static RtspServer* g_server = nullptr;

void sigint_handler(int sig) {
    (void)sig;
    printf("Received SIGINT, stopping server...\n");
    if (g_server) {
        rtsp_server_stop(g_server);
    }
}

int main(int argc, char* argv[]) {
    int port = 8554;
    
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            printf("Invalid port: %d\n", port);
            return -1;
        }
    }

    printf("RTSP Server Demo\n");
    printf("Creating server...\n");
    
    g_server = rtsp_server_create();
    if (!g_server) {
        printf("Failed to create server\n");
        return -1;
    }

    printf("Starting server on port %d...\n", port);
    int ret = rtsp_server_start(g_server, nullptr, port);
    if (ret != RTSP_OK) {
        printf("Failed to start server: %d\n", ret);
        rtsp_server_destroy(g_server);
        return -1;
    }

    printf("Server started successfully\n");
    printf("Use VLC or other RTSP client to connect to: rtsp://localhost:%d\n", port);

    // Register signal handler for graceful shutdown
    signal(SIGINT, sigint_handler);

    printf("Running event loop...\n");
    rtsp_server_run(g_server);

    printf("Server stopped\n");
    rtsp_server_destroy(g_server);
    
    return 0;
}
