#include <string>
#include <iostream>
#include <muduo/TcpServer.h>
#include <muduo/Logger.h>
#include <muduo/EventLoop.h>

#include "GomokuServer.h"

int main(int argc, char* argv[])
{
  LOG_INFO ("pid = %d",getpid());
  
  std::string serverName = "HttpServer";
  int port = 80;
  
  // 参数解析
  int opt;
  const char* str = "p:";
  while ((opt = getopt(argc, argv, str)) != -1)
  {
    switch (opt)
    {
      case 'p':
      {
        port = atoi(optarg);
        break;
      }
      default:
        break;
    }
  }
  
  muduo::Logger::instance().setLogLevel(muduo::WARN);  
  GomokuServer server(port, serverName);
  server.setThreadNum(4);
  server.start();
}
