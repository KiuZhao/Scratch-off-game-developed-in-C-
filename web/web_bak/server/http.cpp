#include "http.h"
#include <sstream>
#include <algorithm>

void HttpParser::reset() {
    m_state = REQ_LINE; m_contentLen = 0; m_headerBytes = 0;
    m_headerBuf.clear(); m_req.clear();
}

size_t HttpParser::parse(const char* data, size_t len) {
    size_t consumed = 0;

    while (consumed < len && m_state != COMPLETE && m_state != ERROR) {
        if (m_state == REQ_LINE || m_state == HEADERS) {
            while (consumed < len) {
                char c = data[consumed++];
                m_headerBuf += c;
                m_headerBytes++;

                if (m_headerBytes > MAX_HEADER_SIZE) { m_state = ERROR; return consumed; }

                size_t hl = m_headerBuf.length();
                if (hl >= 4 && m_headerBuf[hl-4]=='\r' && m_headerBuf[hl-3]=='\n' &&
                    m_headerBuf[hl-2]=='\r' && m_headerBuf[hl-1]=='\n') {
                    std::istringstream iss(m_headerBuf);
                    std::string line; bool first = true;
                    while (std::getline(iss, line)) {
                        if (!line.empty() && line.back()=='\r') line.pop_back();
                        if (line.empty()) continue;
                        if (first) {
                            if (!parseRequestLine(line)) { m_state = ERROR; return consumed; }
                            first = false;
                        } else {
                            if (!parseHeaderLine(line)) { m_state = ERROR; return consumed; }
                        }
                    }
                    auto it = m_req.headers.find("content-length");
                    m_contentLen = (it != m_req.headers.end()) ? (size_t)std::stoll(it->second) : 0;
                    if (m_contentLen > 0 && m_contentLen <= MAX_BODY_SIZE)
                        m_state = BODY;
                    else
                        m_state = COMPLETE;
                    break;
                }
            }
        } else if (m_state == BODY) {
            size_t remaining = m_contentLen - m_req.body.length();
            size_t take = std::min(remaining, len - consumed);
            m_req.body.append(data + consumed, take);
            consumed += take;
            if (m_req.body.length() >= m_contentLen) m_state = COMPLETE;
        }
    }
    return consumed;
}

bool HttpParser::parseRequestLine(const std::string& line) {
    std::istringstream iss(line);
    return (iss >> m_req.method >> m_req.path >> m_req.version) ? true : false;
}

bool HttpParser::parseHeaderLine(const std::string& line) {
    size_t colon = line.find(':');
    if (colon == std::string::npos) return true;
    std::string key = line.substr(0, colon);
    std::string val = line.substr(colon + 1);
    size_t s = val.find_first_not_of(" \t");
    if (s != std::string::npos) val = val.substr(s);
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    m_req.headers[key] = val;
    return true;
}

// ── 响应构建 ────────────────────────
HttpResponse& HttpResponse::status(int code, const char* msg) { m_code=code; m_msg=msg; return *this; }
HttpResponse& HttpResponse::header(const char* k, const char* v) { m_headers.push_back({k,v}); return *this; }

HttpResponse& HttpResponse::json(const std::string& body) {
    m_body = body; header("Content-Type", "application/json; charset=utf-8"); return *this;
}
HttpResponse& HttpResponse::text(const std::string& body) {
    m_body = body; header("Content-Type", "text/html; charset=utf-8"); return *this;
}
HttpResponse& HttpResponse::setCookie(const char* k, const char* v) {
    header("Set-Cookie", (std::string(k)+"="+v+"; Path=/; HttpOnly; Max-Age=86400; SameSite=Lax").c_str());
    return *this;
}

std::string HttpResponse::build() const {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << m_code << " " << m_msg << "\r\n";
    oss << "Content-Length: " << m_body.length() << "\r\n";
    oss << "Connection: keep-alive\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    oss << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    for (auto& h : m_headers) oss << h.first << ": " << h.second << "\r\n";
    oss << "\r\n" << m_body;
    return oss.str();
}
