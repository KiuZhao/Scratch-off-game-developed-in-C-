#ifndef DB_H
#define DB_H

#include "config.h"
#include <mysql/mysql.h>
#include <hiredis/hiredis.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <queue>
#include <condition_variable>

// ── MySQL 行 ─────────────────────────
struct MySQLRow {
    std::vector<std::string> cols;
    std::string& operator[](size_t i) { return cols[i]; }
    const std::string& operator[](size_t i) const { return cols[i]; }
    size_t size() const { return cols.size(); }
};

// ── 单个 MySQL 连接 ──────────────────
class MySQLConn {
public:
    MySQLConn()  { m_conn = mysql_init(nullptr); }
    ~MySQLConn() { if (m_conn) { mysql_close(m_conn); m_conn = nullptr; } }
    bool connect();
    bool ping();
    std::vector<MySQLRow> query(const char* sql, ...);
    int  execute(const char* sql, ...);
    uint64_t lastInsertId();
    std::string escape(const std::string& s);
private:
    MYSQL* m_conn = nullptr;
    std::string vformat(const char* fmt, va_list ap);
};

// ── MySQL 连接池 ─────────────────────
class MySQLPool {
public:
    MySQLPool(int size);
    ~MySQLPool();
    MySQLConn* acquire();       // 获取连接 (阻塞等待)
    void release(MySQLConn*);   // 归还连接
    bool allConnected();
private:
    std::queue<MySQLConn*> m_pool;
    std::mutex m_mutex;
    std::condition_variable m_cv;
};

// ── Redis ────────────────────────────
class Redis {
public:
    Redis();
    ~Redis();
    bool connect();
    bool isConnected() const { return m_ctx != nullptr; }

    bool zadd(const char* key, int64_t score, const char* member);
    std::vector<std::pair<std::string, int64_t>> zrevrange(const char* key, int start, int stop);
    std::vector<std::pair<std::string, int64_t>> zrange(const char* key, int start, int stop);
    bool zset(const char* key, int64_t score, const char* member);

private:
    redisContext* m_ctx = nullptr;
};

#endif
