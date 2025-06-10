#include "TcpServer.h"
#include "Logger.h"

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
{
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));


}

TcpServer::~TcpServer()
{

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
