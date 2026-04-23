#include "../../../include/httpserver/http/HttpResponse.h"

namespace http
{
namespace
{
const char* defaultStatusMessage(HttpResponse::HttpStatusCode statusCode)
{
    switch (statusCode)
    {
    case HttpResponse::k200Ok:
        return "OK";
    case HttpResponse::k204NoContent:
        return "No Content";
    case HttpResponse::k301MovedPermanently:
        return "Moved Permanently";
    case HttpResponse::k400BadRequest:
        return "Bad Request";
    case HttpResponse::k401Unauthorized:
        return "Unauthorized";
    case HttpResponse::k403Forbidden:
        return "Forbidden";
    case HttpResponse::k404NotFound:
        return "Not Found";
    case HttpResponse::k409Conflict:
        return "Conflict";
    case HttpResponse::k500InternalServerError:
        return "Internal Server Error";
    default:
        return "OK";
    }
}
}

void HttpResponse::appendToBuffer(muduo::Buffer* outputBuf) const{
    // HttpResponse封装的信息格式化输出
    char buf[32];
    const std::string version = httpVersion_.empty() ? "HTTP/1.1" : httpVersion_;
    const HttpStatusCode statusCode = statusCode_ == kUnknown ? k200Ok : statusCode_;
    const std::string statusMessage = statusMessage_.empty() ? defaultStatusMessage(statusCode) : statusMessage_;
    // 为什么不把状态信息放入格式化字符串中，因为状态信息有长有短，不方便定义一个固定大小的内存存储
    int len=snprintf(buf,sizeof(buf),"%s %d ",version.c_str(),statusCode);

    outputBuf->append(buf,len);
    outputBuf->append(statusMessage.c_str(), statusMessage.size());
    outputBuf->append("\r\n");

    if (closeConnection_) // 思考一下这些地方是不是可以直接移入近headers_中
    {
        outputBuf->append("Connection: close\r\n");
    }
    else
    {
        //snprintf(buf, sizeof buf, "Content-Length: %zd\r\n", body_.size());
        //outputBuf->append(buf);
        outputBuf->append("Connection: Keep-Alive\r\n");
    }
    if (headers_.find("Content-Length") == headers_.end())
    {
        int contentLengthLen = snprintf(buf, sizeof(buf), "Content-Length: %zu\r\n", body_.size());
        outputBuf->append(buf, contentLengthLen);
    }
    for(const auto &header:headers_){
        outputBuf->append(header.first);
        outputBuf->append(": ");
        outputBuf->append(header.second);
        outputBuf->append("\r\n");
    }
    outputBuf->append("\r\n");
    
    outputBuf->append(body_);
}

void HttpResponse::setStatusLine(const std::string& version,
                                 HttpStatusCode statusCode,
                                 const std::string& statusMessage)
{
    httpVersion_ = version;
    statusCode_ = statusCode;
    statusMessage_ = statusMessage;
}

}//namespace hhtp
