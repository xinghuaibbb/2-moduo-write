#pragma once

#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"


class InetAddress;
class EventLoop;

class Acceptor : noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;

    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback &cb) 
    { 
        newConnectionCallback_ = std::move(cb);   // 推荐 std::move
    }

    bool listenning() const { return listenning_; }

    void listen();
    void handleRead(); // 处理新连接的读事件

private:
    EventLoop *loop_;
    Scoket acceptSocket_; 
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;

};