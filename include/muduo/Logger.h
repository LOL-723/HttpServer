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
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL,
};

class Logger : noncopyable
{
public:
    static Logger& instance();   

    void setLogLevel(int level); 
    void log(int level, const std::string& msg); 
    int getLogLevel() const { return logLevel_; }
    bool enabled(int level) const { return level >= logLevel_; }

private:
    int logLevel_ = INFO;
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
    do { if (muduo::Logger::instance().enabled(muduo::INFO)) muduo::LogStream(muduo::INFO) << muduo::formatLog(__VA_ARGS__); } while (0)

#define LOG_ERROR(...) \
    do { if (muduo::Logger::instance().enabled(muduo::ERROR)) muduo::LogStream(muduo::ERROR) << muduo::formatLog(__VA_ARGS__); } while (0)

#define LOG_WARN(...) \
    do { if (muduo::Logger::instance().enabled(muduo::WARN)) muduo::LogStream(muduo::WARN) << muduo::formatLog(__VA_ARGS__); } while (0)

#define LOG_FATAL(...) \
    muduo::LogStream(muduo::FATAL, true) << muduo::formatLog(__VA_ARGS__)

#ifdef MUDEBUG
#define LOG_DEBUG(...) \
    do { if (muduo::Logger::instance().enabled(muduo::DEBUG)) muduo::LogStream(muduo::DEBUG) << muduo::formatLog(__VA_ARGS__); } while (0)
#else
#define LOG_DEBUG(...) do {} while(0)
#endif

} // namespace muduo
