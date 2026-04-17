#pragma once

#include <string>
#include <sstream>
#include <cstdarg>
#include <utility>

#include "noncopyable.h"

namespace muduo
{

enum LogLevel
{
    INFO,
    ERROR,
    FATAL,
    DEBUG,
    WARN,
};

class Logger : noncopyable
{
public:
    static Logger& instance();   

    void setLogLevel(int level); 
    void log(const std::string& msg); 
    int getLogLevel() const { return logLevel_; }

private:
    int logLevel_;
};

// ================= LogStream =================
class LogStream
{
public:
    LogStream(int level, bool shouldExit = false);
    ~LogStream();

    template<typename T>
    LogStream& operator<<(const T& val)
    {
        stream_ << val;
        return *this;
    }

private:
    int level_;
    bool shouldExit_;
    std::ostringstream stream_;
};

// ================= formatLog =================

inline std::string formatLog(const char* format, ...)
{
    char buf[1024] = {0};

    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    return std::string(buf);
}

inline std::string formatLog(const std::string& msg)
{
    return msg;
}

// ================= 宏 =================

#define LOG_INFO(...) \
    muduo::LogStream(muduo::INFO) << muduo::formatLog(__VA_ARGS__)

#define LOG_ERROR(...) \
    muduo::LogStream(muduo::ERROR) << muduo::formatLog(__VA_ARGS__)

#define LOG_WARN(...) \
    muduo::LogStream(muduo::WARN) << muduo::formatLog(__VA_ARGS__)

#define LOG_FATAL(...) \
    muduo::LogStream(muduo::FATAL, true) << muduo::formatLog(__VA_ARGS__)

#ifdef MUDEBUG
#define LOG_DEBUG(...) \
    muduo::LogStream(muduo::DEBUG) << muduo::formatLog(__VA_ARGS__)
#else
#define LOG_DEBUG(...) do {} while(0)
#endif

} // namespace muduo