#pragma once

#include "Poller.h"

#include <vector>
#include <sys/epoll.h>

class EPOLLPoller : public Poller
{
public:
    EPOLLPoller(EventLoop* loop);
    ~EPOLLPoller() override;

    // 重写Poller的基类抽象方法
    Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;
    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;


private:
    static const int kInitEventListSize = 16;  // 初始事件列表大小

    // 填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList* actionChannels) const;

    // 更新channel通道--注册或修改事件
    // operation: EPOLL_CTL_ADD, EPOLL_CTL_MOD, EPOLL_CTL_DEL
    void update(int operation, Channel* channel);

    using EventList = std::vector<epoll_event>;

    int epollfd_;  // epoll的文件描述符
    EventList events_;  // 用于存储epoll_wait返回的事件列表
};