#include "server.h"
#include <csignal>

std::atomic<bool> g_running{true};
void sigHandler(int) { g_running = false; }

int main() {
    signal(SIGINT, sigHandler);
    signal(SIGTERM, sigHandler);
    signal(SIGPIPE, SIG_IGN);

    MySQLPool pool(DB_POOL_SIZE);
    if (!pool.allConnected())
        fprintf(stderr, "Warning: MySQL pool not fully connected.\n");

    Redis redis;
    if (!redis.connect())
        fprintf(stderr, "Warning: Redis not available, rankings won't cache.\n");

    HttpServer server(&pool, &redis);
    if (!server.start(SERVER_PORT)) {
        fprintf(stderr, "Failed to start server.\n");
        return 1;
    }

    printf("Server running. Ctrl+C to stop.\n");
    server.run();
    printf("Server stopped.\n");
    return 0;
}
