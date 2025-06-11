#include "Socket.h"
#include "Logger.h"
#include "InetAddress.h"


#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/tcp.h>

Socket::~Socket()
{
    close(sockfd_);
}

void Socket::bindAddress(const InetAddress& localaddr)
{
    if (0 != ::bind(sockfd_, (sockaddr*)localaddr.getSockAddr(), sizeof(sockaddr_in)))
    {
        LOG_FATAL("bind socket: %d fail \n", sockfd_);
    }
}

void Socket::listen()
{
    if (0 != ::listen(sockfd_, 1024)) // 设置监听个数
    {
        LOG_FATAL(" listen socket: %d fail \n", sockfd_);
    }
}

int Socket::accept(InetAddress* peeraddr)
{
    /**
     * 问题:
     * 1. accept 传参错误 
     * 2. connd没有设置 非阻塞 
     */
    sockaddr_in addr;
    // socklen_t len; 
    socklen_t len = sizeof(addr); 
    bzero(&addr, sizeof(addr));
    // int connfd = ::accept(sockfd_, (sockaddr*)&addr, &len);
    //设置非阻塞
    int connfd = ::accept4(sockfd_, (sockaddr*)&addr, &len, SOCK_CLOEXEC | SOCK_NONBLOCK);

    if (connfd >= 0)
    {
        peeraddr->setSockAddr(addr); // 设置客户端地址
    }
    return connfd;  

}

void Socket::shutdownWrite()
{
    if (0 != ::shutdown(sockfd_, SHUT_WR))
    {
        LOG_ERROR("shutdown socket: %d fail \n", sockfd_);
    }
}

void Socket::setTcpNoDelay(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

void Socket::setReuseAddr(bool on) // 设置地址重用
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

void Socket::sertReusePort(bool on) // 设置端口重用
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}

void Socket::setKeepAlive(bool on) // 设置长连接
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}