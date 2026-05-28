#include "../../../include/httpserver/ssl/SslConnection.h"
#include <muduo/Logger.h>
#include <openssl/err.h>
#include <string>

namespace ssl
{

// 自定义 BIO 方法
static BIO_METHOD* createCustomBioMethod() 
{
    BIO_METHOD*method=BIO_meth_new(BIO_TYPE_MEM, "custom");
    BIO_meth_set_read(method, SslConnection::bioRead);
    BIO_meth_set_write(method, SslConnection::bioWrite);
    BIO_meth_set_ctrl(method, SslConnection::bioCtrl);
    return method;
}

SslConnection::SslConnection(const TcpConnectionPtr& conn, SslContext* ctx)
    : ssl_(nullptr)
    , ctx_(ctx)
    , conn_(conn)
    , state_(SSLState::HANDSHAKING)
    , readBio_(nullptr)
    , writeBio_(nullptr)
    , messageCallback_(nullptr)
{
    // 创建 SSL 对象
    ssl_=SSL_new(ctx->getNativeHandle());
    if (!ssl_) {
        LOG_ERROR("Failed to create SSL object: %s", ERR_error_string(ERR_get_error(), nullptr));
        return;
    }

    // 创建 BIO
    readBio_=BIO_new(BIO_s_mem());
    writeBio_=BIO_new(BIO_s_mem());
    if(!readBio_ || !writeBio_){
        SSL_set_accept_state(ssl_);  // 设置为服务器模式
        SSL_free(ssl_);
        ssl_=nullptr;
        return;
    }
    SSL_set_bio(ssl_, readBio_, writeBio_);
    SSL_set_accept_state(ssl_);
    // 设置 SSL 选项
    SSL_set_mode(ssl_, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    SSL_set_mode(ssl_,SSL_MODE_ENABLE_PARTIAL_WRITE);
}

SslConnection::~SslConnection() 
{
    if (ssl_) 
    {
        SSL_free(ssl_);  // 这会同时释放 BIO
    }
}

void SslConnection::startHandshake() 
{
    SSL_set_accept_state(ssl_);
    handleHandshake();
}

void SslConnection::flushWriteBio()
{
    if (!writeBio_) {
        return;
    }

    char buf[4096];
    int pending = 0;
    while ((pending = BIO_pending(writeBio_)) > 0) {
        int bytes = BIO_read(writeBio_, buf, std::min(pending, static_cast<int>(sizeof(buf))));
        if (bytes <= 0) {
            break;
        }
        conn_->send(std::string(buf, bytes));
    }
}

void SslConnection::drainApplicationData(const TcpConnectionPtr& conn, muduo::Timestamp time)
{
    char decryptedData[4096];
    int ret = 0;

    decryptedBuffer_.retrieveAll();
    while ((ret = SSL_read(ssl_, decryptedData, sizeof(decryptedData))) > 0) {
        decryptedBuffer_.append(decryptedData, ret);
    }

    if (decryptedBuffer_.readableBytes() > 0 && messageCallback_) {
        messageCallback_(conn, &decryptedBuffer_, time);
    }

    int err = SSL_get_error(ssl_, ret);
    if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE && err != SSL_ERROR_ZERO_RETURN) {
        LOG_ERROR("SSL_read error: %d", err);
    }
}
/*        TCP socket (muduo)
*                ↓
*        Buffer (加密数据 TLS record)
*                ↓
*        BIO_write(readBio_)
*                ↓
*        [ OpenSSL 内部 ]
*        ┌────────────────────┐
*        │   TLS Record 层     │
*        │   解密 + 重组       │
*        └────────────────────┘
*                ↓
*        SSL_read()
*                ↓
*        decryptedData（明文）
*                ↓
*        muduo::Buffer
*                ↓
*        messageCallback（HTTP/RPC等）
*/
//发送数据
void SslConnection::send(const void* data, size_t len) 
{
    if (state_ != SSLState::ESTABLISHED) {
        LOG_ERROR("Cannot send data before SSL handshake is complete");
        return;
    }

    const char* current = static_cast<const char*>(data);
    size_t remaining = len;
    while (remaining > 0) {
        int writen = SSL_write(ssl_, current, remaining);
        if (writen <= 0) {
            int err = SSL_get_error(ssl_, writen);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                flushWriteBio();
                continue;
            }
            unsigned long errCode = ERR_get_error();
            LOG_ERROR("SSL_write failed: %s", ERR_error_string(errCode, nullptr));
            return;
        }

        current += writen;
        remaining -= writen;
        flushWriteBio();
    }
}

