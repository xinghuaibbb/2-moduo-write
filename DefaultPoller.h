#include "Poller.h"

#include <stdlib.h>

Poller* Poller::newDefaultPoller(EventLoop* loop)
{
    if (::getenv("MUDUO_USE_POLL"))
    {
        return nullptr; // 这里可以返回一个Poll的实现
    }
    else
    {
        return nullptr; // 这里可以返回一个EPoll的实现
    }
}