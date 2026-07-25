#include <cstring>

#include "../../include/rtsp_server.h"
#include "../core/rtsp_server.h"
#include "../rtp/rtp_packet.h"

struct RtspServer
{
    rtsp_server::RtspServer* impl;
};

extern "C"
{
RtspServer* rtsp_server_create()
{
    RtspServer* server = new RtspServer();
    if (!server)
    {
        return nullptr;
    }
    server->impl = new rtsp_server::RtspServer();
    if (!server->impl)
    {
        delete server;
        return nullptr;
    }
    return server;
}

void rtsp_server_destroy(RtspServer* server)
{
    if (!server)
    {
        return;
    }
    if (server->impl)
    {
        delete server->impl;
    }
    delete server;
}

int rtsp_server_start(RtspServer* server, const char* ip, int port)
{
    if (!server || !server->impl)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    if (port <= 0 || port > 65535)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    const char* bind_ip = ip ? ip : "0.0.0.0";
    rtsp_server::Status status = server->impl->Start(bind_ip, port);
    if (!status.ok())
    {
        return static_cast<int>(status.code());
    }
    return RTSP_OK;
}

void rtsp_server_stop(RtspServer* server)
{
    if (!server || !server->impl)
    {
        return;
    }
    server->impl->Stop();
}

void rtsp_server_run(RtspServer* server)
{
    if (!server || !server->impl)
    {
        return;
    }
    server->impl->Run();
}

int rtsp_server_send_rtp(RtspServer* server, const uint8_t* data, size_t len, int stream_index)
{
    if (!server || !server->impl || !data || len == 0)
    {
        return RTSP_INVALID_ARGUMENT;
    }

    rtsp_server::RtpPacket packet;
    packet.data = data;
    packet.len = len;
    packet.stream_index = stream_index;

    rtsp_server::Status status = server->impl->BroadcastRtp(packet);
    if (!status.ok())
    {
        return static_cast<int>(status.code());
    }
    return RTSP_OK;
}

int rtsp_server_is_running(RtspServer* server)
{
    if (!server || !server->impl)
    {
        return 0;
    }
    return server->impl->is_running() ? 1 : 0;
}

int rtsp_server_get_port(RtspServer* server)
{
    if (!server || !server->impl)
    {
        return -1;
    }
    return server->impl->port();
}

}  // extern "C"
