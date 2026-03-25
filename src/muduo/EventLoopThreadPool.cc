#include <memory>

#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"
#include "Logger.h"

EventLoopThreadPool::EventLoopThreadPool(EventLoop*baseloop,const std::string&nameArg)
    :baseLoop_(baseloop),
    name_(nameArg),
    started_(false),
    numThreads_(0),
    next_(0)
{}

EventLoopThreadPool::~EventLoopThreadPool(){
    // Don't delete loop, it's stack variable
}

void EventLoopThreadPool::start(const ThreadInitCallback&cb){
    started_=true;
    for(int i=0;i<numThreads_;++i){
        char buf[name_.size()+32];
        snprintf(buf,sizeof buf,"%s%d",name_.c_str(),i);
        EventLoopThread*t=new EventLoopThread(cb,buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        loops_.push_back(t->startloop());
    }
    if(numThreads_==0 && cb){
        cb(baseLoop_);
    }
}

// 如果工作在多线程中，baseLoop_(mainLoop)会默认以轮询的方式分配Channel给subLoop
EventLoop* EventLoopThreadPool::getNextLoop(){
    // 如果只设置一个线程 也就是只有一个mainReactor 无subReactor 
    // 那么轮询只有一个线程 getNextLoop()每次都返回当前的baseLoop_
    EventLoop*Loop=baseLoop_;
    // 通过轮询获取下一个处理事件的loop
    // 如果没设置多线程数量，则不会进去，相当于直接返回baseLoop
    if(!loops_.empty()){/*注意这里是if，不是循环*/
        Loop=loops_[next_];
        next_++;
        // 轮询
        if(next_>=loops_.size()){
            next_=0;
        }
    }
    return Loop;
    /*
        每次只返回一个loop，一次一次从头遍历到尾，到最后一位的时候，把next重新改回0，再从头轮询
    */
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops(){
    if(loops_.empty()){
        return std::vector<EventLoop*>(1,baseLoop_);
    }else{
        return loops_;
    }
}
