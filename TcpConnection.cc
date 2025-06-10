#include "TcpConnection.h"
#include "Logger.h"
#include "Channel.h"
#include "Socket.h"
#include "Eventloop.h"

#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>

static EventLoop* CheckLoopNotNull(EventLoop* loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s%d TcpConnection loop is null!", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}


TcpConnection::TcpConnection(EventLoop* loop,
    const std::string& name,
    int sockfd,
    const InetAddress& localAddress,
    const InetAddress& peerAddress)
    : loop_(CheckLoopNotNull(loop))
    , name_(name)
    , state_(kConnecting)
    , reading_(true)
    , socket_(new Socket(sockfd))
    , channel_(new Channel(loop, sockfd))
    , localAddress_(localAddress)
    , peerAddress_(peerAddress)
    , highWaterMark_(64 * 1024 * 1024) // 默认高水位64MB
{
    // 设置channel的回调函数, poller通知channel有事件发生时, channel会调用这些回调函数
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));

    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));

    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));

    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d", name_.c_str(), sockfd);

    socket_->setKeepAlive(true);

}
TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d", name_.c_str(), socket_->fd(), state_.load());

}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if (n > 0)
    {
        // 已建立连接的用户, 有可读事件发生了, 调用用户传入的回调操作onMessage
        // 就是 使用时的 onMessage
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0)
    {
        handleClose(); // 对端关闭连接
    }
    else
    {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead() error\n");
        handleError(); // 发生错误, 处理错误事件
    }
}


void TcpConnection::handleWrite() // 写事件回调, 发送数据
{
    if (channel_->isWriting())
    {
        int savedErrno = 0;
        // 可以使用 类似 readfd 的函数, 补一下

        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);

        if (n > 0)
        {
            outputBuffer_.retrieve(n); // 重置区域,(包含可读区域是否使用完两种情况) 即 更新 此次发送完的 区域
            if (outputBuffer_.readableBytes() == 0)  // 就是 可读区域读完了,  即 数据全部发送完了
            {
                channel_->disableWriteing(); // 禁止写事件, 因为没有数据了
                if (writeCompleteCallback_)
                {
                    // 唤醒loop对应的thread线程, 执行回调
                    // 其实, 调用这个函数的, 应该就是处理该连接的subloop线程, 这个 应该可以改进--即使用runInLoop
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this())); // 发送完成的回调
                }

                if (state_ == kDisconnecting) // 如果正在断开连接
                {
                    shutdownInLoop();  // 并没有shutdown, 而是去 shutdowninloop
                }
            }
            else
            {
                LOG_ERROR("TcpConnection::handleWrite() \n");
            }
        }
    }
    else
    {
        LOG_ERROR("TcpConnectionfd = %d is down, no more writing\n", channel_->fd());
    }

}

void TcpConnection::handleClose()
{
    LOG_INFO("fd = %d handleClose state = %d\n", channel_->fd(), state_.load());
    setState(kDisconnected); // 设置状态为已断开连接
    channel_->disbleALL(); // 禁用所有事件

    TcpConnectionPtr connPtr(shared_from_this()); // 使用shared_from_this获取TcpConnection的shared_ptr

    connectionCallback_(connPtr); // 执行连接关闭的回调--用户用
    closeCallback_(connPtr); // 关闭连接的回调--系统用
    
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof(optval);
    int err =0;
    if(::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno; // getsockopt 调用失败，错误来源于系统调用
    }
    else
    {
        err = optval; // getsockopt 调用成功，错误来源于套接字本身
    }
    LOG_ERROR("TcpConnection::handleError() - SO_ERROR = %d, errno = %d\n", err, errno);
}



// 发送数据 框架
void TcpConnection::send(const std::string &buf)  // 如果是 Buffer&  就需要在发送完处理缓冲区
{
    if(state_ == kConnected)
    {
        if(loop_->isInLoopThread())
        {
            sendInLoop(buf.c_str(), buf.size());
        }
        else
        {
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));
        }
    }
}

/**
 * 发送数据  应用写的快  而内核发送慢  需要把数据写入缓冲区,  而且设置了高水位回调
 */

// 发送数据 核心
void TcpConnection::sendInLoop(const void* data, int len)
{
    ssize_t nwrote = 0; // 已经发送的字节数
    size_t remaining = len; // 剩余要发送的数据长度
    bool faultError = false; // 是否发生错误

    if(state_ == kDisconnected)
    {
        LOG_ERROR("TcpConnection::sendInLoop() - disconnected, give up writing");
        return;
    }
    
    // 刚开始, 所有事件都是对 读感兴趣
    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        // 表示channel是第一次 写事件, 且Buffer缓冲区没有数据, 则可以直接写入数据
        nwrote = ::write(channel_->fd(), data, len);
        if(nwrote >= 0)
        {
            remaining = len - nwrote; // 更新剩余要发送的数据长度
            if(remaining == 0 && writeCompleteCallback_)
            {
                // 如果发送完了, 且有发送完成的回调, 则唤醒loop对应的线程, 执行回调
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else // nwrote < 0
        {
            nwrote = 0;
            if(errno != EWOULDBLOCK)
            {
                LOG_ERROR("TcpConnection::sendInLoop()");
                if(errno == EPIPE || errno == ECONNRESET) // EPIPE: 对端关闭连接, ECONNRESET: 连接重置
                {
                    faultError = true; // 发生错误
                }

            }
        }
    }

    // 如果没有发生错误, 且还有剩余数据要发送,  需要放入缓冲区中, 然后给channel
    // 然后 注册 epollout事件, poller发现 tcp的发送缓冲区又空间, 会通知相应的 socket->channel, 调用writeCallback_ 回调写事件
    // 先 写 再注册
    if(!faultError && remaining > 0) 
    {
        // 目前发送缓冲区剩余的 待发送的数据长度
        size_t oldLen = outputBuffer_.readableBytes(); // 记录原来缓冲区的可读长度
        if(oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_) // 旧的加上新的超过水位线  并 旧的小于水位线  并且 有高水位回调
        {
            loop_->queueInLoop(
                std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining) // 调用高水位回调
            );
        }

        outputBuffer_.append(static_cast<const char*>(data) + nwrote, remaining); // 把剩余数据添加到输出缓冲区

        if(!channel_->isWriting())
        {
            channel_->enableWriteing();  // 必须注册 写事件, 否则 poller 不会给channel 通知epollout, 数据也就发送不完
        }

    }
}

// 连接建立时调用
void TcpConnection::connectEstablished()
{
    setState(kConnected); // 设置状态为已连接
    channel_->tie(shared_from_this()); // 绑定channel和TcpConnection, 防止channel被手动remove掉, channel还在执行回调
    channel_->enableReading(); // 启用读事件

    connectionCallback_(shared_from_this()); // 执行连接建立的回调--用户用
}


void TcpConnection::connectDestroyed()
{
    if(state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disbleALL();
        connectionCallback_(shared_from_this()); // 执行连接关闭的回调--用户用
    }
    channel_->remove(); // 从事件循环中移除channel
}

// 关闭连接
void TcpConnection::shutdown()
{
    if(state_ == kConnected) // 如果连接是已连接状态
    {
        setState(kDisconnecting); // 设置状态为正在断开连接
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this)); // 在事件循环中执行关闭连接操作
    }
}



void TcpConnection::shutdownInLoop()
{
    if(!channel_->isWriting())  // 说明 outputBuffer_ 中没有数据了, 可以直接关闭连接
    {
        socket_->shutdownWrite(); // 关闭写端
    }
}   

