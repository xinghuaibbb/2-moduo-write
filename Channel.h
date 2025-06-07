#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <functional>
#include <memory>

class EventLoop;


/*
`Channel` 是事件通道，封装了文件描述符（`fd`）及其感兴趣的事件, 如EPOLLIN、EPOLLOUT等。
还绑定了poller返回的具体事件
*/
class Channel : noncopyable
{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    // 设置事件回调
    void setReadCallback(ReadEventCallback cb)
    {
        readCallback_ = std::move(cb);
    }

    void setWriteCallback(EventCallback cb)
    {
        writeCallback_ = std::move(cb);
    }

    void setCloseCallback(EventCallback cb)
    {
        closeCallback_ = std::move(cb);
    }
    void setErrorCallback(EventCallback cb)
    {
        errorCallback_ = std::move(cb);
    }

    // fd得到poller通知后, 处理事件
    void handleEvent(Timestamp receiveTime);

    // 防止当channel被手动remove掉, channel还在执行回调
    void tie(const std::shared_ptr<void>&);
    
    int fd() const { return fd_; }
    int events() const { return events_; }
    void set_revents(int revt){ revents_ = revt;}

    // 设置关注的事件
    void enableReading() {events_ |= kReadEvent; update();}
    void disableReading() {events_ &= ~kReadEvent; update();}
    void enableWriteing() {events_ |= kWriteEvent; update();}
    void disableWriteing() {events_ &= ~kWriteEvent; update();}
    void disbleALL() {events_ = kNoneEvent; update();}

    bool isNoneEvent() const {return events_ == kNoneEvent;}
    bool isWriting() const {return events_ & kWriteEvent;}
    bool isReading() const {return events_ & kReadEvent;}

    int index() const {return index_;}
    void set_index(int idx) {index_ = idx; }
    
    // one loop per thread
    EventLoop* owenrLoop() { return loop_; } // 获取所属的事件循环,  每个channel只属于一个事件循环

    void remove();

private:
    void update(); // 更新poller中的事件
    void handleEventWithGuard(Timestamp receiveTime); // 受保护的事件处理函数

    static const int kNoneEvent; // 无事件
    static const int kReadEvent; // 可读事件
    static const int kWriteEvent; // 可写事件

    EventLoop* loop_; // 事件循环
    const int fd_; // 文件描述符
    int events_; // 关注的事件
    int revents_; // poller返回的具体发生的事件
    int index_; // 在poller中的索引

    std::weak_ptr<void> tie_; // 用于绑定生命周期
    bool tied_; // 是否绑定了生命周期
    // bool eventHandling_; // 是否正在处理事件

    // 因为channel通道里面能获知fd最终发生的具体的事件revent_, 所以他负责调用具体事件的回调操作
    ReadEventCallback readCallback_; // 可读事件回调
    EventCallback writeCallback_; // 可写事件回调
    EventCallback closeCallback_; // 关闭事件回调
    EventCallback errorCallback_; // 错误事件回调

};