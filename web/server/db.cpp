#include "db.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <thread>
#include <chrono>

// =====================================================================
//  MySQLConn
// =====================================================================

bool MySQLConn::connect() {
    // 如果是重连，先关闭旧连接
    if (m_conn) { mysql_close(m_conn); m_conn = nullptr; }
    m_conn = mysql_init(nullptr);
    if (!mysql_real_connect(m_conn, MYSQL_HOST, MYSQL_USER, MYSQL_PASS,
                             MYSQL_DB, MYSQL_PORT, nullptr, 0)) {
        fprintf(stderr, "MySQL connect: %s\n", mysql_error(m_conn));
        return false;
    }
    mysql_set_character_set(m_conn, "utf8mb4");
    return true;
}

bool MySQLConn::ping() {
    if (!m_conn) return 1;
    return mysql_ping(m_conn) == 0;
}

std::string MySQLConn::vformat(const char* fmt, va_list ap) {
    va_list ap2; va_copy(ap2, ap);
    int len = vsnprintf(nullptr, 0, fmt, ap2); va_end(ap2);
    if (len < 0) return "";
    std::string buf(len+1, '\0');
    vsnprintf(&buf[0], len+1, fmt, ap); buf.resize(len);
    return buf;
}

std::vector<MySQLRow> MySQLConn::query(const char* sql, ...) {
    std::vector<MySQLRow> rows;
    va_list ap; va_start(ap, sql);
    std::string q = vformat(sql, ap);
    va_end(ap);
    if (mysql_query(m_conn, q.c_str()) != 0) {
        fprintf(stderr, "MySQL query: %s\nSQL: %s\n", mysql_error(m_conn), q.c_str());
        return rows;
    }
    MYSQL_RES* res = mysql_store_result(m_conn);
    if (!res) return rows;
    unsigned ncols = mysql_num_fields(res);
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        MySQLRow r;
        for (unsigned i=0; i<ncols; ++i) r.cols.push_back(row[i]?row[i]:"");
        rows.push_back(std::move(r));
    }
    mysql_free_result(res);
    return rows;
}

int MySQLConn::execute(const char* sql, ...) {
    va_list ap; va_start(ap, sql);
    std::string q = vformat(sql, ap);
    va_end(ap);
    if (mysql_query(m_conn, q.c_str()) != 0) {
        fprintf(stderr, "MySQL exec: %s\nSQL: %s\n", mysql_error(m_conn), q.c_str());
        return -1;
    }
    return (int)mysql_affected_rows(m_conn);
}

uint64_t MySQLConn::lastInsertId() { return mysql_insert_id(m_conn); }

std::string MySQLConn::escape(const std::string& s) {
    if (!m_conn) return s;
    std::string out(s.length()*2+1, '\0');
    mysql_real_escape_string(m_conn, &out[0], s.c_str(), (unsigned long)s.length());
    out.resize(strlen(out.c_str()));
    return out;
}

// =====================================================================
//  MySQLPool
// =====================================================================

MySQLPool::MySQLPool(int size) {
    for (int i = 0; i < size; ++i) {
        auto* conn = new MySQLConn();
        if (conn->connect()) {
            m_pool.push(conn);
        } else {
            delete conn;
        }
    }
    printf("MySQL pool: %zu/%d connected\n", m_pool.size(), size);
}

MySQLPool::~MySQLPool() {
    while (!m_pool.empty()) { delete m_pool.front(); m_pool.pop(); }
}

MySQLConn* MySQLPool::acquire() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this]{ return !m_pool.empty(); });
    auto* conn = m_pool.front(); m_pool.pop();
    // 空闲超时断开后自动重连
    if (conn->ping() != 0) {
        fprintf(stderr, "MySQL reconnect...\n");
        conn->connect();
    }
    return conn;
}

void MySQLPool::release(MySQLConn* conn) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_pool.push(conn);
    m_cv.notify_one();
}

bool MySQLPool::allConnected() { return !m_pool.empty(); }

// =====================================================================
//  Redis
// =====================================================================

Redis::Redis() {}
Redis::~Redis() { if (m_ctx) redisFree(m_ctx); }

bool Redis::connect() {
    m_ctx = redisConnect(REDIS_HOST, REDIS_PORT);
    if (!m_ctx || m_ctx->err) {
        fprintf(stderr, "Redis: %s\n", m_ctx?m_ctx->errstr:"unknown");
        if (m_ctx) { redisFree(m_ctx); m_ctx = nullptr; }
        return false;
    }
    return true;
}

bool Redis::zadd(const char* key, int64_t score, const char* member) {
    if (!m_ctx) return false;
    auto* r = (redisReply*)redisCommand(m_ctx, "ZADD %s %lld %s", key, (long long)score, member);
    bool ok = r && r->type != REDIS_REPLY_ERROR;
    if (r) freeReplyObject(r);
    return ok;
}

std::vector<std::pair<std::string,int64_t>> Redis::zrevrange(const char* key, int start, int stop) {
    std::vector<std::pair<std::string,int64_t>> res;
    if (!m_ctx) return res;
    auto* r = (redisReply*)redisCommand(m_ctx, "ZREVRANGE %s %d %d WITHSCORES", key, start, stop);
    if (r && r->type == REDIS_REPLY_ARRAY) {
        for (size_t i=0; i+1<r->elements; i+=2)
            res.push_back({r->element[i]->str, (int64_t)strtoll(r->element[i+1]->str,nullptr,10)});
    }
    if (r) freeReplyObject(r);
    return res;
}

std::vector<std::pair<std::string,int64_t>> Redis::zrange(const char* key, int start, int stop) {
    std::vector<std::pair<std::string,int64_t>> res;
    if (!m_ctx) return res;
    auto* r = (redisReply*)redisCommand(m_ctx, "ZRANGE %s %d %d WITHSCORES", key, start, stop);
    if (r && r->type == REDIS_REPLY_ARRAY) {
        for (size_t i=0; i+1<r->elements; i+=2)
            res.push_back({r->element[i]->str, (int64_t)strtoll(r->element[i+1]->str,nullptr,10)});
    }
    if (r) freeReplyObject(r);
    return res;
}

bool Redis::zset(const char* key, int64_t score, const char* member) {
    return zadd(key, score, member);
}
