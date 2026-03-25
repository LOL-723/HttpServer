#pragma once

#include <functional>
#include <memory>

#include "Logger.h"
#include "noncopyable.h"
#include "Timestamp.h"

namespace muduo
{

class EventLoop;

/**
 * 理清楚 EventLoop、Channel、Poller之间的关系  Reactor模型上对应多路事件分发器
 * Channel理解为通道 封装了sockfd和其感兴趣的event 如EPOLLIN、EPOLLOUT事件 还绑定了poller返回的具体事件
 **/
class Channel
{
public:
    using EventCallBack=std::function<void()>;
    using ReadEventCallBack=std::function<void(Timestamp)>;

    Channel(EventLoop*loop,int fd);
    ~Channel();
    // fd得到Poller通知以后 处理事件 handleEvent在EventLoop::loop()中调用
    void handleEvent(Timestamp receiveTime);
    // 设置回调函数对象
    void setReadCallBack(ReadEventCallBack cb){readCallBack_=std::move(cb);}
    void setWriteCallBack(EventCallBack cb){writeCallBack_=std::move(cb);}
    void setCloseCallBack(EventCallBack cb){closeCallBack_=std::move(cb);}
    void setErrorCallBack(EventCallBack cb){errorCallBack_=std::move(cb);}
    // 防止当channel被手动remove掉 channel还在执行回调操作
    void tie(const std::shared_ptr<void>&);

    int fd()const{ return fd_;}
    int events(){return events_;}
    void set_revents(int revent){revents_=revent;}
    // 设置fd相应的事件状态 相当于epoll_ctl add delete
    void enableReading(){events_ |=KReadEvent;update();}
    void disableReading(){events_ &=~KReadEvent;update();}
    void enableWriting(){events_ |=KWriteEvent;update();}
    void disableWriting(){events_ &=~KWriteEvent;update();}
    void disableAll(){events_ =KNoneEvent;update();}
    // 返回fd当前的事件状态
    bool isNoneEvent(){return events_==KNoneEvent;}
    bool isWriting(){return events_ &KWriteEvent;}
    bool isReading(){return events_ & KReadEvent;}

    int index(){return index_;}
    void set_index(int index){index_=index;}

    // one loop per thread
    EventLoop* ownloop(){return loop_;}
    void remove();
private:
    void update();
    void handleEventWithGuard(Timestamp receiveTime);
    
    std::weak_ptr<void>tie_;
    bool tied_;

    int index_;

    EventLoop *loop_; // 事件循环
    const int fd_;    // fd，Poller监听的对象
    int events_;      // 注册fd感兴趣的事件
    int revents_;     // Poller返回的具体发生的事件

    static int KNoneEvent;
    static int KReadEvent;
    static int KWriteEvent;
    // 因为channel通道里可获知fd最终发生的具体的事件events，所以它负责调用具体事件的回调操作
    EventCallBack writeCallBack_;
    EventCallBack closeCallBack_;
    EventCallBack errorCallBack_;
    ReadEventCallBack readCallBack_;
};

} // namespace muduo


///////////////////////////////////////////////////////////////////////////////////////
// ║    ██████╗     ██████╗     ██████╗     ██████╗      ██████╗     ██╗    ██╗     ║//
// ║   ██╔════╝    ██╔═══██╗    ██╔══██╗    ██╔══██╗    ██╔═══██╗    ██║    ██║     ║//
// ║   ███████╗    ██║   ██║    ██████╔╝    ██████╔╝    ██║   ██║    ██║ █╗ ██║     ║//
// ║   ╚════██║    ██║   ██║    ██╔══██╗    ██╔══██╗    ██║   ██║    ██║███╗██║     ║//
// ║   ██████║     ╚██████╔╝    ██║  ██║    ██║  ██║    ╚██████╔╝    ╚███╔███╔╝     ║//
// ║   ╚══════╝     ╚═════╝     ╚═╝  ╚═╝    ╚═╝  ╚═╝     ╚═════╝      ╚══╝╚══╝      ║//
///////////////////////////////////////////////////////////////////////////////////////
//       SORROW - The foundation of growth lies in the depths of sadness.            //
///////////////////////////////////////////////////////////////////////////////////////
