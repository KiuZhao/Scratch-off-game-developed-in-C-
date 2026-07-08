#ifndef RATE_LIMITER_H
#define RATE_LIMITER_H

#include "config.h"
#include <unordered_map>
#include <string>
#include <mutex>
#include <chrono>

// ── 令牌桶 ───────────────────────────
struct TokenBucket {
    double tokens = RATE_LIMIT_BURST;
    std::chrono::steady_clock::time_point lastRefill;
};

// ── IP/用户 速率限制器 ───────────────
class RateLimiter {
public:
    // IP 级别 (默认 10 rps, 20 burst)
    bool allow(const std::string& key) {
        return allowWith(key, RATE_LIMIT_RPS, RATE_LIMIT_BURST);
    }

    // 用户级别 (可自定义速率, 如买卡限制)
    bool allowUser(const std::string& key, double rps, double burst) {
        return allowWith("uid:" + key, rps, burst);
    }

    // 定期清理过期条目
    void gc() {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto now = std::chrono::steady_clock::now();
        for (auto it = m_buckets.begin(); it != m_buckets.end(); ) {
            double elapsed = std::chrono::duration<double>(now - it->second.lastRefill).count();
            if (elapsed > 60.0 && it->second.tokens >= it->second.burst - 1.0)
                it = m_buckets.erase(it);
            else
                ++it;
        }
    }

private:
    struct Bucket {
        double tokens = RATE_LIMIT_BURST;
        double burst  = RATE_LIMIT_BURST;
        std::chrono::steady_clock::time_point lastRefill;
    };
    std::unordered_map<std::string, Bucket> m_buckets;
    std::mutex m_mutex;

    bool allowWith(const std::string& key, double rps, double burst) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& b = m_buckets[key];
        b.burst = burst;
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - b.lastRefill).count();
        b.tokens = std::min(burst, b.tokens + elapsed * rps);
        b.lastRefill = now;
        if (b.tokens >= 1.0) { b.tokens -= 1.0; return true; }
        return false;
    }
};

#endif
