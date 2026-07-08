#include "save.h"
#include "config.h"
#include <fstream>
#include <cstring>

static const uint32_t SAVE_MAGIC   = 0x474C4753;  // "SGLG" — 刮刮乐存档
static const uint32_t SAVE_VERSION = 1;

SaveManager::SaveManager() {
    defaultPath = SAVE_FILE;
}

SaveManager::~SaveManager() {}

// XOR 加密
void SaveManager::xorCrypt(uint8_t* buf, size_t len) {
    const char* key = SAVE_KEY;
    size_t keyLen = std::strlen(key);
    for (size_t i = 0; i < len; ++i) {
        buf[i] ^= (uint8_t)key[i % keyLen];
    }
}

uint32_t SaveManager::crc32(const uint8_t* buf, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= buf[i];
        for (int j = 0; j < 8; ++j)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return ~crc;
}

// 序列化: magic(4) + version(4) + dataSize(4) + data(n) + crc32(4)
std::vector<uint8_t> SaveManager::serialize(const SaveData& d) const {
    std::vector<uint8_t> out;
    auto push32 = [&](uint32_t v) {
        out.push_back((uint8_t)(v));
        out.push_back((uint8_t)(v >> 8));
        out.push_back((uint8_t)(v >> 16));
        out.push_back((uint8_t)(v >> 24));
    };
    auto push64 = [&](int64_t v) {
        uint64_t uv = (uint64_t)v;
        for (int i = 0; i < 8; ++i)
            out.push_back((uint8_t)(uv >> (i * 8)));
    };

    // 计算数据区大小
    // balance(8) + profit(8) + cheatMoney(8) + souvenirs[3](12) +
    // totalBought(4) + totalWon(4) + totalSpent(8) + totalPrizes(8) +
    // cheatClicks(4) + cheatDialogNormal(1) + cheatDialog10(1) + cheatDialog20(1)
    const uint32_t dataSize = 8+8+8 + 4*3 + 4+4+8+8 + 4+1+1+1;

    push32(SAVE_MAGIC);
    push32(SAVE_VERSION);
    push32(dataSize);

    size_t dataStart = out.size();
    push64(d.balance);
    push64(d.profit);
    push64(d.cheatMoney);
    for (int i = 0; i < 3; ++i) push32((uint32_t)d.souvenirs[i]);
    push32((uint32_t)d.totalBought);
    push32((uint32_t)d.totalWon);
    push64(d.totalSpent);
    push64(d.totalPrizes);
    push32((uint32_t)d.cheatClicks);
    out.push_back(d.cheatDialogNormal ? 1 : 0);
    out.push_back(d.cheatDialog10     ? 1 : 0);
    out.push_back(d.cheatDialog20     ? 1 : 0);

    // CRC32 of data payload only
    uint32_t crc = crc32(&out[dataStart], dataSize);
    push32(crc);

    // 加密数据区 + CRC
    xorCrypt(&out[dataStart], out.size() - dataStart);

    return out;
}

bool SaveManager::deserialize(const uint8_t* buf, size_t len, SaveData& d) const {
    if (len < 16) return false;

    // 读取头部 (从原始 buf)
    uint32_t magic   = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
                       ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    if (magic != SAVE_MAGIC) return false;

    uint32_t version = (uint32_t)buf[4] | ((uint32_t)buf[5] << 8) |
                       ((uint32_t)buf[6] << 16) | ((uint32_t)buf[7] << 24);
    if (version != SAVE_VERSION) return false;

    uint32_t dataSize = (uint32_t)buf[8] | ((uint32_t)buf[9] << 8) |
                        ((uint32_t)buf[10] << 16) | ((uint32_t)buf[11] << 24);

    if (len < 12 + dataSize + 4) return false;

    // 拷贝并解密
    std::vector<uint8_t> dec(dataSize + 4);
    std::memcpy(dec.data(), buf + 12, dataSize + 4);
    xorCrypt(dec.data(), dataSize + 4);

    // 验签
    uint32_t expectedCrc = crc32(dec.data(), dataSize);
    uint32_t storedCrc = ((uint32_t)dec[dataSize]) |
                         ((uint32_t)dec[dataSize+1] << 8) |
                         ((uint32_t)dec[dataSize+2] << 16) |
                         ((uint32_t)dec[dataSize+3] << 24);
    if (expectedCrc != storedCrc) return false;

    // 从解密后的 dec 读取数据
    auto read32dec = [&dec](size_t off) -> uint32_t {
        return (uint32_t)dec[off] | ((uint32_t)dec[off+1] << 8) |
               ((uint32_t)dec[off+2] << 16) | ((uint32_t)dec[off+3] << 24);
    };
    auto read64dec = [&dec](size_t off) -> int64_t {
        uint64_t uv = 0;
        for (int i = 0; i < 8; ++i)
            uv |= ((uint64_t)dec[off+i] << (i * 8));
        return (int64_t)uv;
    };

    size_t off = 0;
    d.balance       = read64dec(off); off += 8;
    d.profit        = read64dec(off); off += 8;
    d.cheatMoney    = read64dec(off); off += 8;
    for (int i = 0; i < 3; ++i) {
        d.souvenirs[i] = (int)read32dec(off);
        off += 4;
    }
    d.totalBought   = (int)read32dec(off); off += 4;
    d.totalWon      = (int)read32dec(off); off += 4;
    d.totalSpent    = read64dec(off); off += 8;
    d.totalPrizes   = read64dec(off); off += 8;
    d.cheatClicks   = (int)read32dec(off); off += 4;
    d.cheatDialogNormal = dec[off] != 0; off += 1;
    d.cheatDialog10     = dec[off] != 0; off += 1;
    d.cheatDialog20     = dec[off] != 0; off += 1;

    return true;
}

bool SaveManager::autoLoad(SaveData& data) {
    std::ifstream f(defaultPath, std::ios::binary);
    if (!f) return false;

    f.seekg(0, std::ios::end);
    size_t len = (size_t)f.tellg();
    f.seekg(0, std::ios::beg);

    std::vector<uint8_t> buf(len);
    f.read((char*)buf.data(), len);
    f.close();

    return deserialize(buf.data(), len, data);
}

bool SaveManager::autoSave(const SaveData& data) {
    auto buf = serialize(data);

    std::ofstream f(defaultPath, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write((const char*)buf.data(), buf.size());
    f.close();
    return true;
}

bool SaveManager::saveToFile(const SaveData& data, const char* path) {
    auto buf = serialize(data);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write((const char*)buf.data(), buf.size());
    f.close();
    return true;
}

bool SaveManager::loadFromFile(SaveData& data, const char* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    f.seekg(0, std::ios::end);
    size_t len = (size_t)f.tellg();
    f.seekg(0, std::ios::beg);

    std::vector<uint8_t> buf(len);
    f.read((char*)buf.data(), len);
    f.close();

    return deserialize(buf.data(), len, data);
}
