#include "handlers.h"
#include "name_filter.h"
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <cmath>
#include <ctime>
#include <algorithm>

using json = nlohmann::json;

// ============================== JWT ==============================

static std::string b64urlEncode(const std::string& d) {
    static const char* t="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string o; o.reserve((d.length()+2)/3*4);
    int v=0,vb=-6;
    for(uint8_t c:d){v=(v<<8)+c;vb+=8;while(vb>=0){o+=t[(v>>vb)&0x3F];vb-=6;}}
    if(vb>-6)o+=t[((v<<8)>>(vb+8))&0x3F];
    return o;
}

static std::string b64urlDecode(const std::string& d) {
    std::string s=d;
    for(auto& c:s){if(c=='-')c='+';if(c=='_')c='/';}
    while(s.length()%4)s+='=';
    static const char* t="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string o; int v=0,vb=-8;
    for(char c:s){if(c=='=')break;auto p=strchr(t,c);if(!p)continue;v=(v<<6)+(int)(p-t);vb+=6;if(vb>=0){o+=(char)((v>>vb)&0xFF);vb-=8;}}
    return o;
}

std::string Router::generateToken(int64_t uid, const std::string& uname) {
    auto now=(int64_t)time(nullptr);
    std::string hdr=b64urlEncode("{\"alg\":\"HS256\",\"typ\":\"JWT\"}");
    std::ostringstream pl;
    pl<<"{\"sub\":"<<uid<<",\"name\":\""<<uname<<"\",\"iat\":"<<now<<",\"exp\":"<<now+JWT_EXPIRE_SEC<<"}";
    std::string pay=b64urlEncode(pl.str()), si=hdr+"."+pay;
    uint8_t sig[32]; unsigned sl=32;
    auto sec=getJwtSecret();
    HMAC(EVP_sha256(), sec.c_str(), (int)sec.length(), (const uint8_t*)si.c_str(), si.length(), sig, &sl);
    return si+"."+b64urlEncode(std::string((char*)sig,sl));
}

bool Router::verifyToken(const std::string& tok, int64_t& uid, std::string& uname) {
    size_t d1=tok.find('.'), d2=tok.find('.',d1+1);
    if(d1==std::string::npos||d2==std::string::npos)return false;
    std::string si=tok.substr(0,d2);
    uint8_t sig[32]; unsigned sl=32;
    auto sec=getJwtSecret();
    HMAC(EVP_sha256(), sec.c_str(), (int)sec.length(), (const uint8_t*)si.c_str(), si.length(), sig, &sl);
    if(b64urlEncode(std::string((char*)sig,sl))!=tok.substr(d2+1))return false;
    std::string pay=b64urlDecode(tok.substr(d1+1,d2-d1-1));
    try{ auto j=json::parse(pay); uid=j.at("sub").get<int64_t>(); uname=j.at("name").get<std::string>();
         if((int64_t)time(nullptr)>j.at("exp").get<int64_t>())return false; }catch(...){return false;}
    return uid>0;
}

// ============================== 概率 =============================

double Router::calcWinProb(int cost, int pos, int maxPrize) {
    std::vector<int> pr; for(int i=1;i<=10;++i)pr.push_back(i*10000);
    pr.push_back(150000);pr.push_back(200000);pr.push_back(250000);
    if(maxPrize>=50)pr.push_back(500000); if(maxPrize>=100)pr.push_back(1000000);
    constexpr double L=1.2; double sW=0,sPW=0;
    for(int p:pr){double w=std::exp(-L*(p/10000.0));sW+=w;sPW+=(p/10000.0)*w;}
    return (cost/10000.0*0.9)/(pos*sPW/sW);
}

std::string Router::getCardTypesJson() {
    json arr=json::array();
    struct{int cost,pos,maxPrize;const char* name;const char* color;} cfgs[]={
        {10000,10,25,"小试一手","#3C8C50"},{20000,15,50,"人之常情","#3C64B4"},{50000,20,100,"人，稿子，钻石","#B43C8C"}};
    for(auto&c:cfgs)arr.push_back({{"cost",c.cost},{"positions",c.pos},{"maxPrize",c.maxPrize},{"name",c.name},{"color",c.color}});
    return arr.dump();
}

// ============================== Router ===========================

Router::Router(MySQLPool* pool, Redis* redis, RateLimiter* rl)
    : m_pool(pool), m_redis(redis), m_rl(rl), m_rng(std::random_device{}()) {}

int64_t Router::authenticate(const HttpRequest& req) {
    std::string tok;
    auto it=req.headers.find("authorization");
    if(it!=req.headers.end()&&it->second.rfind("Bearer ",0)==0)tok=it->second.substr(7);
    if(tok.empty()){auto ci=req.headers.find("cookie");
        if(ci!=req.headers.end()){auto&c=ci->second;auto p=c.find("token=");
            if(p!=std::string::npos){p+=6;auto e=c.find(';',p);tok=c.substr(p,e-p);}}}
    int64_t uid=0; std::string un;
    if(tok.empty()||!verifyToken(tok,uid,un))return 0;
    return uid;
}

