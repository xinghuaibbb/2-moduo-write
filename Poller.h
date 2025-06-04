#pragma once 

#include "noncopyable.h"
#include "Timestamp.h"

#include <vector>
#include <unordered_map>

class Channel;  // 只使用指针类型
class EventLoop;

//muduo库中多路事件分发器的核心IO复用模块
class Poller : noncopyable
{
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop* Loop);
    virtual ~Poller();

    // 给所有io复用 保留统一的接口
    virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;
    virtual void updateChannel(Channel* channel) = 0;
    virtual void removeChannel(Channel* channel) = 0;

    // 判断当前channel是否在当前Poller中
    virtual bool hasChannel(Channel* channel) const;

    // 相当于 单例模式 的 instance接口
    // EventLoop可以通过该接口获取默认的IO复用的具体实现
    static Poller* newDefaultPoller(EventLoop* loop);



protected:
    // map key->sockfd
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_;  // 管理所有的Channel

private:
    EventLoop* ownerLoop_;  // 所属的EventLoop

};