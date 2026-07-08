#ifndef MAIN_H
#define MAIN_H

#include <vector>

constexpr int CARD_COST = 10000;           // 每张刮刮乐售价 (元)
constexpr int INITIAL_BALANCE = 100000;    // 初始持有金额 (元)
constexpr int POSITIONS_PER_CARD = 10;     // 单张刮刮乐刮开次数

// 奖金额度 (元): 1万~10万, 15万, 20万, 25万
const std::vector<int> PRIZES = {
    10000, 20000, 30000, 40000, 50000,
    60000, 70000, 80000, 90000, 100000,
    150000, 200000, 250000
};

// 每位置中奖概率: 使期望奖金 = 9000元/张
constexpr double PRIZE_SUM_WAN = 115.0;
constexpr int    PRIZE_COUNT   = 13;
constexpr double WIN_PROB_PER_POSITION =
    0.9 * PRIZE_COUNT / (POSITIONS_PER_CARD * PRIZE_SUM_WAN);

struct CardResult {
    std::vector<int> positions;
    int total_prize;
    bool is_winner;
};

CardResult generateCard();
void playGame();

#endif
