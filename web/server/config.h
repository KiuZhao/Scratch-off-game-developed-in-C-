#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>
#include <string>
#include <cstdlib>

// ── 服务器 ──────────────────────────
constexpr int    SERVER_PORT       = 8080;
constexpr int    MAX_CONNECTIONS   = 4096;
constexpr int    RING_BUF_SIZE     = 65536;   // 64KB
constexpr int    MAX_HEADER_SIZE   = 8192;
constexpr int    MAX_BODY_SIZE     = 65536;
constexpr int    WORKER_THREADS    = 4;
constexpr int    MAX_EVENTS        = 1024;
constexpr int    EPOLL_TIMEOUT_MS  = 1000;

// ── MySQL 连接池 ────────────────────
constexpr const char* MYSQL_HOST = "127.0.0.1";
constexpr int         MYSQL_PORT = 3306;
constexpr const char* MYSQL_USER = "root";
constexpr const char* MYSQL_PASS = "YOUR_MYSQL_PASSWORD";
constexpr const char* MYSQL_DB   = "guaguale";
constexpr int         DB_POOL_SIZE = 4;

// ── Redis ───────────────────────────
constexpr const char* REDIS_HOST = "127.0.0.1";
constexpr int         REDIS_PORT = 6379;

// ── JWT ─────────────────────────────
inline std::string getJwtSecret() {
    const char* env = std::getenv("JWT_SECRET");
    return env ? std::string(env) : "change_me_in_production";
}
constexpr int JWT_EXPIRE_SEC = 86400;  // 24h

// ── 初始资金 ────────────────────────
constexpr int64_t INITIAL_BALANCE = 1000000;  // 100万方斯

// ── 排行榜 ──────────────────────────
constexpr int RANKING_TOP_N = 100;
constexpr const char* REDIS_KEY_PROFIT = "ranking:profit";
constexpr const char* REDIS_KEY_LOSS   = "ranking:loss";

// ── 速率限制 (令牌桶) ────────────────
constexpr double RATE_LIMIT_RPS   = 10.0;   // 每秒请求数
constexpr double RATE_LIMIT_BURST = 20.0;   // 突发容量

#endif
