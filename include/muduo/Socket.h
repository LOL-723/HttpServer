#pragma once

#include "noncopyable.h"

namespace muduo{

class InetAddress;
// 封装socket fd
class Socket : noncopyable{
public:
    explicit Socket(int sockfd)
        :sockfd_(sockfd)
    {}
    ~Socket();

    int fd()const{
        return sockfd_;
    }
    void bindAddress(const InetAddress &localaddr);
    void listen();//监听套接字
    int accept(InetAddress *peeraddr);//接收新连接

    void shutdownWrite();//关闭服务端的写端

    //调用setsocketopt进行设置
    void setTcpNoDelay(bool on);//禁用naggle算法，让小数据包发送，不阻塞
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);
private:
    const int sockfd_;
};
}//namespace muduo