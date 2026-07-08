#ifndef SERVER_H
#define SERVER_H

#include "config.h"
#include "ring_buffer.h"
#include "http.h"
#include "handlers.h"
#include "rate_limiter.h"
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <vector>
#include <string>

struct Connection {
    int          fd;
    RingBuffer   readBuf;
    RingBuffer   writeBuf;
    HttpParser   parser;
    bool         closeAfterWrite = false;
    int64_t      lastActivity;
    std::string  clientIp;
    // 固定解析缓冲区 (每连接复用, 零分配)
    char         tmpBuf[MAX_BODY_SIZE];
};

class HttpServer {
public:
    HttpServer(MySQLPool* pool, Redis* redis);
    ~HttpServer();
    bool start(int port);
    void run();

private:
    MySQLPool*  m_pool;
    Redis*      m_redis;
    RateLimiter m_rl;
    Router     m_router;
    int        m_listenFd = -1;
    int        m_epollFd  = -1;
    std::atomic<bool> m_running{true};
    std::vector<std::thread> m_workers;
    std::vector<Connection*> m_closed;  // 待回收连接

    void workerLoop(int workerId);
    void handleRead(Connection* conn);
    void handleWrite(Connection* conn);
    void closeConn(Connection* conn);
    void serveStatic(Connection* conn, const std::string& path);
    static void setNonBlocking(int fd);

    static std::string getMime(const std::string& p);
    static std::string readFile(const std::string& path);
};

#endif