HttpResponse Router::route(const HttpRequest& req, const std::string& clientIp, MySQLConn* db) {
    if(req.method=="OPTIONS")return handleOptions();
    std::string p=req.path;
    std::string queryStr;
    size_t qpos=p.find('?'); if(qpos!=std::string::npos){queryStr=p.substr(qpos+1);p=p.substr(0,qpos);}

    if(p=="/api/register"&&req.method=="POST")return handleRegister(req,db);
    if(p=="/api/login"&&req.method=="POST")return handleLogin(req,clientIp,db);
    if(p=="/api/cards/types"&&req.method=="GET")return handleCardTypes();

    int64_t uid=authenticate(req);
    if(p=="/api/me"&&req.method=="GET")return handleMe(req,db);
    if(p=="/api/cards/buy"&&req.method=="POST")return handleBuy(req,clientIp,db);
    if(p=="/api/cheat"&&req.method=="POST")return handleCheat(req,clientIp,db);
    if(p=="/api/ranking"&&req.method=="GET")return handleRanking(queryStr);
    if(p=="/api/souvenirs"&&req.method=="GET")return handleSouvenirs(req,db);

    return HttpResponse::notFound().json("{\"error\":\"not found\"}");
}

// ── 卡片类型 ────────────────────────
HttpResponse Router::handleCardTypes() { return HttpResponse::ok().json(getCardTypesJson()); }

// ── 注册 ────────────────────────────
HttpResponse Router::handleRegister(const HttpRequest& req, MySQLConn* db) {
    if(!m_rl->allow("reg:"+req.headers.value_or("x-forwarded-for","0.0.0.0")))
        return HttpResponse().status(429,"Too Many Requests").json("{\"error\":\"rate limited\"}");
    try{
        auto j=json::parse(req.body);
        std::string u=j.at("username"), p=j.at("password");
        if(u.length()<2||u.length()>32)return HttpResponse::badRequest().json("{\"error\":\"username 2-32 chars\"}");
        std::string filterReason;
        if(!NameFilter::instance().check(u, filterReason))
            return HttpResponse::badRequest().json(("{\"error\":\""+filterReason+"\"}").c_str());
        auto rows=db->query("SELECT id FROM users WHERE username='%s'",db->escape(u).c_str());
        if(!rows.empty())return HttpResponse().status(409,"Conflict").json("{\"error\":\"username taken\"}");
        uint8_t h[SHA256_DIGEST_LENGTH]; SHA256((const uint8_t*)p.c_str(),p.length(),h);
        char hx[65];for(int i=0;i<32;++i)snprintf(hx+i*2,3,"%02x",h[i]);
        db->execute("INSERT INTO users(username,password_hash,balance)VALUES('%s','%s',%lld)",db->escape(u).c_str(),hx,(long long)INITIAL_BALANCE);
        int64_t uid=(int64_t)db->lastInsertId();
        std::string tok=generateToken(uid,u);
        json resp={{"token",tok},{"user",{{"id",uid},{"username",u},{"balance",INITIAL_BALANCE},{"profit",0}}}};
        return HttpResponse::created().json(resp.dump()).setCookie("token",tok.c_str());
    }catch(...){return HttpResponse::badRequest().json("{\"error\":\"invalid json\"}");}
}

// ── 登录 ────────────────────────────
HttpResponse Router::handleLogin(const HttpRequest& req, const std::string& ip, MySQLConn* db) {
    if(!m_rl->allow(ip))return HttpResponse().status(429,"Too Many Requests").json("{\"error\":\"rate limited\"}");
    try{
        auto j=json::parse(req.body);
        std::string u=j.at("username"), p=j.at("password");
        uint8_t h[SHA256_DIGEST_LENGTH]; SHA256((const uint8_t*)p.c_str(),p.length(),h);
        char hx[65];for(int i=0;i<32;++i)snprintf(hx+i*2,3,"%02x",h[i]);
        auto rows=db->query("SELECT id,balance,profit FROM users WHERE username='%s' AND password_hash='%s'",db->escape(u).c_str(),hx);
        if(rows.empty())return HttpResponse::unauthorized().json("{\"error\":\"invalid credentials\"}");
        auto&r=rows[0]; int64_t uid=strtoll(r[0].c_str(),nullptr,10);
        std::string tok=generateToken(uid,u);
        json resp={{"token",tok},{"user",{{"id",uid},{"username",u},{"balance",strtoll(r[1].c_str(),nullptr,10)},{"profit",strtoll(r[2].c_str(),nullptr,10)}}}};
        return HttpResponse::ok().json(resp.dump()).setCookie("token",tok.c_str());
    }catch(...){return HttpResponse::badRequest().json("{\"error\":\"invalid json\"}");}
}

