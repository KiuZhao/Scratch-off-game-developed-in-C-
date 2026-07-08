#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>
#include <vector>
#include <string>
#include <cmath>

// ── 窗口 ────────────────────────────
constexpr int WIN_W = 1280;
constexpr int WIN_H = 720;
constexpr const char* WIN_TITLE = "刮刮乐";

// ── 自动存档 ────────────────────────
constexpr const char* SAVE_FILE = "guaguale_save.dat";
constexpr const char* SAVE_KEY   = "Guaguale2024!@#SecretKey";  // XOR 加密密钥

// ── 刮开判定阈值 ────────────────────
constexpr double SCRATCH_REVEAL_THRESHOLD = 0.95;

// ── 刮涂层格子内部掩码分辨率 ────────
constexpr int MASK_COLS = 25;
constexpr int MASK_ROWS = 18;

// ── 刮刮乐格子布局 ──────────────────
constexpr int CELL_W    = 150;
constexpr int CELL_H    = 100;
constexpr int CELL_GAP  = 10;
constexpr int GRID_COLS = 5;

// ── 字体 ────────────────────────────
#ifdef _WIN32
constexpr const char* FONT_PATH = "C:\\Windows\\Fonts\\msyh.ttc";
#else
constexpr const char* FONT_PATH = "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc";
#endif
constexpr int FONT_SIZE_SMALL  = 14;
constexpr int FONT_SIZE_NORMAL = 18;
constexpr int FONT_SIZE_LARGE  = 24;
constexpr int FONT_SIZE_TITLE  = 32;

// ── 颜色 ────────────────────────────
struct Color {
    uint8_t r, g, b, a;
    constexpr Color(uint8_t r_=255, uint8_t g_=255, uint8_t b_=255, uint8_t a_=255)
        : r(r_), g(g_), b(b_), a(a_) {}
};
constexpr Color C_BG       { 30,  20,  50, 255};  // 深紫背景
constexpr Color C_CARD_BG  { 50,  40,  80, 255};  // 卡片背景
constexpr Color C_COATING  {180, 170, 160, 255};  // 刮涂层颜色
constexpr Color C_WIN      {255, 200,  50, 255};  // 中奖色
constexpr Color C_LOSE     {100, 100, 110, 255};  // 未中奖色
constexpr Color C_AMOUNT   { 40,  30, 100, 255};  // 金额底色
constexpr Color C_BUTTON   { 70, 130, 200, 255};  // 按钮色
constexpr Color C_BUTTON_H {100, 160, 230, 255};  // 按钮悬浮色
constexpr Color C_GOLD     {255, 215,   0, 255};  // 金色文字
constexpr Color C_RED      {255,  80,  80, 255};  // 红色
constexpr Color C_GREEN    { 80, 220,  80, 255};  // 绿色
constexpr Color C_WHITE    {255, 255, 255, 255};
constexpr Color C_BLACK    {  0,   0,   0, 255};
constexpr Color C_OVERLAY  {  0,   0,   0, 180};  // 弹窗遮罩
constexpr Color C_POPUP_BG { 40,  30,  70, 255};  // 弹窗背景

// ── 卡片类型 ────────────────────────
struct CardTypeConfig {
    int  cost;                    // 售价 (方斯)
    int  positions;               // 刮开次数
    int  maxPrize;                // 最高奖金 (万方斯)
    const char* name;             // 名称
    const char* description;      // 描述文字
    Color bgColor;                // 卡片底色
    // 计算所得: 每位置中奖概率
    double winProbPerPos;
};

// 奖金表: 1,2,3,4,5,6,7,8,9,10,15,20,25 万 (+ 50万/+100万)
inline std::vector<int> getPrizeList(int maxPrizeWan) {
    std::vector<int> prizes;
    for (int i = 1; i <= 10; ++i) prizes.push_back(i * 10000);
    prizes.push_back(150000);
    prizes.push_back(200000);
    prizes.push_back(250000);
    if (maxPrizeWan >= 50)  prizes.push_back(500000);
    if (maxPrizeWan >= 100) prizes.push_back(1000000);
    return prizes;
}

// 指数衰减权重, λ=1.2
constexpr double LAMBDA = 1.2;
inline double prizeWeight(int prizeYuan) {
    return std::exp(-LAMBDA * (prizeYuan / 10000.0));
}

// 计算给定卡片配置下, 期望奖金 = cost * 0.9 所需的每位置中奖概率
inline double calcWinProb(int cost, int positions, int maxPrizeWan) {
    auto prizes = getPrizeList(maxPrizeWan);
    double sumW = 0.0, sumPW = 0.0;
    for (int p : prizes) {
        double w = prizeWeight(p);
        sumW  += w;
        sumPW += (p / 10000.0) * w;   // 万方斯 × 权重
    }
    double eWin = sumPW / sumW;        // 单次中奖期望 (万)
    double target = (cost / 10000.0) * 0.9;  // 目标期望 (万)
    return target / (positions * eWin);
}

// ── 三种刮刮乐定义 ──────────────────
inline std::vector<CardTypeConfig> getCardTypes() {
    std::vector<CardTypeConfig> types;

    // 1万 — 小试一手
    CardTypeConfig t1;
    t1.cost       = 10000;
    t1.positions  = 10;
    t1.maxPrize   = 25;
    t1.name       = "小试一手";
    t1.description = "小额试水, 10次机会";
    t1.bgColor    = Color{ 60, 140, 80, 255};
    t1.winProbPerPos = calcWinProb(t1.cost, t1.positions, t1.maxPrize);
    types.push_back(t1);

    // 2万 — 人之常情
    CardTypeConfig t2;
    t2.cost       = 20000;
    t2.positions  = 15;
    t2.maxPrize   = 50;
    t2.name       = "人之常情";
    t2.description = "中规中矩, 15次机会";
    t2.bgColor    = Color{ 60, 100, 180, 255};
    t2.winProbPerPos = calcWinProb(t2.cost, t2.positions, t2.maxPrize);
    types.push_back(t2);

    // 5万 — 人，稿子，钻石
    CardTypeConfig t3;
    t3.cost       = 50000;
    t3.positions  = 20;
    t3.maxPrize   = 100;
    t3.name       = "人，稿子，钻石";
    t3.description = "豪赌一场, 20次机会";
    t3.bgColor    = Color{ 180, 60, 140, 255};
    t3.winProbPerPos = calcWinProb(t3.cost, t3.positions, t3.maxPrize);
    types.push_back(t3);

    return types;
}

#endif
