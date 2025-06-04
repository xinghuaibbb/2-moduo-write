#include "EPOLLPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <unistd.h> // for close
#include <string.h>


namespace
{
    const int kNew = -1;  // 表示新创建的Channel
    const int kAdded = 1;  // 表示已添加到Poller中
    const int kDeleted = 2;  // 表示已从Poller中删除
}

// epoll_create1
EPOLLPoller::EPOLLPoller(EventLoop* loop)
    : Poller(loop),  // 调用基类Poller的构造函数
    epollfd_(::epoll_create1(EPOLL_CLOEXEC))
    , events_(kInitEventListSize)
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("epoll_create1 failed: %d \n", errno);
    }
}

EPOLLPoller::~EPOLLPoller()
{
    ::close(epollfd_);
}

// epoll_ctl add,del,
// channel updata remove --> eventloop update remove --> poller update remove
//特别注意: channel的 fd 一般是 sockfd, 而不是 epollfd!!
/*
*           EventLoop
*     ChannelLists     Poller
*                     ChannelMap <fd, Channel*>
*/

void EPOLLPoller::updateChannel(Channel* channel)
{
    const int index = channel->index();
    LOG_INFO("func=%s ==> fd = %d, events = %d, index = %d", __FUNCTION__, channel->fd(), channel->events(), index);
    if (index == kNew || index == kDeleted)
    {
        if (index == kNew)
        {
            int fd = channel->fd();
            channels_[fd] = channel;  // 将Channel添加到Poller的ChannelMap中
        }

        channel->set_index(kAdded);  // 更新Channel的状态
        update(EPOLL_CTL_ADD, channel); // 添加到epoll中
    }
    else  // 已经存在, 则更新模式
    {
        int fd = channel->fd();
        if (channel->isNoneEvent()) // 如果没有关注的事件, 则从epoll中删除
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);  // 更新Channel的状态
        }
        else  // 有关注的事件, 则更新epoll中的事件
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }

}

void EPOLLPoller::removeChannel(Channel* channel)
{
    int fd = channel->fd();
    channels_.erase(fd);  // 从Poller的ChannelMap中删除
    LOG_INFO("func=%s ==> fd = %d", __FUNCTION__, fd);

    int index = channel->index();
    if (index == kAdded)  // 如果Channel在Poller中
    {
        update(EPOLL_CTL_DEL, channel); // 从epoll中删除
    }
    channel->set_index(kNew);  // 更新Channel的状态
}



// epoll_ctl
// operation: EPOLL_CTL_ADD, EPOLL_CTL_MOD, EPOLL_CTL_DEL
void EPOLLPoller::update(int operation, Channel* channel)
{
    epoll_event event;

    memset(&event, 0, sizeof(event));
    int fd = channel->fd();

    event.events = channel->events();  // 设置关注的事件
    event.data.fd = fd;  // 将文件描述符存储在event.data.fd中----这里实际没啥用,联合体 覆盖了
    event.data.ptr = channel;  // 将Channel指针存储在event.data.ptr中


    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        if (operation == EPOLL_CTL_DEL) // 删除错误可以容忍
        {
            LOG_ERROR("epoll_ctl del error: errno = %d", errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl add/mod error: errno = %d", errno);
        }
    }
}

Timestamp EPOLLPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
    // debug更合适
    LOG_INFO("func=%s ==> fd total count = %lu", __FUNCTION__, channels_.size());

    // 类成员 events_ 是一个epoll_event类型的vector, 用于存储epoll_wait返回的事件
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);

    int saveErrno = errno; // 保存错误码

    Timestamp now(Timestamp::now());

    if (numEvents > 0)
    {
        LOG_INFO("%d events happened \n", numEvents);
        fillActiveChannels(numEvents, activeChannels);  // 填充活跃的Channel列表
        if (numEvents == events_.size())
        {
            events_.resize(events_.size() * 2);  // 扩展事件列表
        }
    }
    else if (numEvents == 0)
    {
        LOG_DEBUG("%s timeout \n", __FUNCTION__);
    }
    else
    {
        if (saveErrno != EINTR)  // 如果不是被信号中断
        {
            errno = saveErrno;
            LOG_ERROR("EPOLLPoller::poll() error! \n");
        }

    }
    return now;  // 返回当前时间戳
}

// 填充活跃的Channel列表
// 在poll()中调用--poll()是监听,epoll_wait(), 把有事件发生的Channel填充到activeChannels中
void EPOLLPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const
{
    for(int i = 0; i < numEvents; ++i)
    {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr); // 获取Channel指针
        channel->set_revents(events_[i].events); // 设置Channel的revents
        activeChannels->push_back(channel); // 将Channel添加到活跃的Channel列表中
        // eventloop 就拿到了它的Poller返回的所有活跃的channel列表
    }
}

