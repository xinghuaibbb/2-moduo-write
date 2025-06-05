#include "Eventloop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <mutex>

// 线程局部存储, 每个线程都有一个EventLoop指针
// 防止一个线程中创建多个EventLoop对象
__thread EventLoop* t_loopInThisThread = nullptr;

const int kPollTimeMs = 10000; // 默认的poller IO多路复用的超时时间

// 创建一个eventfd, 用于唤醒subloop处理channel
int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC); // 创建一个非阻塞的事件文件描述符

    if (evtfd < 0)
    {
        LOG_FATAL("eventfd error: %d \n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFuntors_(false)
    , threadId_(CurrentThread::tid())
    , Poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventfd())
    , wakeupChannel_(std::make_unique<Channel>(this, wakeupFd_))  // c++14  c++11 就是new Channel(this, wakeupFd_)
    , currentActiveChannel_(nullptr)

{
    LOG_DEBUG("EventLoop created %p in thread %d\n", this, threadId_);
    if (t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d\n", t_loopInThisThread, threadId_);
    }
    else
    {
        t_loopInThisThread = this; // 设置当前线程的EventLoop指针
    }

    // 设置wakeupChannel_的事件回调函数
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));

    // 每一个eventloop都将监听wakeupchannel的EPOLLIN事件
    wakeupChannel_->enableReading();

    // main可以通过写事件唤醒subloop
}

EventLoop::~EventLoop()
{
    wakeupChannel_->disbleALL(); // 禁用所有事件
    wakeupChannel_->remove(); // 从Poller中移除
    ::close(wakeupFd_); // 关闭wakeupFd_
    t_loopInThisThread = nullptr; // 清除当前线程的EventLoop指针
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one))
    {
        LOG_ERROR("EventLoop::handleRead() reads %ld bytes instead of 8\n", n);
    }

}

void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping\n", this);

    while (!quit_)
    {
        activeChannels_.clear(); // 清空活跃的channel列表
        // 监听两种fd : client fd 和 wakeupFd_
        pollReturnTime_ = Poller_->poll(kPollTimeMs, &activeChannels_); // 调用Poller的poll方法获取活跃的channel, 并拿到发生时间

        for (Channel* channel : activeChannels_)
        {
            // Poller监听那些channel的事件发生, 然后上报给Eventloop, 通知channel处理事件
            channel->handleEvent(pollReturnTime_);
        }

        // 执行当前EventLoop需要处理的`其他`回调操作
        /*
        * IO线程的 mainloop ->accept()--> sockfd
        * channel封装sockfd  给到 subloop
        * mainloop 事先注册了一个回调cb(需要subloop执行)
        * wakeup sunloop后, 执行下面的方法, 执行 之前mainloop注册的回调cb
        */

        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping\n", this);
    looping_ = false; // 设置为非循环状态
}

// 退出loop
// 1. loop在自己的线程中执行, 直接设置quit_为true, 退出循环
// 2. 在非loop线程中执行, 需要唤醒loop所在的线程, 执行quit操作
void EventLoop::quit()
{
    quit_ = true; // 设置退出标志

    if (!isInLoopThread())
    {
        wakeup(); // 如果不是在loop线程中, 需要唤醒loop所在的线程
    }

}

// 执行当前loop需要处理的回调操作
void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread())
    {
        cb();
    }
    else
    {
        queueInLoop(std::move(cb)); // 如果不在loop线程中, 则将cb放入到队列中, 等待唤醒
    }
}

void EventLoop::queueInLoop(Functor cb)
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        pendingFunctors_.emplace_back(std::move(cb)); // 将cb添加到待处理的回调队列中
    }

    // 唤醒相应的, 需要执行上面回调操作的的loop线程
    // || callingPendingFuntors_ 是 当前loop正在处理回调操作, 按时loop又有了新的回调操作, 上面loop()执行完doPendingFunctors()后,又睡着了, 需要再次唤醒loop线程, 执行新的回调操作
    if (!isInLoopThread() || callingPendingFuntors_)  
    {
        wakeup(); // 唤醒loop所在的线程, 执行cb
    }
}

// eventfd，必须用 uint64_t, 不能使用char或者别的
void EventLoop::wakeup()
{
    uint64_t one = 1;
    int n = write(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one))
    {
        LOG_ERROR("EventLoop::wakeup() writes %d bytes instead of 8\n", n);
    }

}

// Eventloop  => Poller => channel
void EventLoop::updateChannel(Channel* channel)
{
    Poller_->updateChannel(channel); // 调用Poller的updateChannel方法更新channel

}
void EventLoop::removeChannel(Channel* channel)
{
    Poller_->removeChannel(channel); // 调用Poller的removeChannel方法移除channel
}
bool EventLoop::hasChannel(Channel* channel)
{
    return Poller_->hasChannel(channel); // 调用Poller的hasChannel方法判断channel是否存在
}


// 重点!!!
void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFuntors_ = true; // 标记正在处理回调操作
    {
        std::lock_guard<std::mutex> lock(mutex);
        functors.swap(pendingFunctors_); // 交换functors和pendingFunctors_
    }

    for( const Functor& functor : functors)
    {
        functor(); // 执行当前loop需要处理的回调操作
    }

    callingPendingFuntors_ = false; // 标记处理完毕
}
