#include "server.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <arpa/inet.h>

extern std::atomic<bool> g_running;

// ── MIME ────────────────────────────
std::string HttpServer::getMime(const std::string& p) {
    if(p.ends_with(".html"))return "text/html; charset=utf-8";
    if(p.ends_with(".css"))return "text/css; charset=utf-8";
    if(p.ends_with(".js"))return "application/javascript; charset=utf-8";
    if(p.ends_with(".json"))return "application/json";
    if(p.ends_with(".png"))return "image/png";
    if(p.ends_with(".jpg")||p.ends_with(".jpeg"))return "image/jpeg";
    if(p.ends_with(".svg"))return "image/svg+xml";
    if(p.ends_with(".ico"))return "image/x-icon";
    return "application/octet-stream";
}

std::string HttpServer::readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if(!f)return "";
    std::ostringstream oss; oss<<f.rdbuf(); return oss.str();
}

void HttpServer::setNonBlocking(int fd) {
    int fl=fcntl(fd,F_GETFL,0); if(fl>=0)fcntl(fd,F_SETFL,fl|O_NONBLOCK);
}

// =====================================================================
HttpServer::HttpServer(MySQLPool* pool, Redis* redis)
    : m_pool(pool), m_redis(redis), m_router(pool, redis, &m_rl) {}

HttpServer::~HttpServer() {
    g_running = false;
    if(m_listenFd>=0)close(m_listenFd);
    if(m_epollFd>=0)close(m_epollFd);
    for(auto& t:m_workers)if(t.joinable())t.join();
    for(auto* c:m_closed)delete c;
}

bool HttpServer::start(int port) {
    m_listenFd = socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0);
    if(m_listenFd<0){perror("socket");return false;}
    int opt=1; setsockopt(m_listenFd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    sockaddr_in addr={}; addr.sin_family=AF_INET; addr.sin_addr.s_addr=INADDR_ANY;
    addr.sin_port=htons((uint16_t)port);
    if(bind(m_listenFd,(sockaddr*)&addr,sizeof(addr))<0){perror("bind");return false;}
    if(listen(m_listenFd,SOMAXCONN)<0){perror("listen");return false;}

    m_epollFd = epoll_create1(0);
    if(m_epollFd<0){perror("epoll_create1");return false;}

    epoll_event ev={}; ev.events=EPOLLIN; ev.data.fd=m_listenFd;
    epoll_ctl(m_epollFd,EPOLL_CTL_ADD,m_listenFd,&ev);

    for(int i=0;i<WORKER_THREADS;++i)
        m_workers.emplace_back(&HttpServer::workerLoop,this,i);

    printf("HTTP Server (epoll + %d workers) on port %d\n",WORKER_THREADS,port);
    return true;
}

void HttpServer::run() {
    // 主线程: accept + 分发到 epoll
    while(g_running) {
        epoll_event evs[MAX_EVENTS];
        int n=epoll_wait(m_epollFd,evs,MAX_EVENTS,100);
        for(int i=0;i<n&&g_running;++i){
            if(evs[i].data.fd==m_listenFd){
                while(true){
                    sockaddr_in cli={}; socklen_t len=sizeof(cli);
                    int fd=accept4(m_listenFd,(sockaddr*)&cli,&len,SOCK_NONBLOCK);
                    if(fd<0)break;
                    auto* conn=new Connection();
                    conn->fd=fd; conn->lastActivity=(int64_t)time(nullptr);
                    char ip[64]; inet_ntop(AF_INET,&cli.sin_addr,ip,sizeof(ip));
                    conn->clientIp=ip;
                    epoll_event ev={}; ev.events=EPOLLIN|EPOLLOUT|EPOLLET; ev.data.ptr=conn;
                    epoll_ctl(m_epollFd,EPOLL_CTL_ADD,fd,&ev);
                }
            }else{
                auto* conn=(Connection*)evs[i].data.ptr;
                if(evs[i].events&(EPOLLIN|EPOLLERR|EPOLLHUP))handleRead(conn);
                if(conn->fd>=0&&evs[i].events&EPOLLOUT)handleWrite(conn);
            }
        }
        m_rl.gc();
        // 回收已关闭的连接
        for(auto* c:m_closed)delete c;
        m_closed.clear();
    }
}

