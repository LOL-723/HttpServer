#include <string>
#include <filesystem>
#include <iostream>
#include <exception>
#include <muduo/TcpServer.h>
#include <muduo/Logger.h>
#include <muduo/EventLoop.h>

#include "GomokuServer.h"
#include <httpserver/ssl/SslConfig.h>

int main(int argc, char* argv[])
{
  LOG_INFO ("pid = %d",getpid());
  
  std::string serverName = "HttpServer";
  int port = 8443;
  
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
  try {
    GomokuServer server(port, serverName);
    const std::filesystem::path projectRoot = HTTP_SERVER_PROJECT_ROOT;
    ssl::SslConfig sslConfig;
    sslConfig.setCertificateFile((projectRoot / "certs/server.crt").string());
    sslConfig.setPrivateKeyFile((projectRoot / "certs/server.key").string());
    server.setSslConfig(sslConfig);
    server.setThreadNum(8);
    std::cout << "Sorrow HTTPS server listening on https://localhost:" << port << std::endl;
    server.start();
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
}
