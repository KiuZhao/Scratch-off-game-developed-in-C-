#ifndef SAVE_H
#define SAVE_H

#include <string>
#include <vector>
#include <cstdint>

// ── 持久化数据结构 ──────────────────
struct SaveData {
    // 财务
    int64_t balance    = 1000000;  // 当前持有方斯 (初始100万)
    int64_t profit     = 0;       // 累计盈利 (正=赚, 负=亏, 不含作弊)
    int64_t cheatMoney = 0;       // 通过作弊获得的方斯 (不计入 profit)

    // 彩票纪念品收集 (每种卡片各收集了多少张)
    int souvenirs[3]   = {0, 0, 0};

    // 购买/中奖记录
    int totalBought    = 0;       // 总购买次数
    int totalWon       = 0;       // 中奖次数 (至少中一次的卡)
    int64_t totalSpent  = 0;      // 累计消费
    int64_t totalPrizes = 0;      // 累计中奖金额

    // 作弊状态
    int cheatClicks    = 0;       // 当前会话点击次数
    bool cheatDialogNormal = false;  // 普通对话已触发
    bool cheatDialog10     = false;  // 第10次对话已触发
    bool cheatDialog20     = false;  // 第20次对话已触发
};

// ── 存档管理器 ──────────────────────
class SaveManager {
public:
    SaveManager();
    ~SaveManager();

    // 自动加载 (启动时)
    bool autoLoad(SaveData& data);
    // 自动保存 (退出时)
    bool autoSave(const SaveData& data);
    // 手动保存/读取
    bool saveToFile(const SaveData& data, const char* path);
    bool loadFromFile(SaveData& data, const char* path);

private:
    // XOR 加密/解密
    static void xorCrypt(uint8_t* buf, size_t len);
    // 序列化
    std::vector<uint8_t> serialize(const SaveData& data) const;
    bool deserialize(const uint8_t* buf, size_t len, SaveData& data) const;
    // 简单 CRC32
    static uint32_t crc32(const uint8_t* buf, size_t len);

    std::string defaultPath;
};

#endif
