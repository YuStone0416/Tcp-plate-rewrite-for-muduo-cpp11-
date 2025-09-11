#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>
// 防止一个线程创建多个EventLoop
__thread EventLoop *t_loopThisThread = nullptr;

// 定义默认的Poller IO复用接口的超时时间
const int kPollTimeMs = 10000;

// 创建wakeupfd,用来notify唤醒subReactor处理新来的channel
/*
跨线程安全唤醒
eventfd返回值就是一个内核提供的原子计数器 fd，能被 epoll 监听，写就是“通知+1”，读就是“取值并清零”。
*/
int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        LOG_FATAL("eventfd err:%d\n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false), quit_(false), callingPendingFunctors_(false), threadId_(CurrentThread::tid()), poller_(Poller::newDefaultPoller(this)), wakeupFd_(createEventfd()), wakeupChannel_(new Channel(this, wakeupFd_))
{
    LOG_DEBUG("EventLoop create %p in thread %d\n", this, threadId_);
    if (t_loopThisThread)
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d \n", t_loopThisThread, threadId_);
    }
    else
    {
        t_loopThisThread = this;
    }

    // 设置wakeupfd的事件类型以及发生事件后的回调
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handlerRead, this));
    // 每一个eventloop都将监听wakeupchannel的EPOLLIN读事件
    wakeupChannel_->enableReading();
}
EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopThisThread = nullptr;
}
// 开启事件循环
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;
    LOG_INFO("EventLoop %p start looping\n", this);

    while (!quit_)
    {
        activeChannels_.clear();
        // 监听两类fd 一种是client的fd,一种是wakeupfd
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (Channel *channel : activeChannels_)
        {
            // Poller监听哪些channnel发生事件了，然后上报给EventLoop,通知channel处理相应的事件
            channel->handlerEvent(pollReturnTime_);
        }
        // 执行当前EventLoop事件循环需要处理的回调操作
        /*
        IO线程 mainLoop accept fd<=channel subLoop
        mainLoop 实现注册一个回调cb(需要subLoop执行) wakeup subLoop
        执行下面的方法，执行之前mainloop注册的cb
        */
        doPendingFunctors();
    }
    LOG_INFO("EventLoop %p stop looping", this);
    looping_ = false;
}
// 退出事件循环 1.loop在自己的线程调用quit 2.在非loop的线程中，调用loop的quit
void EventLoop::quit()
{
    quit_ = true;

    // 如果在其他线程中调用quit 在一个subloop(worker)中调用了mainLoop(IO)的quit，
    // 由于不知道状态，所以要wakeup
    if (!isInLoopThread())
    {
        wakeup();
    }
}

// 在当前loop执行cb
void EventLoop::runInLoop(Functor cb)
{
    // 在当前的loop线程执行cb
    if (isInLoopThread())
    {
        cb();
    }
    else // 在非当前loop中执行cb,就需要唤醒loop所在线程，执行cb
    {
        queueInLoop(cb);
    }
}
// 把cb放入队列中，唤醒loop所在的线程，执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lokc(mutex_);
        PendingFunctors_.emplace_back(std::move(cb));
    }
    // 唤醒相应的，需要执行上面回调操作的loop的线程了
    // || callingPendingFunctors_的意思：如果当前loop正在执行回调，但是loop又有了新的回调，所以需要唤醒，不能再阻塞
    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup(); // 唤醒loop所在线程
    }
}

void EventLoop::handlerRead() // wake up
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one))
    {
        LOG_ERROR("EventLoop::handleRead() reads %ld bytes instead of 8", n);
    }
}

// 用来唤醒loop所在的线程   向wakeupfd_写一个数据
void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one))
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8", n);
    }
}

// EventLoop的方法 =》Poller的方法
void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel *channel)
{
    return poller_->hasChannel(channel);
}
// 执行回调
void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(PendingFunctors_);
    }
    for (const Functor &functor : functors)
    {
        functor();//执行当前loop需要执行的回调操作
    }
    callingPendingFunctors_ = false;
}