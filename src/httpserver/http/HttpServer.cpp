#include "../../../include/httpserver/http/HttpServer.h"

#include <any>
#include <algorithm>
#include <cctype>
#include <functional>
#include <memory>

namespace http
{
namespace
{
std::string toLowerCopy(const std::string& value)
{
    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return lowered;
}

std::string stripPort(const std::string& hostHeader)
{
    if (hostHeader.empty()) {
        return hostHeader;
    }
    if (hostHeader.front() == '[') {
        const auto closing = hostHeader.find(']');
        if (closing != std::string::npos) {
            return hostHeader.substr(0, closing + 1);
        }
    }
    const auto colon = hostHeader.find(':');
    if (colon == std::string::npos) {
        return hostHeader;
    }
    return hostHeader.substr(0, colon);
}

bool shouldRedirectToLocalhost(const std::string& hostHeader)
{
    const std::string hostOnly = toLowerCopy(stripPort(hostHeader));
    return hostOnly == "127.0.0.1" || hostOnly == "[::1]";
}

std::string portFromHostHeader(const std::string& hostHeader)
{
    if (hostHeader.empty()) {
        return "";
    }
    if (hostHeader.front() == '[') {
        const auto closing = hostHeader.find(']');
        if (closing != std::string::npos && closing + 1 < hostHeader.size() && hostHeader[closing + 1] == ':') {
            return hostHeader.substr(closing + 2);
        }
        return "";
    }
    const auto colon = hostHeader.rfind(':');
    if (colon == std::string::npos) {
        return "";
    }
    return hostHeader.substr(colon + 1);
}
}

// 默认http回应函数
void defaultHttpCallback(const HttpRequest &, HttpResponse *resp)
{
    resp->setStatusCode(HttpResponse::k404NotFound);
    resp->setStatusMessage("Not Found");
    resp->setCloseConnection(true);
}

HttpServer::HttpServer(int port,
                       const std::string &name,
                       bool useSSL,
                       muduo::TcpServer::Option option)
    : listenAddr_(port)
    , server_(&mainLoop_, listenAddr_, name, option)
    , useSSL_(useSSL)
    , httpCallback_(std::bind(&HttpServer::handleRequest, this, std::placeholders::_1, std::placeholders::_2))
{
    initialize();
}

void HttpServer::initialize()
{
    server_.setConnectionCallback(
        std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
    server_.setMessageCallback(
        std::bind(&HttpServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void HttpServer::start(){
    LOG_WARN ("HttpServer[%s] starts listening on %s,",server_.name().c_str(),server_.ipPort().c_str());
    server_.start();
    mainLoop_.loop();
}

void HttpServer::setSslConfig(const ssl::SslConfig &config){
    sslCtx_ = std::make_unique<ssl::SslContext>(config);
    if (!sslCtx_->initialize()){
        sslCtx_.reset();
        useSSL_ = false;
        throw std::runtime_error("Failed to initialize SSL context");
    }
    useSSL_ = true;
}

std::shared_ptr<ssl::SslConnection> HttpServer::findSslConnection(const muduo::TcpConnectionPtr& conn)
{
    std::lock_guard<std::mutex> lock(sslConnsMutex_);
    auto it = sslConns_.find(conn);
    if (it == sslConns_.end()) {
        return nullptr;
    }
    return it->second;
}

void HttpServer::onConnection(const muduo::TcpConnectionPtr&conn){
    if(conn->connected()){
        conn->setContext(std::any(HttpContext()));
        if(useSSL_){
            auto sslconn = std::make_shared<ssl::SslConnection>(conn, sslCtx_.get());
            sslconn->setMessageCallback(std::bind(&HttpServer::onMessage,this,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3));
            {
                std::lock_guard<std::mutex> lock(sslConnsMutex_);
                sslConns_[conn] = sslconn;
            }
            sslconn->startHandshake();
        }
        
    }else{
        if(useSSL_){
            std::lock_guard<std::mutex> lock(sslConnsMutex_);
            sslConns_.erase(conn);
        }
    }
}

void HttpServer::onMessage(const muduo::TcpConnectionPtr &conn,muduo::Buffer *buf,muduo::Timestamp receiveTime){
    try
    {
        // 这层判断只是代表是否支持ssl
        if(useSSL_){
            LOG_INFO ("onMessage useSSL_ is true") ;
            auto sslConn = findSslConnection(conn);
            if(sslConn){
                LOG_INFO ("onMessage sslConns_ is not empty");
                muduo::Buffer* decryptedBuf = sslConn->getDecryptedBuffer();
                if (buf != decryptedBuf) {
                    // 2. SSL连接处理数据
                    sslConn->onRead(conn,buf, receiveTime);
                    // 3. 如果 SSL 握手还未完成，直接返回
                    if(!sslConn->isHandshakeCompleted()){
                        LOG_INFO ("onMessage sslConns_ is not empty");
                        return;
                    }

                    // 4. 从SSL连接的解密缓冲区获取数据
                    if(decryptedBuf->readableBytes()==0){
                        return;// 没有解密后的数据
                    }
                    // 5. 使用解密后的数据进行HTTP 处理
                    buf=decryptedBuf;// 将 buf 指向解密后的数据
                    LOG_INFO ("onMessage decryptedBuf is not empty");
                }
            }
        }
        HttpContext *context = std::any_cast<HttpContext>(conn->getMutableContext()); 
        if (!context)
        {
            LOG_ERROR ("Bad context type");
            conn->shutdown();
            return;
        }
        if (!context->parseRequest(buf, receiveTime)) // 解析一个http请求
        {
            // 如果解析http报文过程中出错
            LOG_ERROR("Failed to parse HTTP request");
            conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
            conn->shutdown();
            return;
        }
        // 如果buf缓冲区中解析出一个完整的数据包才封装响应报文
        if (context->gotAll())
        {
            onRequest(conn, context->request());
            context->reset();
        }
    }
    catch (const std::exception &e){
        // 捕获异常，返回错误信息
        LOG_ERROR ("Exception in onMessage: %s",e.what());
        conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
        conn->shutdown();
    }
}

void HttpServer::onRequest(const muduo::TcpConnectionPtr&conn,const HttpRequest &req){
    const std::string connection = toLowerCopy(req.getHeader("Connection"));
    const bool isHttp10 = req.getVersion() == "HTTP/1.0";
    const bool close = connection == "close" || (isHttp10 && connection != "keep-alive");
    HttpResponse response(close);
    response.setVersion(req.getVersion());
    response.setStatusCode(HttpResponse::k200Ok);
    response.setStatusMessage("OK");

    // 根据请求报文信息来封装响应报文对象
    httpCallback_(req,&response); // 执行onHttpCallback函数
    // 可以给response设置一个成员，判断是否请求的是文件，如果是文件设置为true，并且存在文件位置在这里send出去。
    muduo::Buffer buf;
    response.appendToBuffer(&buf);
    std::string responseData = buf.retrieveAllAsString();
    // 打印完整的响应内容用于调试
    LOG_INFO ("Sending response:\n%s", responseData.c_str());
    if (useSSL_) {
        auto sslConn = findSslConnection(conn);
        if (sslConn && sslConn->isHandshakeCompleted()) {
            sslConn->send(responseData.data(), responseData.size());
        } else {
            LOG_ERROR("SSL connection is not ready for sending response");
            conn->shutdown();
            return;
        }
    } else {
        conn->send(responseData);
    }
    // 如果是短连接的话，返回响应报文后就断开连接
    if (response.closeConnection())
    {
        conn->shutdown();
    }
}

// 执行请求对应的路由处理函数
void HttpServer::handleRequest(const HttpRequest&req,HttpResponse *resp){
    try{
        const std::string hostHeader = req.getHeader("Host");
        const bool shouldCanonicalizeHost =
            useSSL_ &&
            req.method() == HttpRequest::kGet &&
            shouldRedirectToLocalhost(hostHeader);
        if (shouldCanonicalizeHost) {
            const std::string port = portFromHostHeader(hostHeader);
            std::string redirectUrl = "https://localhost";
            if (!port.empty()) {
                redirectUrl += ":" + port;
            }
            redirectUrl += req.path();

            resp->setStatusCode(HttpResponse::k301MovedPermanently);
            resp->setStatusMessage("Moved Permanently");
            resp->setCloseConnection(true);
            resp->addHeader("Location", redirectUrl);
            return;
        }

        //处理请求前的中间件
        HttpRequest mutablereq=req;
        middlewareChain_.processBefore(mutablereq);
        //路由处理
        if(!router_.route(mutablereq, resp)){
            LOG_INFO("请求方法:%s, url:%s",req.methodString(),req.path().c_str());
            LOG_INFO ("未找到路由,返回404");
            resp->setStatusCode(HttpResponse::k404NotFound);
            resp->setStatusMessage("Not Found");
            resp->setCloseConnection(true);
        }
        //处理请求后的中间件
        middlewareChain_.processAfter(*resp);
    }   
    catch (const HttpResponse& res) 
    {
        // 处理中间件抛出的响应（如CORS预检请求）
        *resp = res;
    }
    catch (const std::exception& e) 
    {
        // 错误处理
        resp->setStatusCode(HttpResponse::k500InternalServerError);
        resp->setStatusMessage("Internal Server Error");
        resp->setBody(e.what());
    }
}
}//namespace http
