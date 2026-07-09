#ifndef TMS_CLIENT_H
#define TMS_CLIENT_H

#include <string>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

// ── TC3-HMAC-SHA256 签名 + HTTPS 调用腾讯云 TMS ──

struct TmsResult {
    bool   ok;            // API 调用是否成功
    bool   blocked;       // 是否建议拦截
    std::string label;    // 风险标签 (Normal/Porn/Abuse/Ad/Illegal/Terror)
    int    score;         // 置信度 0-100
    std::string keywords; // 命中关键词, 逗号分隔
};

// Base64 编码
static std::string base64Encode(const std::string& data) {
    static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((data.length() + 2) / 3 * 4);
    int val = 0, valb = -6;
    for (uint8_t c : data) { val = (val << 8) + c; valb += 8;
        while (valb >= 0) { out += tbl[(val >> valb) & 0x3F]; valb -= 6; } }
    if (valb > -6) out += tbl[((val << 8) >> (valb + 8)) & 0x3F];
    while (out.length() % 4) out += '=';
    return out;
}

// SHA256 十六进制
static std::string sha256Hex(const std::string& data) {
    uint8_t hash[SHA256_DIGEST_LENGTH];
    SHA256((const uint8_t*)data.c_str(), data.length(), hash);
    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        oss << std::hex << std::setfill('0') << std::setw(2) << (int)hash[i];
    return oss.str();
}

