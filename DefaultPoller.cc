#include "Poller.h"
#include "EPollPoller.h"

#include <stdlib.h>

// EventLoop可以通过该接口获取默认的IO复用的具体实现
// 但是这个不可以在Poller层实现，必须有具体的类才可以
// 所以我们新建DefaPoller.cc把实现写到这里，这样就避免include子类的问题
Poller *Poller::newDefaultPoller(EventLoop *loop)
{
    if (getenv("MUDUO_USE_POLL"))
    {
        return nullptr; // 生成poll的实例
    }
    else
    {
        return new EPollPoller(loop); // 生成epoll的实例
    }
}