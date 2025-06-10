#include "Channel.h"
#include "Logger.h"
#include "Eventloop.h"

#include <sys/epoll.h>
#include <sys/poll.h>

const int Channel::kNoneEvent=0; // 无事件
const int Channel::kReadEvent=EPOLLIN | EPOLLPRI; // 可读事件
const int Channel::kWriteEvent=EPOLLOUT; // 可写事件

// EventLoop: Channellist poller
Channel::Channel(EventLoop* loop, int fd)
:loop_(loop),
    fd_(fd),
    events_(0),
    revents_(0),
    index_(-1),
    tied_(false)
{

}

Channel::~Channel()
{

}

// 什么时候调用?-- 绑定Tcpconnection的生命周期
void Channel::tie(const std::shared_ptr<void>& obj)
{
    
    tie_ = obj;  // 将生命周期绑定到一个shared_ptr对象上
    tied_ = true; // 设置绑定标志为true
}

// 更新poller中的事件
// EventLoop: Channellist poller
void Channel::update()
{
    // 通过channel的所属事件循环(eventloop)来更新poller中的事件,调用poller的方法, 注册fd的events事件
    // add ...
    loop_->updateChannel(this);
}

// 在channel所属的eventloop中删除 该channel
void Channel::remove()
{
    // add ...
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
    if(tied_)
    {
        std::shared_ptr<void>guard = tie_.lock(); // 升为强智能指针
        if(guard)
        {
            handleEventWithGuard(receiveTime); // 受保护的事件处理函数
        }
    }
    else
    {
        handleEventWithGuard(receiveTime); // 受保护的事件处理函数
    }
}

void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO("channel handleEvent events: %d\n", revents_);
    if((revents_&EPOLLHUP)&& !(revents_&EPOLLIN))
    {
        if(closeCallback_)
        {
            closeCallback_(); // 关闭事件回调
        }
    }

    if(revents_&EPOLLERR)
    {
        if(errorCallback_)
        {
            errorCallback_();
        }
    }

    if(revents_&(EPOLLIN|EPOLLPRI))
    {
        if(readCallback_)
        {
            readCallback_(receiveTime); // 可读事件回调
        }
    }

    if(revents_&EPOLLOUT)
    {

        if(writeCallback_)
        {
            writeCallback_(); // 可写事件回调
        }
    }
}