void HttpServer::workerLoop(int workerId) {
    (void)workerId; // worker 线程目前仅用于未来扩展; 主线程 epoll + 连接池已够用
    // 实际负载处理在 handleRead/handleWrite 中同步完成
    // 数据库通过连接池 acquire/release 避免阻塞
}

// ── 读取 ────────────────────────────
void HttpServer::handleRead(Connection* conn) {
    conn->lastActivity = (int64_t)time(nullptr);
    int n = conn->readBuf.readFrom(conn->fd);
    if(n<=0){closeConn(conn);return;}

    while(conn->readBuf.readable()>0){
        size_t readable=conn->readBuf.readable();
        size_t head=conn->readBuf.readPos()%conn->readBuf.capacity();
        size_t block=std::min(readable, conn->readBuf.capacity()-head);

        // 使用固定缓冲区 (零分配)
        size_t cp=std::min(block, (size_t)MAX_BODY_SIZE-1);
        conn->readBuf.peek(conn->tmpBuf, cp);
        conn->tmpBuf[cp]=0;

        // 但 parser 需要完整连续数据; 简单处理: 拷贝到临时 buffer
        // 完整读取
        size_t toCopy=std::min(readable, (size_t)MAX_BODY_SIZE);
        conn->readBuf.peek(conn->tmpBuf, toCopy);

        size_t consumed=conn->parser.parse(conn->tmpBuf, toCopy);
        if(consumed>0)conn->readBuf.consume(consumed);

        if(conn->parser.state()==HttpParser::COMPLETE){
            const auto& req=conn->parser.request();
            std::string path=req.path;

            if(req.method=="GET"&&path.find("/api/")!=0){
                if(path=="/")path="/index.html";
                serveStatic(conn,path);
            }else{
                auto* db=m_pool->acquire();
                HttpResponse resp=m_router.route(req,conn->clientIp,db);
                m_pool->release(db);
                std::string s=resp.build();
                conn->writeBuf.append(s.c_str(),s.length());
                conn->writeBuf.writeTo(conn->fd);  // 立刻尝试写入
            }
            auto it=req.headers.find("connection");
            if(it!=req.headers.end()&&it->second=="close")conn->closeAfterWrite=true;
            conn->parser.reset();
        }else if(conn->parser.state()==HttpParser::ERROR){
            const char* err="HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            conn->writeBuf.append(err,strlen(err));
            conn->closeAfterWrite=true; break;
        }else{break;}
        if(conn->readBuf.readable()==0)break;
    }
}

// ── 写入 ────────────────────────────
void HttpServer::handleWrite(Connection* conn) {
    conn->lastActivity=(int64_t)time(nullptr);
    if(conn->writeBuf.readable()>0){
        int n=conn->writeBuf.writeTo(conn->fd);
        if(n<0){closeConn(conn);return;}
    }
    if(conn->closeAfterWrite&&conn->writeBuf.empty())closeConn(conn);
}

// ── 静态文件 ────────────────────────
void HttpServer::serveStatic(Connection* conn, const std::string& path) {
    if(path.find("..")!=std::string::npos){
        const char* e="HTTP/1.1 403\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        conn->writeBuf.append(e,strlen(e)); conn->writeBuf.writeTo(conn->fd); conn->closeAfterWrite=true; return;
    }
    std::string full="public"+path;
    std::string content=readFile(full);
    if(content.empty()){
        const char* nf="HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: keep-alive\r\n\r\n";
        conn->writeBuf.append(nf,strlen(nf)); conn->writeBuf.writeTo(conn->fd); return;
    }
    std::string mime=getMime(path);
    char hdr[512]; snprintf(hdr,sizeof(hdr),"HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: keep-alive\r\n\r\n",mime.c_str(),content.length());
    conn->writeBuf.append(hdr,strlen(hdr));
    conn->writeBuf.append(content.c_str(),content.length());
    conn->writeBuf.writeTo(conn->fd);  // 立刻尝试写入
}

void HttpServer::closeConn(Connection* conn) {
    if(conn->fd>=0){epoll_ctl(m_epollFd,EPOLL_CTL_DEL,conn->fd,nullptr);close(conn->fd);conn->fd=-1;}
    m_closed.push_back(conn);  // 延迟回收, 避免主循环悬空指针
}
