#pragma once

/*
为什么用namespace而不用类？
    1.类 (class)：表示有 状态、需要实例化的东西，比如 Channel、Poller、TcpConnection。
    2.命名空间 (namespace)：表示一组 无状态的工具函数/变量，比如 CurrentThread、std::chrono
    对于一些工作函数可以封装到域中。
*/
namespace CurrentThread
{
    /*
    为什么使用__thread?
        __thread 是 GCC/Clang 提供的 线程局部存储 (Thread Local Storage, TLS) 关键字。
        意思是：每个线程都会有一个独立的 t_cachedTid 变量，互不干扰。
        这样做的好处是：避免加锁开销，又能保证每个线程都能缓存自己的线程 ID。
    */
    /*
    为什么使用extern?
        1.extern表示声明，不分配内存，定义在别处（一般跨文件），不extern声明报错
        2.用于全局变量 / 跨文件共享时，头文件写 extern，cpp 文件写定义。
        3.函数声明默认就是 extern。
    */
    extern __thread int t_cachedTid;
    void cacheTid();
    inline int tid()
    {
        if(__builtin_expect(t_cachedTid==0,0))
        {
            cacheTid();
        }
        return t_cachedTid;
    }
}