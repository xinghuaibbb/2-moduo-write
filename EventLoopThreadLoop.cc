#include "EventLoopThreadLoop.h"
#include "EventLoopThread.h"

#include <memory>

EventLoopThreadLoop::EventLoopThreadLoop(EventLoop* baseLoop, const std::string& nameage)
    : baseLoop_(baseLoop)
    , name_(nameage)
    , started_(false)
    , numThreads_(0)
    , next_(0)
{

}
EventLoopThreadLoop::~EventLoopThreadLoop()
{
    // loops_ 无需析构, 栈资源
}


void EventLoopThreadLoop::start(const ThreadInitCallback& cb)
{
    started_ = true;
    for (int i = 0; i < numThreads_; ++i)
    {
        char buf[name_.size()+32];
        snprintf(buf, sizeof(buf), "%s%d", name_.c_str(), i);
        EventLoopThread* t = new EventLoopThread(cb, buf);

        threads_.emplace_back(std::unique_ptr<EventLoopThread>(t));
        loops_.emplace_back(t->startLoop());
    }

    // 整个服务端只有一个线程, 运行baseloop
    if(numThreads_ == 0 && cb)
    {
        cb(baseLoop_);
    }
}

// 如果工作在多线程中, baseloop_ 默认以轮询的方式分配channel给subloop
EventLoop* EventLoopThreadLoop::getNextLoop()
{
    EventLoop* loop = baseLoop_;

    if(!loops_.empty())
    {
        loop = loops_[next_];
        ++next_;
        if(next_ >= loops_.size())
        {
            next_ = 0; // 轮询
        }
    }
    return loop;
}


std::vector<EventLoop*> EventLoopThreadLoop::getAllLoops()
{
    if(loops_.empty())
    {
        return std::vector<EventLoop*>(1, baseLoop_);
    }
    else
    {
        return loops_;
    }
}