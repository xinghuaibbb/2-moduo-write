#pragma once

#include "noncopyable.h"

#include <functional>
#include <string>
#include <vector>
#include <memory>

class EventLoopThread;
class EventLoop;

class EventLoopThreadLoop : noncopyable
{

public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThreadLoop(EventLoop *baseLoop, const std::string &nameage);
    ~EventLoopThreadLoop();


    void setThreadNum(int numThreads) {numThreads_ = numThreads; }

    void start(const ThreadInitCallback &cb = ThreadInitCallback());
    
    // 如果工作在多线程中, baseloop_ 默认以轮询的方式分配channel给subloop
    EventLoop* getNextLoop(); 

    std::vector<EventLoop*> getAllLoops() ;

    bool started() const { return started_; }
    const std::string& name() const { return name_; }


private:
    EventLoop* baseLoop_; // 在使用muduo时, 会先 Eventloop loop; 这就是 这个 baseLoop_
    std::string name_;
    bool started_;
    int numThreads_;
    int next_; // 用于轮询选择下一个EventLoop
    std::vector<std::unique_ptr<EventLoopThread>> threads_; // 存储多个EventLoopThread对象
    std::vector<EventLoop*> loops_; // 存储多个EventLoop对象--通过调用 EventLoopThread::startLoop() 方法获取

};