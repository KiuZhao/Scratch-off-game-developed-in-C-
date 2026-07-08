# 刮刮乐模拟游戏

三版本刮刮乐/刮刮卡模拟：终端版 → 桌面版 → Web 版，逐步迭代，旨在复现异环刮刮乐玩法。

Web 版现已上线，试玩地址：http://43.143.121.240/

## 目录

| 目录 | 说明 | 界面 |
|------|------|------|
| [`terminal/`](terminal/) | 纯终端运行，回车刮开 | 命令行 |
| [`desktop/`](desktop/) | SDL2 图形窗口，鼠标拖动刮涂层 | 1280×720 |
| [`web/`](web/) | 浏览器在线玩，Canvas 刮涂层 | 网页 |

## 快速运行

### 终端版
```bash
cd terminal
g++ -std=c++17 main.cpp -o guaguale_terminal && ./guaguale_terminal
```

### 桌面版
```bash
cd desktop
g++ -std=c++17 main.cpp game.cpp render.cpp save.cpp \
  $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o guaguale_desktop
./guaguale_desktop
```

### Web 版
见 [`web/README.md`](web/README.md)

## 三种刮刮乐

| 卡片 | 售价 | 格子数 | 最高奖金 | 期望回报 |
|------|------|--------|----------|----------|
| 小试一手 | 1万 方斯 | 10 格 | 25万 | ~90% |
| 人之常情 | 2万 方斯 | 15 格 | 50万 | ~90% |
| 人，稿子，钻石 | 5万 方斯 | 20 格 | 100万 | ~90% |

## 概率系统

奖金权重采用指数衰减：**weight = exp(-1.2 × 奖金)**，小额常见、大额稀有。每张彩票的期望奖金 = 售价 × 90%（长期期望净亏损 10%）。

## 功能

- 🎰 三种面额刮刮乐选购
- 🖱️ 鼠标拖动刮涂层（桌面版/Web 版）
- 🏆 富爪榜 / 负爪榜（Web 版）
- 🚗 作弊按钮"开滴滴"
- 💾 加密存档（桌面版）

## 技术栈

| 版本 | 语言 | 关键依赖 |
|------|------|----------|
| 终端版 | C++17 | 无 |
| 桌面版 | C++17 | SDL2, SDL2_ttf |
| Web 后端 | C++20 | MySQL, hiredis, OpenSSL, nlohmann/json |
| Web 前端 | Vue 3 | Canvas API (CDN) |
