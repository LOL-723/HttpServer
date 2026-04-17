#pragma once
#include <string>

namespace ssl 
{

// SSL/TLS 协议版本
enum class SSLVersion 
{
    TLS_1_3
};

// SSL 错误类型
enum class SSLError
{
    NONE,           // 没有错误

    WANT_READ,      // 需要等待可读
    WANT_WRITE,     // 需要等待可写

    ZERO_RETURN,    // TLS 正常关闭（close_notify）

    SYSCALL,        // 系统调用错误（socket）
    SSL,            // 协议错误（握手/加密等）

    WANT_CONNECT,   // 非阻塞 connect（可选）
    WANT_ACCEPT,    // 非阻塞 accept（可选）

    UNKNOWN         // 未知错误
};

// SSL 状态
enum class SSLState
{
    INIT,           // 刚创建，还没开始握手

    HANDSHAKING,    // 握手进行中（非阻塞核心）
    ESTABLISHED,    // 握手完成，可以收发数据

    SHUTTING_DOWN,  // 正在关闭（close_notify 过程）
    CLOSED,         // 完全关闭

    ERROR           // 致命错误
};

} // namespace ssl