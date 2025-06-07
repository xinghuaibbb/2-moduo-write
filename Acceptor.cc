#include "Acceptor.h"
#include "Logger.h"

#include <sys/socket.h>
#include <unistd.h>

static int createNonblockingSocket() 
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if(sockfd < 0)
    {
        LOG_FATAL("%s:%s:%d listen fd create error: %d \n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop)
    , acceptSocket_(createNonblockingSocket())
    , acceptChannel_(loop, acceptSocket_.fd())
    , listenning_(false)
    
{
    acceptSocket_.setReuseAddr(true); // 设置地址重用
    acceptSocket_.sertReusePort(reuseport); // 设置端口重用
    acceptSocket_.bindAddress(listenAddr); // 绑定监听地址
    // tcpserver.start() --> acceptor.listen()  有新连接时, 执行一个回调, 把connd 封装成 channel--> subloop
    //channel 绑定的读回调 就是  acceptor.handleRead()
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this)); // 设置读事件回调
}

Acceptor::~Acceptor()
{
    acceptChannel_.disbleALL();
    acceptChannel_.remove(); // 从poller中移除
}


void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen(); // listenfd 
    acceptChannel_.enableReading(); // 启用读事件监听
}


// listen fd 的 channel 监听到有新连接到来, 就会调用acceptor.handleRead()方法
void Acceptor::handleRead()
{
    InetAddress peeraddr; // 用于存储新连接的地址信息
    int connfd = acceptSocket_.accept(&peeraddr); // 封装好的连接函数,接受新连接

    if(connfd >= 0)
    {
        if(newConnectionCallback_)
        {
            newConnectionCallback_(connfd, peeraddr);
        }
        else
        {
            ::close(connfd); // 如果没有设置新连接回调, 则关闭连接
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept error: %d \n", __FILE__, __FUNCTION__, __LINE__, errno);
        if(errno == EMFILE)
        {
            LOG_ERROR("%s:%s:%d accept sockfd reached limit \n", __FILE__, __FUNCTION__, __LINE__);
        }
    }
}
