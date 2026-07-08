#ifndef HANDLERS_H
#define HANDLERS_H

#include "http.h"
#include "db.h"
#include "rate_limiter.h"
#include <string>
#include <random>

class Router {
public:
    Router(MySQLPool* pool, Redis* redis, RateLimiter* rl);
    HttpResponse route(const HttpRequest& req, const std::string& clientIp,
                       MySQLConn* db);

    // 卡片类型元数据 (前后端同步)
    static std::string getCardTypesJson();

private:
    MySQLPool*   m_pool;
    Redis*       m_redis;
    RateLimiter* m_rl;

    int64_t authenticate(const HttpRequest& req);
    std::string generateToken(int64_t uid, const std::string& username);
    bool verifyToken(const std::string& token, int64_t& uid, std::string& uname);

    HttpResponse handleRegister(const HttpRequest& req, MySQLConn* db);
    HttpResponse handleLogin(const HttpRequest& req, const std::string& ip, MySQLConn* db);
    HttpResponse handleMe(const HttpRequest& req, MySQLConn* db);
    HttpResponse handleBuy(const HttpRequest& req, const std::string& ip, MySQLConn* db);
    HttpResponse handleCheat(const HttpRequest& req, const std::string& ip, MySQLConn* db);
    HttpResponse handleRanking(const std::string& qs);
    HttpResponse handleSouvenirs(const HttpRequest& req, MySQLConn* db);
    HttpResponse handleCardTypes();
    HttpResponse handleOptions();

    double calcWinProb(int cost, int pos, int maxPrize);
    std::mt19937 m_rng;
    std::uniform_real_distribution<double> m_probDist{0.0,1.0};
};

#endif
