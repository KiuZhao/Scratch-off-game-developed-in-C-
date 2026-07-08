#ifndef HTTP_H
#define HTTP_H

#include "config.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <cstring>

// ── 简易 header map 扩展 ─────────────
struct HeaderMap : std::unordered_map<std::string, std::string> {
    std::string value_or(const char* key, const char* def) const {
        auto it = find(key);
        return it != end() ? it->second : std::string(def);
    }
};

// ── HTTP 请求 ────────────────────────
struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    HeaderMap   headers;
    std::string body;

    void clear() { method.clear(); path.clear(); version.clear(); headers.clear(); body.clear(); }
};

// ── HTTP 解析器 (固定缓冲区, 零分配) ──
class HttpParser {
public:
    enum State { REQ_LINE, HEADERS, BODY, COMPLETE, ERROR };

    HttpParser() = default;
    State state() const { return m_state; }
    const HttpRequest& request() const { return m_req; }
    void reset();

    // 解析: data+len 是已读入的数据, 返回消费字节数
    size_t parse(const char* data, size_t len);

private:
    State  m_state = REQ_LINE;
    HttpRequest m_req;
    size_t m_contentLen = 0;
    size_t m_headerBytes = 0;
    std::string m_headerBuf;  // 仅存头部, 解析完清空

    bool parseRequestLine(const std::string& line);
    bool parseHeaderLine(const std::string& line);
};

// ── HTTP 响应 ────────────────────────
class HttpResponse {
public:
    HttpResponse& status(int code, const char* msg);
    HttpResponse& header(const char* key, const char* val);
    HttpResponse& json(const std::string& body);
    HttpResponse& text(const std::string& body);
    HttpResponse& setCookie(const char* key, const char* val);
    std::string build() const;

    static HttpResponse ok()          { return HttpResponse().status(200,"OK"); }
    static HttpResponse created()     { return HttpResponse().status(201,"Created"); }
    static HttpResponse badRequest()  { return HttpResponse().status(400,"Bad Request"); }
    static HttpResponse unauthorized(){ return HttpResponse().status(401,"Unauthorized"); }
    static HttpResponse notFound()    { return HttpResponse().status(404,"Not Found"); }
    static HttpResponse serverError() { return HttpResponse().status(500,"Internal Server Error"); }

private:
    int m_code=200; std::string m_msg="OK", m_body;
    std::vector<std::pair<std::string,std::string>> m_headers;
};

#endif
