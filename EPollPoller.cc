#include "EPollPoller.h"
#include "Logger.h"
#include "errno.h"
#include "Channel.h"

#include <cstring>
#include <unistd.h>
// Channel未添加到Poller中
const int kNew = -1; // Channel的成员index_=-1;
// Channel已添加到Poller中
const int kAdded = 1;
// Channel从Poller中删除
const int kDeleted = 2;
/*
epollfd_(epoll_create1(EPOLL_CLOEXEC)):
EPOLL_CLOEXEC = 创建 epoll fd 的时候就带上 close-on-exec 标志，
确保进程调用 exec() 时这个 fd 不会被子进程继承，避免 fd 泄露。
*/
EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop), epollfd_(epoll_create1(EPOLL_CLOEXEC)), events_(kInitEventListSize)
{
    if (epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error:%d \n", errno);
    }
}
EPollPoller::~EPollPoller()
{
    close(epollfd_);
}

// 重写基类的抽象方法
Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    // 实际上应该LOG_DEBUG输出日志更为合理
    LOG_INFO("func=%s -> fd total count:%lu\n", __FUNCTION__, channels_.size());
    /*
    &*events_.begin()为什么不用&events_[0]？
        1.&events_[0] 只有在 events_ 非空 时才合法；
        如果 events_ 的 size()==0，访问 events_[0] 就是 UB（未定义行为）。
        2.&*events_.begin() 即使 events_ 是空的，也不会立刻触发未定义行为（begin() == end()，
        解引用其实还是 UB，但很多实现里更可控，而且用法约定俗成）。
        3.C++11 以后更推荐用 events_.data()，它即使在空 vector 上返回的也是合法指针（但不能解引用）。
    但是如果在调用 epoll_wait 之前 给 vector 提前 resize 了，那么 &events_[0] 就一定安全了，就可以用&events_[0]
    */
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int savedError = errno;
    Timestamp now(Timestamp::now());
    if (numEvents > 0)
    {
        LOG_INFO("%d events happened\n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        if (numEvents == events_.size())
        {
            events_.resize(events_.size() * 2);
        }
    }
    else if (numEvents == 0)
    {
        LOG_DEBUG("%s timeout!\n", __FUNCTION__);
    }
    else
    {
        if (savedError != EINTR)
        {
            errno = savedError;
            LOG_ERROR("EPOLLPoller::poll() err!");
        }
    }
    return now;
}
// channel update remove =>EventLoop updateChannel removeChannel
//=>Poller updateChannel removeChannel
/*
          EventLoop => poller.poll
    ChannelList     Poller
                    ChannelMap <fd,channel*> epollfd
*/
void EPollPoller::updateChannel(Channel *channel)
{
    const int index = channel->index();
    LOG_INFO("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__, channel->fd(), channel->events(), index);
    if (index == kNew || index == kDeleted)
    {
        if (index == kNew)
        {
            int fd = channel->fd();
            channels_[fd] = channel;
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }
    else // channel已经在poller上注册过了
    {
        int fd = channel->fd();
        if (channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}
// 从poller中删除channel
void EPollPoller::removeChannel(Channel *channel)
{
    int fd = channel->fd();
    channels_.erase(fd);
    LOG_INFO("func=%s => fd=%d \n", __FUNCTION__, channel->fd());
    int index = channel->index();
    if (index == kAdded)
    {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

// 填写活跃的连接
void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const
{
    for (int i = 0; i < numEvents; ++i)
    {
        Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel); // EventLoop就拿到了它的Poller给他返回的所有发生事件的channel列表
    }
}
// 更新channel通道
void EPollPoller::update(int operation, Channel *channel)
{
    epoll_event event;
    memset(&event, 0, sizeof(event));
    event.events = channel->events();
    event.data.ptr = channel;
    int fd = channel->fd();

    if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
    {
        LOG_ERROR("epoll_ctl del error:%d\n", errno);
    }
    else
    {
        LOG_INFO("epoll_ctl op=%d fd=%d success", operation, fd);
    }
}