/*握手阶段
ClientHello (密文)
        ↓
BIO_write
        ↓
SSL_do_handshake()
        ↓
ServerHello / Certificate / Finished
        ↓
BIO_read(writeBio_)
        ↓
发送给客户端
        ↓
state = ESTABLISHED
*/
/*ESTABLISHED 阶段
[客户端发数据]
HTTP Request（明文）
        ↓
TLS 加密
        ↓
TCP 发过来（密文）
        ↓
你的 onRead()
        ↓
BIO_write（喂给SSL）
        ↓
SSL_read（解密）
        ↓
HTTP 明文
        ↓
messageCallback（进入业务层）*/

//读取数据
void SslConnection::onRead(const TcpConnectionPtr& conn, BufferPtr buf, 
                         muduo::Timestamp time) 
{
    //建立连接中，加密可读数据
    if(state_==SSLState::HANDSHAKING){
        int written=BIO_write(readBio_, buf->peek(),  buf->readableBytes());
        if(written>0){
            buf->retrieve(written);//移除处理完的数据
        }
        //完成握手
        handleHandshake();
        if (state_ == SSLState::ESTABLISHED) {
            drainApplicationData(conn, time);
        }
        return;
    }else if(state_==SSLState::ESTABLISHED){
        //完成握手已经建立连接后，将要解密的数据放到缓冲区中（此时数据处于加密状态）
        int written=BIO_write(readBio_, buf->peek(), buf->readableBytes());
        if(written>0){
            buf->retrieve(written);
        }
        drainApplicationData(conn, time);
    }
}

void SslConnection::handleHandshake() 
{
    int ret = SSL_do_handshake(ssl_);
    flushWriteBio();
    
    if (ret == 1) {
        state_ = SSLState::ESTABLISHED;
        LOG_INFO("SSL handshake completed successfully");
        LOG_INFO("Using cipher: %s", SSL_get_cipher(ssl_));
        LOG_INFO("Protocol version: %s", SSL_get_version(ssl_));
        
        // 握手完成后，确保设置了正确的回调
        if (!messageCallback_) {
            LOG_WARN("No message callback set after SSL handshake");
        }
        return;
    }
    
    int err = SSL_get_error(ssl_, ret);
    switch (err) {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            // 正常的握手过程，需要继续
            break;
            
        default: {
            // 获取详细的错误信息
            char errBuf[256];
            unsigned long errCode = ERR_get_error();
            ERR_error_string_n(errCode, errBuf, sizeof(errBuf));
            LOG_ERROR("SSL handshake failed: %s", errBuf);
            conn_->shutdown();  // 关闭连接
            break;
        }
    }
}

void SslConnection::onEncrypted(const char* data, size_t len) 
{
    conn_->send(std::string(data, len));
}

void SslConnection::onDecrypted(const char* data, size_t len) 
{
    decryptedBuffer_.append(data, len);
}

SSLError SslConnection::getLastError(int ret) 
{
    int err = SSL_get_error(ssl_, ret);
    switch (err) 
    {
        case SSL_ERROR_NONE:
            return SSLError::NONE;
        case SSL_ERROR_WANT_READ:
            return SSLError::WANT_READ;
        case SSL_ERROR_WANT_WRITE:
            return SSLError::WANT_WRITE;
        case SSL_ERROR_SYSCALL:
            return SSLError::SYSCALL;
        case SSL_ERROR_SSL:
            return SSLError::SSL;
        default:
            return SSLError::UNKNOWN;
    }
}

void SslConnection::handleError(SSLError error) 
{
    switch (error) 
    {
        case SSLError::WANT_READ:
        case SSLError::WANT_WRITE:
            // 需要等待更多数据或写入缓冲区可用
            break;
        case SSLError::SSL:
        case SSLError::SYSCALL:
        case SSLError::UNKNOWN:
            LOG_ERROR("SSL error occurred: %s", ERR_error_string(ERR_get_error(), nullptr));
            state_ = SSLState::ERROR;
            conn_->shutdown();
            break;
        default:
            break;
    }
}

int SslConnection::bioWrite(BIO* bio, const char* data, int len) 
{
    SslConnection* conn = static_cast<SslConnection*>(BIO_get_data(bio));
    if (!conn) return -1;

    conn->conn_->send(data, len);
    return len;
}

int SslConnection::bioRead(BIO* bio, char* data, int len) 
{
    SslConnection* conn = static_cast<SslConnection*>(BIO_get_data(bio));
    if (!conn) return -1;

    size_t readable = conn->readBuffer_.readableBytes();
    if (readable == 0) 
    {
        return -1;  // 无数据可读
    }

    size_t toRead = std::min(static_cast<size_t>(len), readable);
    memcpy(data, conn->readBuffer_.peek(), toRead);
    conn->readBuffer_.retrieve(toRead);
    return toRead;
}

long SslConnection::bioCtrl(BIO* bio, int cmd, long num, void* ptr) 
{
    switch (cmd) 
    {
        case BIO_CTRL_FLUSH:
            return 1;
        default:
            return 0;
    }
}


} // namespace ssl 
