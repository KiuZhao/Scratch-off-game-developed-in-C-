#ifndef NAME_FILTER_H
#define NAME_FILTER_H

#include <string>
#include <unordered_set>
#include <cstring>

// ── 用户名过滤 (DFA 敏感词 + 规则) ─────────────────

class NameFilter {
public:
    static NameFilter& instance() {
        static NameFilter f;
        return f;
    }

    // 返回 true = 通过, false = 拒绝
    // rejectReason 输出拒绝原因
    bool check(const std::string& name, std::string& reason) const {
        // 1. 长度
        if (name.length() < 2)      { reason = "用户名至少 2 个字符"; return false; }
        if (name.length() > 16)     { reason = "用户名最多 16 个字符"; return false; }

        // 2. 纯数字/纯符号
        bool hasLetter = false;
        for (char c : name) if (isalpha((uint8_t)c)) { hasLetter = true; break; }
        if (!hasLetter) { reason = "用户名需包含字母"; return false; }

        // 3. 含 URL / 邮箱 / 手机号特征
        if (name.find("http")  != std::string::npos ||
            name.find("www.")  != std::string::npos ||
            name.find(".com")  != std::string::npos ||
            name.find(".cn")   != std::string::npos ||
            name.find('@')     != std::string::npos ||
            std::strpbrk(name.c_str(), "1234567890") != nullptr && name.length() >= 11) {
            // 只有 >=11 位的纯数字才拦截 (可能是手机号)
            // 进一步检查: 如果数字占比 >70% 且长度 >=11, 拦截
            int digits = 0;
            for (char c : name) if (c >= '0' && c <= '9') digits++;
            if (digits >= 11 || (digits >= 8 && (double)digits/name.length() > 0.7)) {
                reason = "用户名不能包含联系方式"; return false;
            }
        }

        // 4. 连续重复字符 (如 "aaaaaa")
        int maxRepeat = 1, curRepeat = 1;
        for (size_t i = 1; i < name.length(); ++i) {
            if (name[i] == name[i-1]) curRepeat++;
            else { maxRepeat = std::max(maxRepeat, curRepeat); curRepeat = 1; }
        }
        maxRepeat = std::max(maxRepeat, curRepeat);
        if (maxRepeat > 5) { reason = "用户名不能重复超过 5 个相同字符"; return false; }

        // 5. DFA 敏感词匹配
        size_t state = 0;
        for (char c : name) {
            char lower = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
            state = transition(state, lower);
            if (state > 0 && isTerminal(state)) {
                reason = "用户名含不当内容";
                return false;
            }
        }

        return true;
    }

private:
    NameFilter() { buildTrie(); }

    // ── 敏感词表 ──────────────────────
    struct Node {
        bool terminal = false;
        int next[256] = {};
    };
    Node m_nodes[2048];
    int  m_nodeCount = 1;

    void addWord(const char* word) {
        size_t idx = 0;
        for (const char* p = word; *p; ++p) {
            uint8_t c = (uint8_t)*p;
            if (m_nodes[idx].next[c] == 0)
                m_nodes[idx].next[c] = m_nodeCount++;
            idx = m_nodes[idx].next[c];
        }
        m_nodes[idx].terminal = true;
    }

    int transition(size_t state, char c) const {
        return m_nodes[state].next[(uint8_t)c];
    }

    bool isTerminal(size_t state) const {
        return m_nodes[state].terminal;
    }

    void buildTrie() {
        // 基础脏话
        addWord("fuck");  addWord("shit");  addWord("damn");
        addWord("bitch"); addWord("ass");   addWord("dick");
        addWord("porn");  addWord("sex");   addWord("nude");
        // 中文违禁词
        addWord("操你"); addWord("傻逼"); addWord("sb");
        addWord("cnm"); addWord("cao"); addWord("tmd");
        addWord("鸡巴"); addWord("妈的"); addWord("你妈");
        addWord("习近平"); addWord("毛泽东"); addWord("邓小平");
        addWord("江泽民"); addWord("胡锦涛"); addWord("温家宝");
        addWord("李克强"); addWord("共产党"); addWord("中共");
        addWord("台独"); addWord("藏独"); addWord("疆独");
        addWord("法轮功"); addWord("六四"); addWord("天安门");
        addWord("admin"); addWord("root"); addWord("系统");
        addWord("管理员"); addWord("版主"); addWord("官方");
        addWord("gm"); addWord("客服"); addWord("测试");
        // 诈骗相关
        addWord("代充"); addWord("充值"); addWord("刷钱");
        addWord("外挂"); addWord("脚本"); addWord("辅助");
        addWord("出售"); addWord("购买"); addWord("转账");
        addWord("微信"); addWord("qq"); addWord("扣扣");
        // 色情相关中文
        addWord("裸体"); addWord("色情"); addWord("黄色");
        addWord("约炮"); addWord("一夜情"); addWord("小姐");
        addWord("赌博"); addWord("赌场"); addWord("彩票");
    }
};

#endif
