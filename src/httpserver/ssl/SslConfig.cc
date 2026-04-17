#include "../../../include/httpserver/ssl/SslConfig.h"

namespace ssl
{
SslConfig::SslConfig()
    // TLS 1.3 cipher suites（建议补充）
    : cipherSuites_("TLS_AES_128_GCM_SHA256:TLS_AES_256_GCM_SHA384")

    , verifyMode_(VerifyMode::NONE)
    , verifyDepth_(4)

    , sessionTimeout_(300)

    // 单位：bytes（建议明确）
    , sessionCacheSize_(20 * 1024)
{
}

};