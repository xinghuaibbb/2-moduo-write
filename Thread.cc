#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

std::atomic_int Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string& name )
    : started_(false),
    joined_(false),
    tid_(0),
    func_(std::move(func)),
    name_(name)
{
    setDefultName();
}

Thread::~Thread()
{
    // 要么是工作线程，要么是分离线程
    // started_ 防止其子线程变为孤儿线程
    if (started_ && !joined_)  // 并不是工作线程
    {
        threadPtr_->detach(); // 分离线程
    }
}

// 一个thread对象, 记录的 就是一个新线程的详细信息(线程共享一些资源, 例如tid, name等)
void Thread::start() 
{
    started_ = true;
    sem_t sem; // 信号量, 用于线程间同步
    sem_init(&sem, false, 0); // 初始化信号量
    
    //  threadPtr_ = std::shared_ptr<std::thread>(new std::thread([&]()->void
    //     {
    //         tid_ = CurrentThread::tid(); // 获取当前线程的ID
    //         sem_post(&sem); // 线程函数开始执行, 发送信号量
    //         func_(); // 开启一个新线程, 专门执行该线程函数
    //     }
    // ));

    threadPtr_ = std::make_shared<std::thread>([&]()->void
        {
            tid_ = CurrentThread::tid(); // 获取当前线程的ID
            sem_post(&sem); // 线程函数开始执行, 发送信号量
            func_(); // 开启一个新线程, 专门执行该线程函数
        });
    
    // 必须等待获取 新线程的 tid值---- 线程间, 使用信号量
    sem_wait(&sem);  // 0阻塞

}

void Thread::join()
{
    joined_ = true; 
    threadPtr_->join();
}


void Thread::setDefultName()
{
    int num = ++numCreated_;
    if (name_.empty())
    {
        char buf[32] = { 0 };
        snprintf(buf, sizeof(buf), "Thread%d", num);
        name_ = buf;
    }
}