// ── 用户信息 ────────────────────────
HttpResponse Router::handleMe(const HttpRequest& req, MySQLConn* db) {
    int64_t uid=authenticate(req); if(!uid)return HttpResponse::unauthorized().json("{\"error\":\"need login\"}");
    auto rows=db->query("SELECT id,username,balance,profit,souvenir_1,souvenir_2,souvenir_3 FROM users WHERE id=%lld",(long long)uid);
    if(rows.empty())return HttpResponse::notFound().json("{\"error\":\"not found\"}");
    auto&r=rows[0];
    json resp={{"id",strtoll(r[0].c_str(),nullptr,10)},{"username",r[1]},{"balance",strtoll(r[2].c_str(),nullptr,10)},{"profit",strtoll(r[3].c_str(),nullptr,10)},{"souvenirs",json::array({atoi(r[4].c_str()),atoi(r[5].c_str()),atoi(r[6].c_str())})}};
    return HttpResponse::ok().json(resp.dump());
}

// ── 买卡 (一段式: 购买即结算) ──────
HttpResponse Router::handleBuy(const HttpRequest& req, const std::string& clientIp, MySQLConn* db) {
    int64_t uid=authenticate(req); if(!uid)return HttpResponse::unauthorized().json("{\"error\":\"need login\"}");
    // IP 限流
    if(!m_rl->allow(clientIp))return HttpResponse().status(429,"Too Many Requests").json("{\"error\":\"rate limited\"}");
    // 用户限流: 每秒最多3张
    if(!m_rl->allowUser(std::to_string(uid), 1.0, 3.0))return HttpResponse().status(429,"Too Many Requests").json("{\"error\":\"too fast, slow down\"}");
    try{
        auto j=json::parse(req.body); int ti=j.at("typeIndex");
        if(ti<0||ti>2)return HttpResponse::badRequest().json("{\"error\":\"invalid type\"}");

        struct{int cost,pos,maxP;std::string name;} cfgs[]={{10000,10,25,"小试一手"},{20000,15,50,"人之常情"},{50000,20,100,"人，稿子，钻石"}};
        auto&c=cfgs[ti];

        auto ur=db->query("SELECT balance FROM users WHERE id=%lld",(long long)uid);
        if(ur.empty())return HttpResponse::notFound().json("{\"error\":\"not found\"}");
        int64_t bal=strtoll(ur[0][0].c_str(),nullptr,10);
        if(bal<c.cost)return HttpResponse::badRequest().json("{\"error\":\"insufficient balance\"}");

        // 生成卡片
        std::vector<int> pr; for(int i=1;i<=10;++i)pr.push_back(i*10000);
        pr.push_back(150000);pr.push_back(200000);pr.push_back(250000);
        if(c.maxP>=50)pr.push_back(500000); if(c.maxP>=100)pr.push_back(1000000);
        std::vector<double> wts; constexpr double L=1.2;
        for(int p:pr)wts.push_back(std::exp(-L*(p/10000.0)));
        std::discrete_distribution<size_t> pd(wts.begin(),wts.end());
        std::uniform_int_distribution<size_t> ud(0,pr.size()-1);
        double wp=calcWinProb(c.cost,c.pos,c.maxP);

        int totalPrize=0; json cells=json::array();
        struct{bool win;int prize,display;} cds[20];  // 最多20格
        for(int i=0;i<c.pos;++i){
            bool win=m_probDist(m_rng)<wp;
            int prize=win?pr[pd(m_rng)]:0;
            int display=win?prize:pr[ud(m_rng)];
            if(win)totalPrize+=prize;
            cells.push_back({{"isWin",win},{"displayPrize",display}});
            cds[i]={win,prize,display};
        }

        // 扣款+发奖
        bal=bal-c.cost+totalPrize;
        db->execute("UPDATE users SET balance=%lld,total_spent=total_spent+%d,total_prizes=total_prizes+%d,profit=total_prizes-total_spent,total_bought=total_bought+1,total_won=total_won+%d,souvenir_%d=souvenir_%d+1 WHERE id=%lld",(long long)bal,c.cost,totalPrize,totalPrize>0?1:0,ti+1,ti+1,(long long)uid);

        // 记录卡片
        db->execute("INSERT INTO cards(user_id,card_type,cost,positions,total_prize)VALUES(%lld,%d,%d,%d,%d)",(long long)uid,ti,c.cost,c.pos,totalPrize);
        int64_t cid=(int64_t)db->lastInsertId();
        for(int i=0;i<c.pos;++i)
            db->execute("INSERT INTO card_cells(card_id,position,is_win,prize,display_prize)VALUES(%lld,%d,%d,%d,%d)",(long long)cid,i,cds[i].win?1:0,cds[i].prize,cds[i].display);

        // 排行榜
        auto rr=db->query("SELECT username,profit FROM users WHERE id=%lld",(long long)uid);
        if(!rr.empty()&&m_redis->isConnected()){int64_t pf=strtoll(rr[0][1].c_str(),nullptr,10);m_redis->zset(REDIS_KEY_PROFIT,pf,rr[0][0].c_str());m_redis->zset(REDIS_KEY_LOSS,pf,rr[0][0].c_str());}

        auto br=db->query("SELECT balance,profit,souvenir_1,souvenir_2,souvenir_3 FROM users WHERE id=%lld",(long long)uid);
        auto&b=br[0];
        json resp={{"cardId",cid},{"cardType",ti},{"cardName",c.name},{"positions",c.pos},{"totalPrize",totalPrize},{"balance",strtoll(b[0].c_str(),nullptr,10)},{"profit",strtoll(b[1].c_str(),nullptr,10)},{"souvenirs",json::array({atoi(b[2].c_str()),atoi(b[3].c_str()),atoi(b[4].c_str())})},{"cells",cells}};
        return HttpResponse::ok().json(resp.dump());
    }catch(...){return HttpResponse::badRequest().json("{\"error\":\"invalid json\"}");}
}

