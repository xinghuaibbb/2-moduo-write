#include "Poller.h"
#include "Channel.h" // 访问成员,就不能依赖前置声明了


Poller::Poller(EventLoop* Loop)
    : ownerLoop_(Loop)
{}


// 判断当前channel是否在当前Poller中
bool Poller::hasChannel(Channel* channel) const
{
    auto it= channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}

    