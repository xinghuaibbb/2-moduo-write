#include "EventLoopThread.h"
#include "Eventloop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(), const std::string& name = std::string())
    : loop_(nullptr)
    , exiting_(false)
    , thread_(std::bind(&EventLoopThread::threadFunc, this), name)
    , mutex_()
    , cond_()
    , callback_(cb)
{

}


EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if(loop_ != nullptr)
    {
        loop_->quit();
        thread_.join(); 
    }
}

EventLoop* EventLoopThread::startLoop()
{
    thread_.start(); // 启动底层新线程, 应该就是 subloop, 新线程会执行下面的threadFunc方法


    // 等待 threadFunc 通知主线程, EventLoop已经创建完成
    // con_d_.wait() 是阻塞的, 直到有线程调用 notify_one() 或 notify_all() 唤醒它
    EventLoop* loop = nullptr;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        while(loop_ == nullptr) // 等待子线程创建 EventLoop 对象
        {
            cond_.wait(lock);
        }
        loop = loop_;   // 做了个线程间通信, 又取出来了
    }

    // 中间变量 存在, 是增加可读性和 调试
    return loop;
}


// 下面这个方法, 是在单独的新线程中 运行的
void EventLoopThread::threadFunc()
{
    EventLoop loop; // 创建一个新的EventLoop对象, 该对象在新线程中运行
    if(callback_)
    {
        callback_(&loop); // 如果有回调函数, 则执行它
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop; // 让主线程的 loop_ 指向子线程的 EventLoop 对象
        cond_.notify_one(); // 通知主线程, EventLoop已经创建完成

    }

    loop.loop(); // 进入事件循环, 开始处理事件=> Poller.poll
    // 一般一直循环

    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr; // 事件循环结束后, 将 loop_ 设置为 nullptr, 表示 EventLoop 对象已经销毁
}