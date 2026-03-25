#pragma once

#include <cassert>
#include <vector>
#include <string>
#include <algorithm>
#include <stddef.h>
//工作流程在最后注释部分

namespace muduo
{

// 网络库底层的缓冲区类型定义
class Buffer{
public:
    static const int kCheapPrepend=8;
    static const int kInitialSize=1024;

    explicit Buffer(size_t initalSize=kInitialSize)
        :buffer_(kCheapPrepend+initalSize),
        readerIndex_(kCheapPrepend),
        writerIndex_(kCheapPrepend)
    {}

    size_t readableBytes()const{return writerIndex_ - readerIndex_;}
    size_t writableBytes()const{return buffer_.size() - writerIndex_;}
    size_t prependableBytes()const{return readerIndex_;}

    
    // 返回缓冲区中可读数据的起始地址
    const char *peek()const{return begin()+readerIndex_;}

    void retrieve(size_t len){
        if(len<readableBytes()){
            readerIndex_+=len;
        }else{
            retrieveAll();
        }
    }

    void retrieveAll()
    {
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }

    // 把onMessage函数上报的Buffer数据 转成string类型的数据返回（上报的是可读数据，readerIndex_到writerIndex_之间的)
    std::string retrieveAllAsString(){return retrieveAsString(readableBytes());}
    std:: string retrieveAsString(size_t len){
        std::string result(peek(),len);
        retrieve(len);// 上面一句把缓冲区中可读的数据已经读取出来 这里肯定要对缓冲区进行复位操作
        /*
            关于为什么需要复原
            假设len=5，执行std::string result(peek(),len);后，result读取到了Hello，此时的buff内部和开始一样，readerIndex_无变化
            [预备空间] [H e l l o W o r l d] [可写空间]
                       ↑                       ↑
                   readerIndex_ = 0        writerIndex_ = 10
            这样会导致下次读取的时候，仍然从0开始读，也就是"H"，所以需要复原，将readerIndex_前进5位
            执行retrieve(len);后
            [预备空间] [H e l l o W o r l d] [可写空间]
                      ↑           ↑
                  readerIndex_ = 5  writerIndex_ = 10
        */
        return result;
    }

    // buffer_.size - writerIndex_
    void ensurewritablebytes(size_t len){
        if(len>writableBytes()){
            makeSpace(len); // 扩容
        }
    }

    // 把[data, data+len]内存上的数据添加到writable缓冲区当中
    void append(const char * data,size_t len){
        ensurewritablebytes(len);
        std::copy(data,data+len,beginWrite());
        writerIndex_+=len;
    }
    void append(const std::string& str)
    {
        append(str.data(), str.size());
    }

    char *beginWrite() { return begin() + writerIndex_; }
    const char *beginWrite() const { return begin() + writerIndex_; }

    // 从fd上读取数据
    ssize_t readFd(int fd, int *saveErrno);
    // 通过fd发送数据
    ssize_t writeFd(int fd, int *saveErrno);

    /////////////////////////////////chenshuo/////////////////////////////
    const char* findCRLF() const
    {
        // FIXME: replace with memmem()?
        const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF+2);
        return crlf == beginWrite() ? NULL : crlf;
    }

    void retrieveUntil(const char* end)
    {
        assert(peek() <= end);
        assert(end <= beginWrite());
        retrieve(end - peek());
    }
private:

    // vector底层数组首元素的地址 也就是数组的起始地址
    char *begin() { return &*buffer_.begin(); }
    const char *begin() const { return &*buffer_.begin(); }

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;

    void makeSpace(size_t len){
        /**
        * | kCheapPrepend |xxx| reader | writer |                     // xxx标示reader中已读的部分
        * | kCheapPrepend | reader ｜          len          |
        **/
       if(readerIndex_+writableBytes()<len+kCheapPrepend){
        buffer_.resize(writerIndex_+len);
       }else {// 这里说明 len <= xxx + writer 把reader搬到从xxx开始 使得xxx后面是一段连续空间
            size_t readable=readableBytes();// readable = reader的长度
            // 将当前缓冲区中从readerIndex_到writerIndex_的数据
            // 拷贝到缓冲区起始位置kCheapPrepend处，以便腾出更多的可写空间
            std::copy(buffer_.begin()+readerIndex_,
                      buffer_.begin()+writerIndex_, 
                      buffer_.begin()+kCheapPrepend);

            readerIndex_=kCheapPrepend;
            writerIndex_=readerIndex_+readable;
       }
    }

    /////////////////////////////////chenshuo/////////////////////////////
    static const char kCRLF[];
};

} // namespace muduo

/*https://zhuanlan.zhihu.com/p/495016351
 *3.5.2 Buffer图更清晰
 *
 * 缓冲区状态示意图（8字节空间示例）：
 *
 * 1. 刚创建时的状态：
 *    prependable bytes | 空闲空间（可写缓冲区）
 *    0                 readerIndex_ = writerIndex_ = 0, size_ = 8
 *
 * 2. 向里面写入数据：
 *    prependable bytes | （可读缓冲区）数据 | （可写缓冲区）空闲空间
 *                      readerIndex_不变，writerIndex_后移
 *
 * 3. 从里面取出一部分数据：
 *    prependable bytes | （可读缓冲区）剩余数据 | （可写缓冲区）空闲空间
 *                      readerIndex_后移，writerIndex_不变
 *
 * 4. 向里面写入数据，但可写缓冲区空间不足：
 *    如果 prependable bytes + 可读数据长度足够容纳新数据，
 *    则将可读数据前移，释放更多可写空间：
 *        prependable bytes | （可读缓冲区）数据 | 空闲空间
 *        readerIndex_调整为0，writerIndex_调整为数据长度
 *
 * 5. 继续写入数据，若总容量仍不足，则扩容：
 *        prependable bytes | （可读缓冲区）数据 | 扩容后的新空间
 *        readerIndex_保持不变（或0），writerIndex_后移，size_增大
 */