// HMAC-SHA256 十六进制
static std::string hmacSha256Hex(const std::string& key, const std::string& msg) {
    uint8_t result[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    HMAC(EVP_sha256(), key.c_str(), (int)key.length(),
         (const uint8_t*)msg.c_str(), msg.length(), result, &len);
    std::ostringstream oss;
    for (unsigned int i = 0; i < len; ++i)
        oss << std::hex << std::setfill('0') << std::setw(2) << (int)result[i];
    return oss.str();
}

// 简易 HTTPS POST
static std::string httpsPost(const std::string& host, const std::string& path,
                              const std::string& body, const std::string& auth,
                              const std::string& action, const std::string& timestamp) {
    // 解析域名
    struct addrinfo hints = {}, *res;
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), "443", &hints, &res) != 0) return "";

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) { freeaddrinfo(res); return ""; }

    struct timeval tv = {2, 0};  // 2秒超时
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        close(fd); freeaddrinfo(res); return "";
    }
    freeaddrinfo(res);

    // SSL
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, fd);
    if (SSL_connect(ssl) <= 0) {
        SSL_free(ssl); SSL_CTX_free(ctx); close(fd); return "";
    }

    // 构建 HTTP 请求
    std::ostringstream req;
    req << "POST " << path << " HTTP/1.1\r\n"
        << "Host: " << host << "\r\n"
        << "Content-Type: application/json\r\n"
        << "X-TC-Action: " << action << "\r\n"
        << "X-TC-Timestamp: " << timestamp << "\r\n"
        << "Authorization: " << auth << "\r\n"
        << "Content-Length: " << body.length() << "\r\n"
        << "Connection: close\r\n\r\n"
        << body;

    std::string reqStr = req.str();
    SSL_write(ssl, reqStr.c_str(), (int)reqStr.length());

    // 读取响应
    std::string response;
    char buf[4096];
    int n;
    while ((n = SSL_read(ssl, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = 0;
        response += buf;
    }

    SSL_shutdown(ssl); SSL_free(ssl); SSL_CTX_free(ctx); close(fd);

    // 剥离 HTTP header, 取 body
    size_t bodyStart = response.find("\r\n\r\n");
    if (bodyStart == std::string::npos) return "";
    return response.substr(bodyStart + 4);
}

// ── TMS 客户端 ───────────────────────
class TmsClient {
public:
    // 审核用户名, 返回结果
    static TmsResult checkUsername(const std::string& username) {
        TmsResult result = {false, false, "Normal", 0, ""};

        const char* envId  = std::getenv("TENCENT_SECRET_ID");
        const char* envKey = std::getenv("TENCENT_SECRET_KEY");
        if (!envId || !envKey) {
            fprintf(stderr, "TMS: TENCENT_SECRET_ID/KEY not set\n");
            return result;  // ok=false, 调用方拒绝
        }
        std::string secretId(envId), secretKey(envKey);

        // 准备请求
        std::string content = base64Encode(username);
        std::string body = "{\"Content\":\"" + content + "\"}";

        // TC3-HMAC-SHA256 签名
        std::string host    = "tms.tencentcloudapi.com";
        std::string service = "tms";
        std::string action  = "TextModeration";
        std::string version = "2020-12-29";
        std::string algorithm = "TC3-HMAC-SHA256";

        int64_t ts = (int64_t)time(nullptr);
        std::string timestamp = std::to_string(ts);

        // 日期格式: 2026-07-08
        time_t t = (time_t)ts;
        struct tm* gmt = gmtime(&t);
        char dateBuf[16];
        strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", gmt);
        std::string date(dateBuf);

        // 1. Canonical Request
        std::string httpMethod  = "POST";
        std::string canonicalUri = "/";
        std::string canonicalQuery = "";
        std::string canonicalHeaders =
            "content-type:application/json\n"
            "host:" + host + "\n"
            "x-tc-action:" + action + "\n";
        std::string signedHeaders = "content-type;host;x-tc-action";
        std::string hashedPayload = sha256Hex(body);

        std::ostringstream canonicalReq;
        canonicalReq << httpMethod << "\n"
                     << canonicalUri << "\n"
                     << canonicalQuery << "\n"
                     << canonicalHeaders << "\n"
                     << signedHeaders << "\n"
                     << hashedPayload;

        // 2. String to Sign
        std::string credentialScope = date + "/" + service + "/tc3_request";
        std::string hashedCanonical = sha256Hex(canonicalReq.str());

        std::ostringstream strToSign;
        strToSign << algorithm << "\n"
                  << timestamp << "\n"
                  << credentialScope << "\n"
                  << hashedCanonical;

        // 3. Signature
        std::string secretDate    = hmacSha256Hex("TC3" + secretKey, date);
        std::string secretService = hmacSha256Hex(secretDate, service);
        std::string secretSigning = hmacSha256Hex(secretService, "tc3_request");
        std::string signature     = hmacSha256Hex(secretSigning, strToSign.str());

        // 4. Authorization header
        std::ostringstream auth;
        auth << algorithm << " Credential=" << secretId << "/" << credentialScope
             << ", SignedHeaders=" << signedHeaders
             << ", Signature=" << signature;
        std::string authStr = auth.str();

        // 5. 发送 HTTPS 请求
        std::string respBody = httpsPost(host, canonicalUri, body, authStr, action, timestamp);
        if (respBody.empty()) {
            fprintf(stderr, "TMS: API call failed (network/timeout)\n");
            return result;  // ok=false → 拒绝
        }

        // 6. 解析 JSON 响应 (手写, 避免引入 json.hpp 依赖)
        result.ok = true;

        // 提取 Suggestion
        auto extractStr = [&](const char* key) -> std::string {
            std::string search = std::string("\"") + key + "\":\"";
            size_t p = respBody.find(search);
            if (p == std::string::npos) return "";
            p += search.length();
            size_t e = respBody.find('"', p);
            return respBody.substr(p, e - p);
        };
        auto extractInt = [&](const char* key) -> int {
            std::string search = std::string("\"") + key + "\":";
            size_t p = respBody.find(search);
            if (p == std::string::npos) return 0;
            p += search.length();
            return (int)strtol(respBody.c_str() + p, nullptr, 10);
        };

        std::string suggestion = extractStr("Suggestion");
        result.label    = extractStr("Label");
        result.score    = extractInt("Score");
        result.blocked  = (suggestion == "Block" || suggestion == "Review");

        // 提取命中关键词
        std::string kwStr;
        size_t kwStart = respBody.find("\"Keywords\":[");
        if (kwStart != std::string::npos) {
            kwStart += 12;  // skip "Keywords":[
            size_t kwEnd = respBody.find(']', kwStart);
            if (kwEnd != std::string::npos) {
                std::string kwBlock = respBody.substr(kwStart, kwEnd - kwStart);
                // 提取每个引号内的关键词
                size_t pos = 0;
                while ((pos = kwBlock.find('"', pos)) != std::string::npos) {
                    pos++;
                    size_t e = kwBlock.find('"', pos);
                    if (e == std::string::npos) break;
                    if (!kwStr.empty()) kwStr += ",";
                    kwStr += kwBlock.substr(pos, e - pos);
                    pos = e + 1;
                }
            }
        }
        result.keywords = kwStr;

        if (result.blocked)
            fprintf(stderr, "TMS: BLOCK username='%s' label=%s score=%d keywords=%s\n",
                    username.c_str(), result.label.c_str(), result.score, result.keywords.c_str());

        return result;
    }
};

#endif
