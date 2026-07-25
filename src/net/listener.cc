#include "listener.h"

#include "../util/log.h"

namespace rtsp_server
{

Listener::Listener() {}

Listener::~Listener()
{
    Close();
}

Status Listener::Listen(const char* addr, int port)
{
    Status status = socket_.Create(AF_INET, SOCK_STREAM, 0);
    if (!status.ok())
    {
        return status;
    }

    status = socket_.SetReuseAddr(true);
    if (!status.ok())
    {
        return status;
    }

    status = socket_.SetNonBlocking(true);
    if (!status.ok())
    {
        return status;
    }

    status = socket_.SetNoSigPipe(true);
    if (!status.ok())
    {
        return status;
    }

    status = socket_.Bind(addr, port);
    if (!status.ok())
    {
        return status;
    }

    status = socket_.Listen(128);
    if (!status.ok())
    {
        return status;
    }

    LOG_INFO("Listener started on %s:%d", addr, port);
    return Status::Ok();
}

int Listener::Accept(struct sockaddr_in* client_addr)
{
    return socket_.Accept(client_addr);
}

int Listener::Accept()
{
    return socket_.Accept(nullptr);
}

void Listener::Close()
{
    socket_.Close();
}

}  // namespace rtsp_server
