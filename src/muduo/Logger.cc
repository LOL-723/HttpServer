#include <muduo/Logger.h>
#include <cstdio>
#include <cstdlib>

namespace muduo
{

Logger& Logger::instance()
{
    static Logger inst;
    return inst;
}

void Logger::setLogLevel(int level)
{
    logLevel_ = level;
}

void Logger::log(const std::string& msg)
{
    switch (logLevel_)
    {
        case INFO:
            printf("[INFO] %s\n", msg.c_str());
            break;
        case ERROR:
            printf("[ERROR] %s\n", msg.c_str());
            break;
        case FATAL:
            printf("[FATAL] %s\n", msg.c_str());
            break;
        case DEBUG:
            printf("[DEBUG] %s\n", msg.c_str());
            break;
        case WARN:
            printf("[WARN] %s\n", msg.c_str());
            break;
        default:
            printf("[UNKNOWN] %s\n", msg.c_str());
            break;
    }
}

// ================= LogStream =================

LogStream::LogStream(int level, bool shouldExit)
    : level_(level), shouldExit_(shouldExit)
{
}

LogStream::~LogStream()
{
    // 获取当前全局日志级别
    int currentLevel = Logger::instance().getLogLevel();  // 需要添加 getLogLevel()
    
    // 只有当日志级别 >= 全局级别时才输出
    if (level_ >= currentLevel)
    {
        Logger::instance().log(stream_.str());
    }

    if (shouldExit_)
    {
        std::abort();
    }
}
} // namespace muduo