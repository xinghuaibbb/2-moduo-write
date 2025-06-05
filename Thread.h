#pragma once

#include "noncopyable.h"

#include <functional>
#include <thread>
#include <memory>
#include <unistd.h> // For pid_t
#include <string>
#include <atomic>

class Thread : noncopyable
{
public:
    using ThreadFunc = std::function<void()>;

    explicit Thread(ThreadFunc func, const std::string &name = std::string());
    ~Thread();

    void start();
    void join();

    bool started() const {return started_; }
    pid_t tid() const {return tid_;}
    const std::string& name() const {return name_;}
    
    static int numCreated() { return numCreated_.load(); }



private:
    void setDefultName();

    bool started_;
    bool joined_; // 是否等待子线程完成
    // std::thread thread_; // 使用指针来避免在构造函数中初始化std::thread对象
    std::shared_ptr<std::thread> threadPtr_;
    pid_t tid_;
    ThreadFunc func_;
    std::string name_;
    static std::atomic_int numCreated_;

};