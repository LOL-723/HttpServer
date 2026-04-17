#pragma once
#include "SslTypes.h"
#include <string>

namespace ssl {

// 客户端验证模式（比 bool 更专业）
enum class VerifyMode
{
    NONE,               // 不验证客户端
    PEER,               // 验证客户端证书（但不强制）
    REQUIRE_PEER_CERT   // 必须提供客户端证书
};

class SslConfig
{
public:
    SslConfig();

    ~SslConfig() = default;

    // =========================
    // 证书配置
    // =========================
    void setCertificateFile(const std::string& certFile) { certFile_ = certFile; }
    void setPrivateKeyFile(const std::string& keyFile) { keyFile_ = keyFile; }

    // 可选：单独链文件（如果不用 fullchain.pem）
    void setCertificateChainFile(const std::string& chainFile) { chainFile_ = chainFile; }


    // =========================
    // 加密套件配置
    // =========================
    // TLS 1.3 专用
    void setCipherSuites(const std::string& cipherSuites) { cipherSuites_ = cipherSuites; }

    // =========================
    // 客户端验证配置
    // =========================
    void setVerifyMode(VerifyMode mode) { verifyMode_ = mode; }
    void setVerifyDepth(int depth) { verifyDepth_ = depth; }

    // =========================
    // 会话配置
    // =========================
    void setSessionTimeout(int seconds) { sessionTimeout_ = seconds; }
    void setSessionCacheSize(long size) { sessionCacheSize_ = size; }

    // =========================
    // Getters
    // =========================
    const std::string& getCertificateFile() const { return certFile_; }
    const std::string& getPrivateKeyFile() const { return keyFile_; }
    const std::string& getCertificateChainFile() const { return chainFile_; }


    const std::string& getCipherSuites() const { return cipherSuites_; }

    VerifyMode getVerifyMode() const { return verifyMode_; }
    int getVerifyDepth() const { return verifyDepth_; }

    int getSessionTimeout() const { return sessionTimeout_; }
    long getSessionCacheSize() const { return sessionCacheSize_; }

private:
    // =========================
    // 证书
    // =========================
    std::string certFile_;     // 建议：fullchain.pem
    std::string keyFile_;      // 私钥
    std::string chainFile_;    // 可选链文件


    // =========================
    // 加密套件
    // =========================
    std::string cipherSuites_;  // TLS 1.3

    // =========================
    // 验证
    // =========================
    VerifyMode verifyMode_;
    int verifyDepth_;

    // =========================
    // 会话
    // =========================
    int  sessionTimeout_;
    long sessionCacheSize_;
};

} // namespace ssl