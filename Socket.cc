#include "Socket.h"
#include "Logger.h"
#include "InetAddress.h"


#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/tcp.h>

Scoket::~Scoket()
{
    close(sockfd_);
}

void Scoket::bindAddress(const InetAddress& localaddr)
{
    if (0 != ::bind(sockfd_, (sockaddr*)localaddr.getSockAddr(), sizeof(sockaddr_in)))
    {
        LOG_FATAL("bind socket: %d fail \n", sockfd_);
    }
}

void Scoket::listen()
{
    if (0 != ::listen(sockfd_, 1024))
    {
        LOG_FATAL(" listen socket: %d fail \n", sockfd_);
    }
}

int Scoket::accept(InetAddress* peeraddr)
{
    sockaddr_in addr;
    socklen_t len;
    bzero(&addr, sizeof(addr));
    int connfd = ::accept(sockfd_, (sockaddr*)&addr, &len);
    if (connfd >= 0)
    {
        peeraddr->setSockAddr(addr); // 设置客户端地址
    }
    return connfd;
}

void Scoket::shutdownWrite()
{
    if (0 != ::shutdown(sockfd_, SHUT_WR))
    {
        LOG_ERROR("shutdown socket: %d fail \n", sockfd_);
    }
}

void Scoket::setTcpNoDelay(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

void Scoket::setReuseAddr(bool on) // 设置地址重用
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

void Scoket::sertReusePort(bool on) // 设置端口重用
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}

void Scoket::setKeepAlive(bool on) // 设置长连接
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}