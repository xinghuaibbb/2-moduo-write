#include "TcpServer.h"
#include "Logger.h"
#include "TcpConnection.h"

#include <string>
#include <string.h>

static EventLoop* CheckLoopNotNull(EventLoop* loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s%d mainloop is null!", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}


TcpServer::TcpServer(EventLoop* loop,
    const InetAddress& listenAddr,
    const std::string& namearg,
    Option option)
    : loop_(CheckLoopNotNull(loop)) // 不能为空
    , ipPort_(listenAddr.toIpPort())
    , name_(namearg)
    , acceptor_(new Acceptor(loop_, listenAddr, option == kReusePort))
    , threadPool_(new EventLoopThreadLoop(loop_, name_))
    , connectionCallback_()
    , messageCallback_()
    , nextConnId_(1)
    , started_(0)
{
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));


}

TcpServer::~TcpServer()
{
    for(auto& item : connections_)
    {
        // 这个局部的 shared_ptr智能指针, 出有括号, 可以自动释放new出来的TcpConnection对象资源
        TcpConnectionPtr conn(item.second);  
        item.second.reset(); // 释放连接

        conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed, conn)); // 在subloop中执行连接销毁的操作

    }
}

void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::start()
{
    if(started_++ == 0) // 防止一个tcpserver对象 被started 多次
    {
        threadPool_->start(threadInitCallback_); // 启动底层的loop线程池---threadInitCallback_ 应该是轮询选择subloop
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));  // 唤醒并执行
    }
}

// 有一个新的连接到来时, 由Acceptor调用, 会分配 sockfd 和 peerAddr
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{
    // 轮训算法,  选择一个 subloop, 来管理 channel
    EventLoop* ioLoop = threadPool_->getNextLoop(); // 轮询选择一个subloop

    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "-%s#%d", ipPort_.c_str(), nextConnId_); // 生成连接的唯一标识符

    ++nextConnId_; // 下一个连接ID

    std::string connName = name_ + buf; 

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s", name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    // 通过 sockfd 获取其绑定的本机的ip地址和端口信息
    sockaddr_in local;
    ::bzero(&local, sizeof(local));
    socklen_t addrlen = sizeof(local);
    if(::getsockname(sockfd, (sockaddr*)&local, &addrlen) < 0)
    {
        LOG_ERROR("TcpServer::newConnection getsockname error");
    }
    
    InetAddress localAddr(local); // 本地地址

    // 根据连接成功的sockfd, 创建TcpConnection连接对象
    TcpConnectionPtr conn(new TcpConnection(loop_, connName, sockfd, localAddr, peerAddr));

    connections_[connName] = conn; // 存储连接到map中

    // 下面的回调都是用户设置给TcpServer ==>  TcpConnection ==> Channel ==> Poller ==>notify Channel回调操作 
    // 这两实际 就是 使用时 自己设置的 onConnection 和 onMessage 函数
    conn->setConnectionCallback(connectionCallback_); // 设置连接建立的回调 
    conn->setMessageCallback(messageCallback_); // 设置消息回调

    conn->setWriteCompleteCallBack(writeCompleteCallback_); // 设置写完成的回调
    conn->setCloseCallBack(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1)); // 设置关闭连接的回调, 由TcpConnection调用

    // 直接调用TcpConnection的connectEstablished方法, 进行连接建立的操作
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn)); // 在subloop中执行连接建立的操作
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn)); // 在主loop中执行删除连接的操作
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s \n", name_.c_str(), conn->name().c_str());

    size_t n = connections_.erase(conn->name()); // 从map中删除连接
    EventLoop* ioLoop = conn->getLoop(); // 获取连接所在的subloop
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn)); // 在subloop中执行连接销毁的操作
}

