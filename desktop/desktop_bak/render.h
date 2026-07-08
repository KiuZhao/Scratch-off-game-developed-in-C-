#ifndef RENDER_H
#define RENDER_H

#include "config.h"
#include "save.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <string>
#include <vector>

// ── 每个刮开格子的结果 ──────────────
struct CellResult {
    bool  isWin;        // 是否中奖
    int   prize;        // 实际奖金 (方斯, 0=未中奖)
    int   displayPrize; // 展示金额 (未中奖格也随机显示一个金额, 仅展示)
    bool  mask[MASK_ROWS][MASK_COLS] = {};  // false=涂层存在, true=已刮开
    int   revealedCount = 0;
    int   totalCount    = MASK_ROWS * MASK_COLS;
};

// ── 一张刮刮乐的实例 ────────────────
struct CardInstance {
    int cardTypeIndex;                // 0=1万, 1=2万, 2=5万
    CardTypeConfig config;            // 按值存储, 避免悬挂指针
    std::vector<CellResult> cells;
    int  totalPrize = 0;
    bool souvenirAdded = false;       // 纪念品是否已发放
};

// ── 渲染器 ──────────────────────────
class Renderer {
public:
    Renderer(SDL_Window* win);
    ~Renderer();

    bool initFonts();
    SDL_Renderer* getSDLRenderer() { return m_renderer; }

    // ── 纹理工具 ──
    SDL_Texture* makeColorTexture(Color c, int w, int h);
    SDL_Texture* renderText(const char* text, Color fg, int fontSize);
    SDL_Texture* renderText(const char* text, Color fg, int fontSize, int maxW, int& outW, int& outH);

    // ── 主选购页面 ──
    void drawMainMenu(const SaveData& save, int hoveredCard, int selectedCard,
                      bool hoverBuy, bool hoverSave, bool hoverLoad,
                      SDL_Texture* coinTex, SDL_Texture* cardTex[3]);

    // ── 刮奖页面 ──
    void createCoatingTexture(const CardInstance& card);
    void scratchAt(int cellIdx, int localX, int localY, int brushRadius, CardInstance& card);
    bool checkRevealPercent(const CardInstance& card);
    void drawScratchPage(const CardInstance& card, const SaveData& save,
                         bool hoverRevealAll, SDL_Texture* coinTex);

    // ── 弹窗 ──
    void drawResultPopup(const CardInstance& card, SDL_Texture* coinTex);
    void drawCheatPopup(const char* message, int amount);

    // ── 存档提示 ──
    void drawSaveTip(const char* msg, bool success);

    // ── 文件对话框 ──
    void drawFileDialog(char mode, const std::string& path);

private:
    SDL_Renderer* m_renderer;
    TTF_Font* m_fontSmall;
    TTF_Font* m_fontNormal;
    TTF_Font* m_fontLarge;
    TTF_Font* m_fontTitle;

    // 刮涂层纹理 (render target)
    SDL_Texture* m_coatingTex = nullptr;
    int m_gridStartX = 0;
    int m_gridStartY = 0;
    int m_gridRows   = 0;

    void drawCell(int idx, const CellResult& cell, int startX, int startY,
                  SDL_Texture* coinTex);
    SDL_Rect getCellRect(int idx) const;
    void drawCoatingOverCell(int idx, const CellResult& cell);

public:
    // 网格位置 (game.cpp 需要访问)
    int gridStartX() const { return m_gridStartX; }
    int gridStartY() const { return m_gridStartY; }
    int gridRows()   const { return m_gridRows; }
};

#endif
