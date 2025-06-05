#pragma once

#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"

#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

class Channel;
class Poller;

// 事件循环类 包括 Poller（事件分发器）和多个 Channel（事件通道）两大类
class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()> ;

    EventLoop();
    ~EventLoop();
    
    void loop();  // 使用的时候, .loop() 进入事件循环
    void quit();

    Timestamp pollReturnTime() const {return pollReturnTime_;}

    void runInLoop(Functor cb); // 在当前loop中执行cb
    void queueInLoop(Functor cb); // 把cb放入到队列中, 唤醒loop所在的线程, 执行cb

    void wakeup(); // 唤醒loop所在的线程

    // Eventloop  => Poller
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    bool isInLoopThread() const {return threadId_ == CurrentThread::tid();} // 判断当前事件循环是否在创建他的线程中执行, 如果在, 说明可以直接进行回调操作, 如果不在, 就要进入 queueInLoop等待其所属线程唤醒
    


private:
    void handleRead(); // wakedup_的回调函数, 唤醒subloop
    void doPendingFunctors(); // 执行当前loop需要处理的回调操作

    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_; // 是否正在事件循环中
    std::atomic_bool quit_;     // 标志退出loop循环
    
    const pid_t threadId_;  // 事件循环所属的线程ID
    Timestamp pollReturnTime_; // poll返回 活跃channel的时间点
    std::unique_ptr<Poller> Poller_; // 事件分发器

    int wakeupFd_; // 主要作用, 当mainloop 获取一个新用户的channel, 通过轮询算法选择一个subloop, 通过该成员唤醒subloop处理

    std::unique_ptr<Channel> wakeupChannel_; // 应该封装 wakeupFd_的Channel, 因为 poller 不直接操作fd,而是操作channel, 是返回activechannel

    ChannelList activeChannels_; // 活跃的channel列表
    Channel* currentActiveChannel_; // 当前活跃的channel

    std::atomic_bool callingPendingFuntors_; // 标识当前loop 是否有需要执行的回调函数
    std::vector<Functor> pendingFunctors_;  // 存储loop需要执行的所有回调操作
    std::mutex mutex;  // 保护 pendingFunctors_ 的互斥锁  线程安全

};