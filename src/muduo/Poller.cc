#include "Poller.h"
#include "Channel.h"
#include "EPollPoller.h" 

Poller::Poller(EventLoop *loop)
    : ownerLoop_(loop)
{
}
Poller* Poller::newDefaultPoller(EventLoop* loop) {
    // 使用 epoll（Linux 推荐）
    return new EPollPoller(loop);
    
    // 或者使用 poll（更通用）
    // return new PollPoller(loop);
}


// bool Poller::hasChannel(Channel *channel) const
// {
//     auto it = channels_.find(channel->fd());
//     return it != channels_.end() && it->second == channel;
// }