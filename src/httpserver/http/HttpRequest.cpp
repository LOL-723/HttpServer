#include "../../../include/httpserver/http/HttpRequest.h"

#include <cassert>

namespace http
{

    void HttpRequest::setReceiveTime(muduo::Timestamp t)
    {
        receiveTime_ = t;
    }

    bool HttpRequest::setMethod(const char *start, const char *end)
    {
        assert(method_ == kInvalid);
        std::string m(start, end); // [start, end)
        if (m == "GET")
        {
            method_ = kGet;
        }
        else if (m == "POST")
        {
            method_ = kPost;
        }
        else if (m == "PUT")
        {
            method_ = kPut;
        }
        else if (m == "DELETE")
        {
            method_ = kDelete;
        }
        else if (m == "OPTIONS")
        {
            method_ = kOptions;
        }
        else
        {
            method_ = kInvalid;
        }

        return method_ != kInvalid;
    }
    void HttpRequest::setPath(const char* start, const char* end){
        path_.assign(start,end);
    }
    void HttpRequest:: setPathParameters(const std::string &key, const std::string &value){
        pathParameters_[key]=value;
    }
    std::string HttpRequest::getPathParameters(const std::string &key)const{
        auto it=pathParameters_.find(key);
        if(it!=pathParameters_.end()){
            return it->second;
        }
        return "";
    }

    std::string HttpRequest::getQueryParameters(const std::string &key)const{
        auto it=queryParameters_.find(key);
        if(it!=queryParameters_.end()){
            return it->second;
        }
        return "";
    }
    
    // 这是从问号后面分割参数
    void HttpRequest::setQueryParameters(const char* start, const char* end){
        std::string argumentStr(start,end);
        std::string::size_type pos=0;//pos代表&位置
        std::string::size_type prev=0;//prev代表起始位置

        // 按 & 分割多个参数
        while((pos=argumentStr.find('&',prev))!=std::string::npos){
            std::string pair=argumentStr.substr(prev,pos-prev);
            std::string::size_type equalPos=pair.find('=');
            if(equalPos!=std::string::npos){
                std::string key=pair.substr(0,equalPos);
                std::string value=pair.substr(equalPos+1);
                queryParameters_[key]=value;
            }
            prev=pos+1;
        }
        // 处理最后一个参数
        std::string lastPair = argumentStr.substr(prev);
        std::string::size_type equalPos = lastPair.find('=');
        if (equalPos != std::string::npos)
        {
            std::string key = lastPair.substr(0, equalPos);
            std::string value = lastPair.substr(equalPos + 1);
            queryParameters_[key] = value;
        }
    }
    /*
        std::string pair = "id=123";
        // 位置:        0  1  2  3  4  5
        //              i  d  =  1  2  3

        size_t equalPos = pair.find('=');  // equalPos = 2

        // substr(0, equalPos) → 从0开始，取2个字符
        std::string key = pair.substr(0, equalPos);
        // key = "id"  (取位置0和1，不包括位置2的'=')

        // substr(equalPos + 1) → 从位置3开始到结尾
        std::string value = pair.substr(equalPos + 1);
        // value = "123"  (从位置3开始)
    */

    void HttpRequest::addHeader(const char* start, const char* colon, const char* end){
        std::string key(start,colon);
        colon++;
        while(colon<end && isspace(*colon)){
            ++colon;
        }
        std::string value(colon,end);
        while(!value.empty() && isspace(value[value.size()-1])){
            value.resize(value.size()-1);
        }
        headers_[key]=value;
    }

    std::string HttpRequest::getHeader(const std::string &field) const
    {
        std::string result;
        auto it = headers_.find(field);
        if (it != headers_.end())
        {
            result = it->second;
        }
        return result;
    }

    //swap 的本质是高效地交换两个对象的内部状态，通常通过交换指针/句柄实现 O(1) 的复杂度，避免昂贵的深拷贝
    void HttpRequest::swap(HttpRequest &that)
    {
        std::swap(method_, that.method_);
        std::swap(path_, that.path_);
        std::swap(pathParameters_, that.pathParameters_);
        std::swap(queryParameters_, that.queryParameters_);
        std::swap(version_, that.version_);
        std::swap(headers_, that.headers_);
        std::swap(receiveTime_, that.receiveTime_);
    }
    /*
        主要用途：
        1.实现异常安全的赋值操作（copy-and-swap idiom）
        2.实现高效的移动语义
        3.容器操作中的高效交换
        4.实现 RAII 的资源转移
    */
}