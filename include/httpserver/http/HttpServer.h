#pragma once 

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <unordered_map>

#include <muduo/TcpServer.h>
#include <muduo/EventLoop.h>
#include <muduo/Logger.h>

#include "HttpContext.h"
#include "HttpRequest.h"
#include "HttpResponse.h"

namespace http{
class HttpServer : muduo::noncopyable{
private:
    void onRequest(const muduo::TcpConnectionPtr&, const HttpRequest&);
    
};
}//namespace http