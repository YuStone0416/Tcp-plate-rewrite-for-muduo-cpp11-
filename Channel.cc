#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <sys/epoll.h>
const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false)
{
}
Channel::~Channel()
{
}

// 防止当channel被手动remove掉，channel还在执行回调操作
void Channel::tie(const std::shared_ptr<void> &obj)
{
    tie_ = obj;
    tied_ = true;
}

/*
当改变channel所表示fd的events事件后，update负责在Poller里面更改fd相应的事件epoll_ctl
EventLoop => ChannelList Poller
*/
void Channel::update()
{
    // 通过Channel所属的EventLoop,调用Poller的相应方法，注册fd的events事件
    loop_->updateChannel(this);
}
// 在channel所属的EventLoop中，把当前的Channel删除掉
void Channel::remove()
{
    loop_->removeChannel(this);
}

// fd得到Poller通知以后，处理事件
void Channel::handlerEvent(Timestamp receiveTime)
{
    if (tied_)
    {
        std::shared_ptr<void> guard = tie_.lock();
        if (guard)
        {
            handleEventWithGuard(receiveTime);
        }
    }
    else
    {
        handleEventWithGuard(receiveTime);
    }
}

// 根据Poller通知的channel发生的具体事件，由channel负责调用具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    LOG_INFO("Channel handlerEvent revents:%d", revents_);
    // EPOLLHUP：对端关闭连接（挂起事件）
    // 判断“只有挂起且不可读”才触发关闭回调
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN))
    {
        if (closeCallback_)
        {
            closeCallback_();
        }
    }

    if (revents_ & EPOLLERR)
    {
        if (errorCallback_)
        {
            errorCallback_();
        }
    }
    if (revents_ & (EPOLLIN | EPOLLPRI))
    {
        if (readCallback_)
        {
            readCallback_(receiveTime);
        }
    }
    if (revents_ & EPOLLOUT)
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }
}