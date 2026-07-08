#include "main.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <random>
#include <limits>
#ifdef _WIN32
#include <windows.h>
#endif

// 格式化金额显示
static std::string fmtMoney(int v) {
    if (v >= 10000) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << (v / 10000.0) << "万";
        return oss.str();
    }
    return std::to_string(v);
}

CardResult generateCard() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<double> prob_dist(0.0, 1.0);
    static std::uniform_int_distribution<size_t> prize_dist(0, PRIZES.size() - 1);

    CardResult result;
    result.total_prize = 0;
    result.positions.resize(POSITIONS_PER_CARD);

    for (int i = 0; i < POSITIONS_PER_CARD; ++i) {
        if (prob_dist(gen) < WIN_PROB_PER_POSITION) {
            int prize = PRIZES[prize_dist(gen)];
            result.positions[i] = prize;
            result.total_prize += prize;
        } else {
            result.positions[i] = 0;
        }
    }
    result.is_winner = (result.total_prize > 0);
    return result;
}

void playGame() {
    int balance = INITIAL_BALANCE;
    int cards_bought = 0;
    int total_spent = 0;
    int total_won = 0;

    std::cout << "========================================\n";
    std::cout << "          刮 刮 乐 模 拟\n";
    std::cout << "========================================\n\n";
    std::cout << "  每张售价: 10,000 元 | 初始持有: 100,000 元\n";
    std::cout << "  单张 10 次刮开机会 | 最高奖金 25 万元\n\n";

    while (true) {
        std::cout << "----------------------------------------\n";
        std::cout << "  当前余额: " << fmtMoney(balance) << " 元\n";
        std::cout << "  是否购买一张刮刮乐? (y/n): ";

        char choice;
        std::cin >> choice;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        if (choice != 'y' && choice != 'Y') break;

        if (balance < CARD_COST) {
            std::cout << "\n  *** 余额不足，无法购买! ***\n";
            break;
        }

        balance -= CARD_COST;
        total_spent += CARD_COST;
        cards_bought++;

        CardResult card = generateCard();

        std::cout << "\n  ====== 第 " << cards_bought << " 张刮刮乐 ======\n";
        std::cout << "    按回车键刮开每个位置\n";

        int win_count = 0;
        for (int i = 0; i < POSITIONS_PER_CARD; ++i) {
            printf("\n    [%2d/%2d] 刮开 (按回车)", i + 1, POSITIONS_PER_CARD);
            std::cin.get();

            if (card.positions[i] > 0) {
                win_count++;
                std::cout << "  --> 🎉 中奖! +" << card.positions[i]
                          << " 元 (" << fmtMoney(card.positions[i]) << "元)";
            } else {
                std::cout << "  --> 💔 未中奖";
            }
        }

        balance += card.total_prize;
        total_won += card.total_prize;

        std::cout << "\n\n  +-- 本张结果 --+\n";
        if (card.is_winner) {
            std::cout << "  | 恭喜! 中了 " << win_count << " 次, 总奖金 "
                      << fmtMoney(card.total_prize) << "元 |\n";
        } else {
            std::cout << "  | 很遗憾，没能中奖，或许就是下一张! |\n";
        }
        std::cout << "  +--------------+\n";
    }

    int net = total_won - total_spent;
    std::cout << "\n";
    std::cout << "  ============ 游戏总结 ============\n";
    std::cout << "    共购买: " << cards_bought << " 张\n";
    std::cout << "    总花费: " << fmtMoney(total_spent) << " 元\n";
    std::cout << "    总中奖: " << fmtMoney(total_won) << " 元\n";
    if (net > 0)
        std::cout << "    净盈利: +" << fmtMoney(net) << " 元\n";
    else if (net < 0)
        std::cout << "    净亏损: " << fmtMoney(-net) << " 元\n";
    else
        std::cout << "    不赚不赔\n";
    std::cout << "  ===================================\n";
    std::cout << "\n  按回车键退出...";
    std::cin.get();
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    playGame();
    return 0;
}
