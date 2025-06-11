#include <hzhmuduo/TcpServer.h>
#include <hzhmuduo/Logger.h>

class EchoServer
{
public:
    EchoServer(EventLoop* loop,
        const InetAddress& listenAddr,
        const std::string& nameArg)
        : loop_(loop)
        , server_(loop, listenAddr, nameArg)
    {
        // 注册回调函数
        server_.setConnectionCallback(std::bind(&EchoServer::onConnection, this, std::placeholders::_1));
        server_.setMessageCallback(std::bind(&EchoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // 设置线程数量
        server_.setThreadNum(4);
    }

    void start()
    {
        server_.start();
    }

private:
    // 新连接建立的回调函数
    void onConnection(const TcpConnectionPtr& conn)
    {
        if (conn->connected())
        {
            LOG_INFO("EchoServer - new connection [%s] from %s", conn->name().c_str(), conn->peerAddress().toIpPort().c_str());
        }
        else
        {
            LOG_INFO("EchoServer - connection [%s] is down", conn->name().c_str());

        }
    }

    // 可读写事件回调 -- 
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime)
    {
        std::string msg = buf->retrieveAllAsString();
        conn->send(msg);
        conn->shutdown();  // 发送完毕后, 关闭连接
    }




    EventLoop* loop_;
    TcpServer server_;
};



int main()
{
    EventLoop loop;
    InetAddress addr(8000);
    EchoServer server(&loop, addr, "EchoServer.01"); // Acceptor non-blocking :   listenfd , create bind
    server.start();  // listen loopthread  listenfd=>acceptChannel => mainloop => Acceptor::listen() => acceptChannel::enableReading() => Poller::updateChannel()

    loop.loop(); // 启动事件循环, 监听新连接的到来  mainloop
    // subloop 的 loop() 由 TcpServer::start() 中的 threadPool_->start(threadInitCallback_) 启动

    return 0;

}
