#include "../../../include/httpserver/ssl/SslContext.h"
#include <muduo/Logger.h>
#include <openssl/err.h>

namespace ssl
{
SslContext::SslContext(const SslConfig& config)
    : ctx_(nullptr)
    , config_(config)
{

}

SslContext::~SslContext()
{
    if (ctx_)
    {
        SSL_CTX_free(ctx_);
    }
}

bool SslContext::initialize()
{
    OPENSSL_init_ssl(
        OPENSSL_INIT_LOAD_SSL_STRINGS |
        OPENSSL_INIT_LOAD_CRYPTO_STRINGS,
        nullptr
    );

    ctx_ = SSL_CTX_new(TLS_server_method());
    if (!ctx_) {
        handleSslError("Failed to create SSL context");
        return false;
    }

    // TLS 1.3 ONLY
    SSL_CTX_set_min_proto_version(ctx_, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(ctx_, TLS1_3_VERSION);
    // TLS 1.3 cipher suites
    if (!config_.getCipherSuites().empty()) {
        if (SSL_CTX_set_ciphersuites(
                ctx_,
                config_.getCipherSuites().c_str()
            ) <= 0)
        {
            handleSslError("Failed to set cipher suites");
            return false;
        }
    }

    // certificates
    if (!loadCertificates()) {
        return false;
    }

    setupSessionCache();

    LOG_INFO("SSL context initialized successfully (TLS 1.3 only)");
    return true;
}

bool SslContext::loadCertificates()
{
    LOG_INFO("Loading server certificate from %s", config_.getCertificateFile().c_str());
    // 加载证书
    if (SSL_CTX_use_certificate_file(ctx_,
     config_.getCertificateFile().c_str(), SSL_FILETYPE_PEM) <= 0)
    {
        handleSslError("Failed to load server certificate");
        return false;
    }

    LOG_INFO("Loading private key from %s", config_.getPrivateKeyFile().c_str());
    // 加载私钥
    if (SSL_CTX_use_PrivateKey_file(ctx_, 
        config_.getPrivateKeyFile().c_str(), SSL_FILETYPE_PEM) <= 0)
    {
        handleSslError("Failed to load private key");
        return false;
    }

    // 验证私钥
    if (!SSL_CTX_check_private_key(ctx_))
    {
        handleSslError("Private key does not match the certificate");
        return false;
    }

    // 加载证书链
    if (!config_.getCertificateChainFile().empty())
    {
        if (SSL_CTX_use_certificate_chain_file(ctx_,
            config_.getCertificateChainFile().c_str()) <= 0)
        {
            handleSslError("Failed to load certificate chain");
            return false;
        }
    }

    return true;
}

void SslContext::setupSessionCache()
{
    SSL_CTX_set_session_cache_mode(ctx_, SSL_SESS_CACHE_SERVER);
    SSL_CTX_sess_set_cache_size(ctx_, config_.getSessionCacheSize());
    SSL_CTX_set_timeout(ctx_, config_.getSessionTimeout());
}

void SslContext::handleSslError(const char* msg)
{
    char buf[256];
    ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
    LOG_ERROR ("%s:%s",msg,buf) ;
}

}; // namespace ssl
