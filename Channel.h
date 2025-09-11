#pragma once

#include "noncopyable.h"
#include "Timestamp.h"

#include <functional>
#include <memory>
/*
关于前置声明 class EventLoop;
什么时候需要 include？
    1.如果类成员是对象（非指针/引用），编译器需要知道对象的大小 → 必须 include。
    2.如果函数参数/返回值是对象（非指针/引用），也需要完整定义。→ 必须 include。
    3.如果只是声明指针、引用，或者在函数里只用到“声明”，那就只 forward declare。

前向声明 vs include 是 编译期依赖管理问题。
    你做成动态库时，头文件暴露给使用者。
    如果使用者只需要知道有 class EventLoop; 就能编译，那库的头文件就不用 include EventLoop.h。
    这样用户编译时依赖更少，只在链接时才需要 .so 提供实现。

能 forward declare 就 forward declare，只有在需要完整定义时才 include。
这样做可以减少编译依赖，加快编译速度，降低耦合。
*/
class EventLoop;
/*
Channel理解为通道，
封装了sockfd和其感兴趣的event,如EPOLLIN,EPOLLOUT事件，
还绑定了poller返回的具体事件
*/
class Channel : noncopyable
{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;
    Channel(EventLoop *loop, int fd);
    ~Channel();

    // fd得到Poller通知以后，处理事件
    void handlerEvent(Timestamp receiveTime);

    // 设置回调函数对象
    /*
    这里回调函数使用值传递+move。
    为什么不把参数设置为引用？首先如果使用lambda对象就不能把参数设置为普通引用。反而如果使用lambda，
    值传递+move这种模式效率更高
    */
    void setReadCallback(ReadEventCallback cb)
    {
        readCallback_ = std::move(cb);
    }
    void setWriteCallback(EventCallback cb)
    {
        writeCallback_ = std::move(cb);
    }
    void setCloseCallback(EventCallback cb)
    {
        closeCallback_ = std::move(cb);
    }
    void setErrorCallback(EventCallback cb)
    {
        errorCallback_ = std::move(cb);
    }

    // channel的tie方法什么时候调用？TcpConnection => Channel
    void tie(const std::shared_ptr<void> &);

    int fd() const
    {
        return fd_;
    }
    int events() const { return events_; }
    void set_revents(int revt) { revents_ = revt; }

    // 设置fd相应的事件状态
    void enableReading()
    {
        events_ |= kReadEvent;
        update();
    }
    void disableReading()
    {
        events_ &= ~kReadEvent;
        update();
    }
    void enableWriting()
    {
        events_ |= kWriteEvent;
        update();
    }
    void disableWriting()
    {
        events_ &= ~kWriteEvent;
        update();
    }
    void disableAll()
    {
        events_ = kNoneEvent;
        update();
    }

    // 返回fd当前的事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    // one loop per thread
    EventLoop *ownerLoop() { return loop_; }

    void remove();

private:
    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_; // 事件循环
    const int fd_;    // fd,Poller监听的对象
    int events_;      // 注册fd感兴趣的事件
    int revents_;     // Poller返回的具体发生的事件
    int index_;

    std::weak_ptr<void> tie_;
    bool tied_;

    // 因为Channel通道里面能够获知fd最终发生的具体的事件的revents，所以它负责调用具体事件的回调操作
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};