#pragma once

/**
 * 用户使用
 */

#include "Eventloop.h"
#include "EventLoopThreadLoop.h"
#include "Acceptor.h"
 // #include "Channel.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "CallBack.h"

#include <functional>
#include <string>
#include <atomic>
#include <unordered_map>  // 无需排序, 不使用 红黑树 排序map

class TcpServer : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    enum Option
    {
        kNoReusePort,
        kReusePort
    };

    TcpServer(EventLoop* loop,
        const InetAddress& listenAddr,
        const std::string& namearg,
        Option option = kNoReusePort);
    ~TcpServer();

    // 各种查询接口 先不实现

    void setThreadInitCallback(const ThreadInitCallback& cb) { threadInitCallback_ = cb; }

    void setConnectionCallback(const ConnectionCallBack& cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallBack& cb) { messageCallback_ = cb; }
    void setWriteCompleteCallBack(const WriteCompleteCallBack& cb) { writeCompleteCallback_ = cb; }

    void setThreadNum(int numThreads); // 设置底层subloop个数

    void start(); // 开启服务器监听

private:
    void newConnection(int sockfd, const InetAddress& peerAddress); // 新连接到来时的回调, 由acceptor调用
    void removeConnection(int sockfd, const InetAddress& peerAddress);
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    EventLoop* loop_; // base loop, 通常是mainloop
    const std::string ipPort_;
    const std::string name_;
    std::unique_ptr<Acceptor> acceptor_; // 运行在mainloop, 监听新连接事件
    std::shared_ptr<EventLoopThreadLoop> threadPool_; // one loop per thread

    ConnectionCallBack connectionCallback_;  // 有新连接时的回调
    MessageCallBack messageCallback_;        // 有读写消息时的回调
    WriteCompleteCallBack writeCompleteCallback_; // 消息发送完成的回调

    ThreadInitCallback threadInitCallback_; // 线程初始化的回调
    std::atomic_int started_;

    int nextConnId_; // 下一个连接的ID
    ConnectionMap connections_; // 存储所有连接的map

};