// ── 作弊 ────────────────────────────
HttpResponse Router::handleCheat(const HttpRequest& req, const std::string& ip, MySQLConn* db) {
    int64_t uid=authenticate(req); if(!uid)return HttpResponse::unauthorized().json("{\"error\":\"need login\"}");
    if(!m_rl->allow("cheat:"+ip))return HttpResponse().status(429,"Too Many Requests").json("{\"error\":\"rate limited\"}");
    try{
        auto j=json::parse(req.body); int clicks=j.at("clicks");
        std::string msg; int64_t amt=0;
        if(clicks==20){msg="背着店长偷偷把伊波恩抵押了，这一世我定要……";amt=10000000;}
        else if(clicks==10){msg="管理局给娜娜莉的医疗补贴下来了";amt=1000000;}
        else{msg="开了一天滴滴，收入10万方斯";amt=100000;}
        db->execute("UPDATE users SET balance=balance+%lld WHERE id=%lld",(long long)amt,(long long)uid);
        auto br=db->query("SELECT balance FROM users WHERE id=%lld",(long long)uid);
        return HttpResponse::ok().json(json{{"message",msg},{"amount",amt},{"balance",strtoll(br[0][0].c_str(),nullptr,10)}}.dump());
    }catch(...){return HttpResponse::badRequest().json("{\"error\":\"invalid json\"}");}
}

// ── 排行榜 ──────────────────────────
HttpResponse Router::handleRanking(const std::string& qs) {
    std::string type="profit";
    { size_t p=qs.find("type="); if(p!=std::string::npos){type=qs.substr(p+5);size_t a=type.find('&');if(a!=std::string::npos)type=type.substr(0,a);}}
    if(!m_redis->isConnected())return HttpResponse::ok().json("{\"ranking\":[]}");
    json arr=json::array(); const char* key=(type=="loss")?REDIS_KEY_LOSS:REDIS_KEY_PROFIT;
    if(type=="loss"){auto l=m_redis->zrange(key,0,RANKING_TOP_N-1);
        for(size_t i=0;i<l.size();++i)arr.push_back({{"rank",i+1},{"username",l[i].first},{"value",l[i].second}});}
    else{auto l=m_redis->zrevrange(key,0,RANKING_TOP_N-1);
        for(size_t i=0;i<l.size();++i)arr.push_back({{"rank",i+1},{"username",l[i].first},{"value",l[i].second}});}
    return HttpResponse::ok().json(json{{"type",type},{"ranking",arr}}.dump());
}

// ── 纪念品 ──────────────────────────
HttpResponse Router::handleSouvenirs(const HttpRequest& req, MySQLConn* db) {
    int64_t uid=authenticate(req); if(!uid)return HttpResponse::unauthorized().json("{\"error\":\"need login\"}");
    auto r=db->query("SELECT souvenir_1,souvenir_2,souvenir_3 FROM users WHERE id=%lld",(long long)uid);
    if(r.empty())return HttpResponse::ok().json("[0,0,0]");
    return HttpResponse::ok().json(json::array({atoi(r[0][0].c_str()),atoi(r[0][1].c_str()),atoi(r[0][2].c_str())}).dump());
}

HttpResponse Router::handleOptions() {
    return HttpResponse().status(204,"No Content").header("Access-Control-Allow-Origin","*").header("Access-Control-Allow-Headers","Content-Type,Authorization").header("Access-Control-Allow-Methods","GET,POST,OPTIONS").header("Access-Control-Max-Age","86400");
}
