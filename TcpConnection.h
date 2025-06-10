#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "CallBack.h"
#include "Buffer.h"


#include <memory>
#include <string>
#include <atomic>

class Channel;
class EventLoop;
class Socket;


/**
 * TcpServer => Acceptor => 有新用户连接, 通过accept函数拿到 connfd
 * 
 * => 封装成TcpConnection对象 设置回调 => Channel => Poller => Channel回调操作
 * 
 * 
 */


class TcpConnection : noncopyable, public std::enable_shared_from_this<TcpConnection>
{
private:
     TcpConnection(EventLoop *loop,
        const std::string &name,
        int sockfd,
        const InetAddress &localAddress,
        const InetAddress &peerAddress);
    ~TcpConnection();

    EventLoop *getLoop() const { return loop_; }
    const std::string &name() const {return name_;}
    const InetAddress &localAddress() const {return localAddress_;}
    const InetAddress &peerAddress() const {return peerAddress_;}

    bool connected() const { return state_ == kConnected; }

    // 发送数据
    // void send(const void *message, int len);
    void send(const std::string &buf);

    // 关闭连接
    void shutdown();
    

    void setConnectionCallback(const ConnectionCallBack& cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallBack& cb) { messageCallback_ = cb; }
    void setWriteCompleteCallBack(const WriteCompleteCallBack& cb) { writeCompleteCallback_ = cb; }
    void setHighWaterMarkCallBack(const HighWaterMarkCallBack& cb, size_t highWaterMark)
    {
        highWaterMarkCallback_ = cb;
        highWaterMark_ = highWaterMark;
    }

    void setCloseCallBack(const CloseCallBack& cb) { closeCallback_ = cb; }

    
    void connectEstablished(); // 连接建立时调用
    void connectDestroyed(); // 连接销毁时调用
    



public:
    enum StateE
    {
        kConnecting, // 正在连接
        kConnected,  // 已经连接
        kDisconnecting, // 正在断开连接
        kDisconnected // 已经断开连接
    };


    void handleRead(Timestamp receiveTime); // 处理读事件
    void handleWrite(); // 处理写事件
    void handleClose(); // 处理关闭事件
    void handleError(); // 处理错误事件

    void sendInLoop(const void *data, int len); // 在事件循环中发送数据
    void shutdownInLoop(); // 在事件循环中关闭连接

    void setState(StateE state) { state_.store(state); } // 设置连接状态


    EventLoop *loop_; // 这里绝对不是baseloop, 因为TcpConnection都是在subloop里面管理的
    
    const std::string name_; // 连接的名字
    std::atomic_int state_; // 连接的状态, 连接中, 已连接, 断开中, 已断开
    bool reading_; // 是否正在读取数据


    // 连接的socket和channel
    // 类似于TcpServer中的acceptor和channel
    // Acceptor是监听连接的-->mainloop, 而TcpConnection是处理已连接的-->subloop
    std::unique_ptr<Socket> socket_; 
    std::unique_ptr<Channel> channel_; 

    const InetAddress localAddress_; // 本地地址
    const InetAddress peerAddress_; // 对端地址 客户端

    // 回调函数组
    ConnectionCallBack connectionCallback_;  // 这个回调在连接建立或关闭时被调用，通知上层用户连接的状态发生了变化。
    MessageCallBack messageCallback_;        // 有读写消息时的回调
    WriteCompleteCallBack writeCompleteCallback_; // 消息发送完成的回调
    HighWaterMarkCallBack highWaterMarkCallback_; // 高水位回调
    CloseCallBack closeCallback_; // 连接关闭的回调
   
    size_t highWaterMark_; // 高水位标记, 当发送的数据超过这个值时, 会触发highWaterMarkCallback_


    // 缓冲区组
    Buffer inputBuffer_; // 输入缓冲区, 用于存储接收到的数据
    Buffer outputBuffer_; // 输出缓冲区, 用于存储待发送的数据
};


