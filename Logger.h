#pragma once
#include "noncopyable.h"

#include <string>

//LOG_INFO("%S %d",arg1,arg2) 宏定义里的 反斜杠 \ 是用来 换行但不结束宏定义 的。
#define LOG_INFO(LogmsgFormat,...)\
    do\
    {\
        Logger &logger=Logger::instance();\
        logger.setLogLevel(INFO);\
        char buf[1024]={0};\
        snprintf(buf,1024,LogmsgFormat,##__VA_ARGS__);\
        logger.log(buf);\
    }while(0)

#define LOG_ERROR(LogmsgFormat,...)\
    do\
    {\
        Logger &logger=Logger::instance();\
        logger.setLogLevel(ERROR);\
        char buf[1024]={0};\
        snprintf(buf,1024,LogmsgFormat,##__VA_ARGS__);\
        logger.log(buf);\
    }while(0)

#define LOG_FATAL(LogmsgFormat,...)\
    do\
    {\
        Logger &logger=Logger::instance();\
        logger.setLogLevel(FATAL);\
        char buf[1024]={0};\
        snprintf(buf,1024,LogmsgFormat,##__VA_ARGS__);\
        logger.log(buf);\
        exit(-1);\
    }while(0)

//如果定义了这个宏，就会输出DEBUG信息，没有就不输出
#ifdef MUDEBUG
#define LOG_DEBUG(LogmsgFormat,...)\
    do\
    {\
        Logger &logger=Logger::instance();\
        logger.setLogLevel(DEBUG);\
        char buf[1024]={0};\
        snprintf(buf,1024,LogmsgFormat,##__VA_ARGS__);\
        logger.log(buf);\
    }while(0)
#else
    #define LOG_DEBUG(LogmsgFormat,...)
#endif
//定义日志的级别 INFO ERROR FATAL DEBUG
enum LogLevel
{
    INFO,//错误信息
    ERROR,//错误信息
    FATAL,//core信息
    DEBUG,//调试信息
};
//输出一个日志类
class Logger:noncopyable
{
public:
    //获取日志唯一的实例对象
    static Logger& instance();
    //设置日志级别
    void setLogLevel(int level);
    //写日志
    void log(std::string msg);
private:
    int logLevel_;
    Logger(){}
};