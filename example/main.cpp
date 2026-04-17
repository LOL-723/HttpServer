#include <httpserver/http/HttpServer.h>
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        http::HttpServer server;
        // TODO: 需要实现server的启动逻辑
        // server.start();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
