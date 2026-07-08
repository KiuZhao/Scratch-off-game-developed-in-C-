# 刮刮乐 Web 版

C++ 自写 HTTP 服务器 + Canvas 刮涂层前端的在线刮刮乐游戏。

## 技术栈

| 层 | 技术 |
|---|------|
| 后端 | C++20, 自写 HTTP/1.1 解析器, epoll 边缘触发, 自写 JWT |
| 数据库 | MySQL 8.x, Redis 7.x |
| 前端 | Vue 3 (CDN), Canvas API |
| 部署 | Nginx 反代, systemd 守护 |

## 项目结构

```
web/
├── server/              # C++ 后端
│   ├── config.h         配置常量（端口、数据库密码、JWT 密钥等）
│   ├── http.h/cpp       HTTP/1.1 解析器（状态机）+ 响应构建器
│   ├── ring_buffer.h    环形缓冲区（每连接读写分离，非阻塞 I/O）
│   ├── server.h/cpp     epoll 事件循环 + accept + 静态文件服务
│   ├── handlers.h/cpp   路由 + 注册/登录/买卡/排行榜/作弊
│   ├── db.h/cpp         MySQL 连接池 + hiredis Redis 封装
│   ├── rate_limiter.h   令牌桶限流器（IP + 用户双级）
│   ├── main.cpp         入口，信号处理，线程池初始化
│   └── CMakeLists.txt   构建文件
├── public/              # 前端
│   ├── index.html       Vue 3 单页应用
│   ├── css/style.css    全界面样式
│   └── js/
│       ├── app.js       Vue 应用逻辑（认证、购卡、刮奖、排行榜）
│       └── scratch.js   Canvas 刮涂层引擎（destination-out 合成）
└── sql/
    └── schema.sql       MySQL 建表脚本
```

## API 接口

| 方法 | 路径 | 说明 | 认证 |
|------|------|------|------|
| POST | `/api/register` | 注册 | 否（IP 限流） |
| POST | `/api/login` | 登录，返回 JWT | 否（IP 限流） |
| GET  | `/api/me` | 当前用户信息 + 纪念品 | JWT |
| GET  | `/api/cards/types` | 三种刮刮乐元数据 | 否 |
| POST | `/api/cards/buy` | 购买刮刮乐，返回完整结果 | JWT + 限流 |
| POST | `/api/cheat` | 作弊按钮 | JWT + 限流 |
| GET  | `/api/ranking?type=profit\|loss` | 富爪榜 / 负爪榜 | 否 |
| GET  | `/api/souvenirs` | 纪念品数量 | JWT |

## 在 Ubuntu 上部署

### 安装依赖

```bash
sudo apt update
sudo apt install -y g++ make cmake pkg-config \
  libmysqlclient-dev libhiredis-dev libssl-dev nlohmann-json3-dev \
  mysql-server redis-server nginx
```

### 配置数据库

```bash
sudo systemctl start mysql redis-server
sudo mysql -u root <<SQL
ALTER USER 'root'@'localhost' IDENTIFIED WITH mysql_native_password BY '你的密码';
FLUSH PRIVILEGES;
CREATE DATABASE IF NOT EXISTS guaguale DEFAULT CHARACTER SET utf8mb4;
SQL
mysql -u root -p你的密码 guaguale < sql/schema.sql
```

### 修改配置

编辑 `server/config.h`：

```cpp
constexpr const char* MYSQL_PASS = "你的密码";    // 改成上面设的密码
```

设置 JWT 环境变量：

```bash
export JWT_SECRET="你自己生成一个随机字符串"
```

### 编译运行

```bash
cd server && mkdir -p build && cd build
cmake .. && make -j$(nproc)
cd ../..
./server/build/guaguale_server
```

### 配置 Nginx（生产环境）

```nginx
server {
    listen 80;
    server_name _;

    location / {
        root /path/to/web/public;
        index index.html;
        try_files $uri /index.html;
    }

    location /api/ {
        proxy_pass http://127.0.0.1:8080;
        proxy_http_version 1.1;
        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-For $remote_addr;
    }
}
```

### 注册 systemd 服务（开机自启 + 崩溃重启）

```ini
[Unit]
Description=GuaGuaLe Server
After=network.target mysql.service redis-server.service

[Service]
Type=simple
WorkingDirectory=/path/to/web
Environment=JWT_SECRET=你的JWT密钥
ExecStart=/path/to/web/server/build/guaguale_server
Restart=always
RestartSec=3

[Install]
WantedBy=multi-user.target
```

## 架构要点

- **一段式购买**：买卡时服务端即随机生成结果、扣款、发奖，前端只做涂层动画揭示，无二次请求
- **指数衰减概率**：奖金权重 = exp(-1.2 × 金额)，小额常见、大额稀有
- **双级限流**：IP 令牌桶（10 rps）+ 用户令牌桶（1 张/秒），防脚本刷
- **自写 HTTP**：零依赖 HTTP/1.1 解析，环形缓冲区零分配，epoll 边缘触发单线程 I/O
- **JWT 认证**：自写 HMAC-SHA256 + base64url，24 小时过期

## 许可证

MIT
