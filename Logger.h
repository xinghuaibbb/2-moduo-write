#pragma once

#include <string>

#include "noncopyable.h"

// LOG_INFO("%s, %d", arg1, arg2)
#define LOG_INFO(logmsgFormat, ...) \
    do \
    { \
    Logger &logger = Logger::instance(); \
    logger.setLogLevel(INFO); \
    char buf[1024]={0}; \
    sprintf(buf, logmsgFormat, ##__VA_ARGS__); \
    logger.log(buf); \
    }while(0)

// LOG_ERROR("%s, %d", arg1, arg2)
#define LOG_ERROR(logmsgFormat, ...) \
    do \
    { \
    Logger &logger = Logger::instance(); \
    logger.setLogLevel(ERROR); \
    char buf[1024]={0}; \
    sprintf(buf, logmsgFormat, ##__VA_ARGS__); \
    logger.log(buf); \
    }while(0)

// LOG_FATAL("%s, %d", arg1, arg2)
#define LOG_FATAL(logmsgFormat, ...) \
    do \
    { \
    Logger &logger = Logger::instance(); \
    logger.setLogLevel(FATAL); \
    char buf[1024]={0}; \
    sprintf(buf, logmsgFormat, ##__VA_ARGS__); \
    logger.log(buf); \
    }while(0)

#ifdef MUDEBUG
// LOG_DEBUG("%s, %d", arg1, arg2)
#define LOG_DEBUG(logmsgFormat, ...) \
    do \
    { \
    Logger &logger = Logger::instance(); \
    logger.setLogLevel(DEBUG); \
    char buf[1024]={0}; \
    sprintf(buf, logmsgFormat, ##__VA_ARGS__); \
    logger.log(buf); \
    }while(0)
#endif


// 定义日志级别 
enum LogLevel
{
    INFO, // 普通信息
    ERROR, // 错误信息
    FATAL, // 致命(core)错误
    DEBUG, // 调试
};

// 日志类
class Logger :noncopyable
{
public:
    // 获取日志实例
    static Logger& instance();

    // 设置日志级别
    void setLogLevel(int level);

    // 写日志
    void log(std::string msg);


private:
    int logLevel_;
    Logger() {}
};


