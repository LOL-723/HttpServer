#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <functional>

#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

static int createNonBlocking(){
    int sockfd=::socket(AF_INET,SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,IPPROTO_TCP);
    if(sockfd<0){
        LOG_FATAL("%s:%s:%d listen socket create err:%d\n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop*loop,const InetAddress &listenAddr,bool requestor)
    :loop_(loop),
    listening_(false),
    acceptSocket_(createNonBlocking()),
    acceptChannel_(loop,acceptSocket_.fd())
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(true);
    acceptSocket_.bindAddress(listenAddr);
    // TcpServer::start() => Acceptor.listen() 如果有新用户连接 要执行一个回调(accept => connfd => 打包成Channel => 唤醒subloop)
    // baseloop监听到有事件发生 => acceptChannel_(listenfd) => 执行该回调函数
    acceptChannel_.setReadCallBack(std::bind(&Acceptor::handleRead,this));
}

Acceptor::~Acceptor(){
    acceptChannel_.disableAll();// 把从Poller中感兴趣的事件删除掉
    acceptChannel_.remove();// 调用EventLoop->removeChannel => Poller->removeChannel 把Poller的ChannelMap对应的部分删除
}

void Acceptor::listen(){
        LOG_INFO("=== Acceptor::listen() is called ===");  // 添加这行
    listening_=true;
    acceptSocket_.listen();// listen
    acceptChannel_.enableReading();// acceptChannel_注册至Poller !重要
    /*
        该函数底层调用了linux的函数listen( )，
        开启对acceptSocket_的监听同时将acceptChannel及其感兴趣事件（可读事件）注册到main EventLoop的事件监听器上。
        换言之就是让main EventLoop事件监听器去监听acceptSocket_
    */
}

// listenfd有事件发生了，就是有新用户连接了
void Acceptor::handleRead(){
    InetAddress perraddr;
    int connfd=acceptSocket_.accept(&perraddr);
    if(connfd>=0){
        if(NewConnectionCallBack_){
            NewConnectionCallBack_(connfd,perraddr);
        }
    }else{
        ::close(connfd);
    }
}
/*
    最终实现的功能:接受新连接，并且以负载均衡的选择方式选择一个sub EventLoop，并把这个新连接分发到这个subEventLoop上。
*/