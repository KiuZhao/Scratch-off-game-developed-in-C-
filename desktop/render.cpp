#include "render.h"
#include <cstdio>
#include <cmath>

// ── 辅助: 画圆角矩形 ────────────────
static void drawRoundedRect(SDL_Renderer* r, const SDL_Rect& rect, int radius) {
    // 用简单的矩形 + 圆角近似 (小圆角直接用填充矩形)
    SDL_RenderFillRect(r, &rect);
    (void)radius;
}

// ── 辅助: 画填充圆 ──────────────────
static void drawFilledCircle(SDL_Renderer* r, int cx, int cy, int radius) {
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx*dx + dy*dy <= radius*radius) {
                SDL_RenderDrawPoint(r, cx + dx, cy + dy);
            }
        }
    }
}

// ── 辅助: 绘制边框矩形 ──────────────
static void drawBorder(SDL_Renderer* r, const SDL_Rect& rect, Color c, int thickness) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_Rect line;
    line.x = rect.x; line.y = rect.y; line.w = rect.w; line.h = thickness;
    SDL_RenderFillRect(r, &line);
    line.y = rect.y + rect.h - thickness;
    SDL_RenderFillRect(r, &line);
    line.x = rect.x; line.y = rect.y; line.w = thickness; line.h = rect.h;
    SDL_RenderFillRect(r, &line);
    line.x = rect.x + rect.w - thickness;
    SDL_RenderFillRect(r, &line);
}

// ── 辅助: 格式化金额显示 ────────────
static std::string fmtMoney(int64_t amount) {
    if (amount >= 10000) {
        char buf[64];
        double wan = amount / 10000.0;
        snprintf(buf, sizeof(buf), "%.1f万", wan);
        return buf;
    }
    return std::to_string(amount);
}

// =====================================================================
//  Renderer 实现
// =====================================================================

Renderer::Renderer(SDL_Window* win) {
    m_renderer = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
    m_fontSmall  = nullptr;
    m_fontNormal = nullptr;
    m_fontLarge  = nullptr;
    m_fontTitle  = nullptr;
}

Renderer::~Renderer() {
    if (m_coatingTex) SDL_DestroyTexture(m_coatingTex);
    if (m_fontSmall)  TTF_CloseFont(m_fontSmall);
    if (m_fontNormal) TTF_CloseFont(m_fontNormal);
    if (m_fontLarge)  TTF_CloseFont(m_fontLarge);
    if (m_fontTitle)  TTF_CloseFont(m_fontTitle);
    SDL_DestroyRenderer(m_renderer);
}

bool Renderer::initFonts() {
    m_fontSmall  = TTF_OpenFont(FONT_PATH, FONT_SIZE_SMALL);
    m_fontNormal = TTF_OpenFont(FONT_PATH, FONT_SIZE_NORMAL);
    m_fontLarge  = TTF_OpenFont(FONT_PATH, FONT_SIZE_LARGE);
    m_fontTitle  = TTF_OpenFont(FONT_PATH, FONT_SIZE_TITLE);
    if (!m_fontNormal) {
        // 尝试备选字体
        m_fontSmall  = TTF_OpenFont("C:\\Windows\\Fonts\\simhei.ttf", FONT_SIZE_SMALL);
        m_fontNormal = TTF_OpenFont("C:\\Windows\\Fonts\\simhei.ttf", FONT_SIZE_NORMAL);
        m_fontLarge  = TTF_OpenFont("C:\\Windows\\Fonts\\simhei.ttf", FONT_SIZE_LARGE);
        m_fontTitle  = TTF_OpenFont("C:\\Windows\\Fonts\\simhei.ttf", FONT_SIZE_TITLE);
    }
    return m_fontNormal != nullptr;
}

SDL_Texture* Renderer::makeColorTexture(Color c, int w, int h) {
    SDL_Texture* tex = SDL_CreateTexture(m_renderer,
        SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, w, h);
    SDL_SetRenderTarget(m_renderer, tex);
    SDL_SetRenderDrawColor(m_renderer, c.r, c.g, c.b, c.a);
    SDL_RenderClear(m_renderer);
    SDL_SetRenderTarget(m_renderer, nullptr);
    return tex;
}

SDL_Texture* Renderer::renderText(const char* text, Color fg, int fontSize) {
    int w, h;
    return renderText(text, fg, fontSize, 0, w, h);
}

SDL_Texture* Renderer::renderText(const char* text, Color fg, int fontSize,
                                   int maxW, int& outW, int& outH) {
    TTF_Font* font = m_fontNormal;
    if (fontSize == FONT_SIZE_SMALL)  font = m_fontSmall;
    else if (fontSize == FONT_SIZE_LARGE)  font = m_fontLarge;
    else if (fontSize == FONT_SIZE_TITLE)  font = m_fontTitle;

    if (!font) return nullptr;

    SDL_Color sdlColor = {fg.r, fg.g, fg.b, fg.a};
    SDL_Surface* surf = nullptr;

    if (maxW > 0) {
        surf = TTF_RenderUTF8_Blended_Wrapped(font, text, sdlColor, (Uint32)maxW);
    } else {
        surf = TTF_RenderUTF8_Blended(font, text, sdlColor);
    }

    if (!surf) return nullptr;

    outW = surf->w;
    outH = surf->h;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(m_renderer, surf);
    SDL_FreeSurface(surf);
    return tex;
}

// ── 获取格子在网格中的位置 ──────────
SDL_Rect Renderer::getCellRect(int idx) const {
    int col = idx % GRID_COLS;
    int row = idx / GRID_COLS;
    return {
        m_gridStartX + col * (CELL_W + CELL_GAP),
        m_gridStartY + row * (CELL_H + CELL_GAP),
        CELL_W, CELL_H
    };
}

// ── 绘制单个格子 (下层: 图案+金额) ──
void Renderer::drawCell(int idx, const CellResult& cell, int startX, int startY,
                         SDL_Texture* coinTex) {
    SDL_Rect r = {startX, startY, CELL_W, CELL_H};

    // 上半部分: 中奖/未中奖图案
    SDL_Rect top = {r.x + 3, r.y + 3, r.w - 6, r.h / 2 - 3};
    if (cell.isWin) {
        SDL_SetRenderDrawColor(m_renderer, C_WIN.r, C_WIN.g, C_WIN.b, C_WIN.a);
        SDL_RenderFillRect(m_renderer, &top);

        SDL_Texture* winTex = renderText("中奖!", C_RED, FONT_SIZE_SMALL);
        if (winTex) {
            int tw, th;
            SDL_QueryTexture(winTex, nullptr, nullptr, &tw, &th);
            SDL_Rect dst = {top.x + (top.w - tw)/2, top.y + (top.h - th)/2, tw, th};
            SDL_RenderCopy(m_renderer, winTex, nullptr, &dst);
            SDL_DestroyTexture(winTex);
        }
    } else {
        SDL_SetRenderDrawColor(m_renderer, C_LOSE.r, C_LOSE.g, C_LOSE.b, C_LOSE.a);
        SDL_RenderFillRect(m_renderer, &top);

        SDL_Texture* loseTex = renderText("未中奖", C_WHITE, FONT_SIZE_SMALL);
        if (loseTex) {
            int tw, th;
            SDL_QueryTexture(loseTex, nullptr, nullptr, &tw, &th);
            SDL_Rect dst = {top.x + (top.w - tw)/2, top.y + (top.h - th)/2, tw, th};
            SDL_RenderCopy(m_renderer, loseTex, nullptr, &dst);
            SDL_DestroyTexture(loseTex);
        }
    }

    // 下半部分: 金额
    SDL_Rect bot = {r.x + 3, r.y + r.h/2 + 1, r.w - 6, r.h/2 - 4};
    SDL_SetRenderDrawColor(m_renderer, C_AMOUNT.r, C_AMOUNT.g, C_AMOUNT.b, C_AMOUNT.a);
    SDL_RenderFillRect(m_renderer, &bot);

    // 金额文字 (displayPrize: 中奖=实际奖金, 未中奖=随机展示金额)
    std::string amtStr = fmtMoney(cell.displayPrize);
    SDL_Texture* amtTex = renderText(amtStr.c_str(), C_GOLD, FONT_SIZE_SMALL);
    if (amtTex) {
        int tw, th;
        SDL_QueryTexture(amtTex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst = {bot.x + (bot.w - tw)/2, bot.y + (bot.h - th)/2, tw, th};
        SDL_RenderCopy(m_renderer, amtTex, nullptr, &dst);
        SDL_DestroyTexture(amtTex);
    }

    // 格子边框
    drawBorder(m_renderer, r, Color{80, 80, 100, 255}, 1);
}

// ── 创建刮涂层纹理 ──────────────────
void Renderer::createCoatingTexture(const CardInstance& card) {
    if (m_coatingTex) {
        SDL_DestroyTexture(m_coatingTex);
        m_coatingTex = nullptr;
    }

    int rows = (card.config.positions + GRID_COLS - 1) / GRID_COLS;
    int gridW = GRID_COLS * CELL_W + (GRID_COLS - 1) * CELL_GAP;
    int gridH = rows * CELL_H + (rows - 1) * CELL_GAP;

    m_gridStartX = (WIN_W - gridW) / 2;
    m_gridStartY = (WIN_H - gridH) / 2 - 20;
    m_gridRows   = rows;

    m_coatingTex = SDL_CreateTexture(m_renderer,
        SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, gridW, gridH);
    SDL_SetTextureBlendMode(m_coatingTex, SDL_BLENDMODE_BLEND);

    // 填充涂层颜色
    SDL_SetRenderTarget(m_renderer, m_coatingTex);
    SDL_SetRenderDrawColor(m_renderer, C_COATING.r, C_COATING.g, C_COATING.b, C_COATING.a);
    SDL_RenderClear(m_renderer);

    // 画格线
    SDL_SetRenderDrawColor(m_renderer, 140, 130, 120, 255);
    for (int i = 0; i < card.config.positions; ++i) {
        int col = i % GRID_COLS;
        int row = i / GRID_COLS;
        SDL_Rect cr = {
            col * (CELL_W + CELL_GAP),
            row * (CELL_H + CELL_GAP),
            CELL_W, CELL_H
        };
        SDL_RenderDrawRect(m_renderer, &cr);
    }

    SDL_SetRenderTarget(m_renderer, nullptr);
}

// ── 刮开操作 ────────────────────────
void Renderer::scratchAt(int cellIdx, int localX, int localY, int brushRadius,
                          CardInstance& card) {
    if (cellIdx < 0 || cellIdx >= (int)card.cells.size()) return;
    auto& cell = card.cells[cellIdx];

    // 更新掩码
    int col0  = localX * MASK_COLS / CELL_W;
    int row0  = localY * MASK_ROWS / CELL_H;
    int bRadCols = brushRadius * MASK_COLS / CELL_W;
    int bRadRows = brushRadius * MASK_ROWS / CELL_H;

    for (int mr = row0 - bRadRows; mr <= row0 + bRadRows; ++mr) {
        for (int mc = col0 - bRadCols; mc <= col0 + bRadCols; ++mc) {
            if (mc >= 0 && mc < MASK_COLS && mr >= 0 && mr < MASK_ROWS) {
                int dc = mc - col0, dr = mr - row0;
                if (dc*dc * CELL_W*CELL_W + dr*dr * CELL_H*CELL_H
                        <= brushRadius*brushRadius * 4) {
                    if (!cell.mask[mr][mc]) {
                        cell.mask[mr][mc] = true;
                        cell.revealedCount++;
                    }
                }
            }
        }
    }

    // 更新涂层纹理
    if (!m_coatingTex) return;
    SDL_Rect cr = getCellRect(cellIdx);
    // 计算在涂层纹理中的位置
    int texX = cr.x - m_gridStartX;
    int texY = cr.y - m_gridStartY;

    SDL_SetRenderTarget(m_renderer, m_coatingTex);
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 0);
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);

    // 擦除刮开区域的涂层 (绘制透明)
    // 对所有已刮开的mask位置, 在涂层纹理上打洞
    for (int mr = 0; mr < MASK_ROWS; ++mr) {
        for (int mc = 0; mc < MASK_COLS; ++mc) {
            if (cell.mask[mr][mc]) {
                int px = texX + mc * CELL_W / MASK_COLS;
                int py = texY + mr * CELL_H / MASK_ROWS;
                int pw = CELL_W / MASK_COLS + 1;
                int ph = CELL_H / MASK_ROWS + 1;
                SDL_Rect hole = {px, py, pw, ph};
                SDL_RenderFillRect(m_renderer, &hole);
            }
        }
    }

    SDL_SetRenderTarget(m_renderer, nullptr);
}

// ── 检查刮开比例 ────────────────────
bool Renderer::checkRevealPercent(const CardInstance& card) {
    int total = 0, revealed = 0;
    for (auto& cell : card.cells) {
        total    += cell.totalCount;
        revealed += cell.revealedCount;
    }
    if (total == 0) return false;
    return (double)revealed / total >= SCRATCH_REVEAL_THRESHOLD;
}

// ── 绘制主选购页面 ──────────────────
void Renderer::drawMainMenu(const SaveData& save, int hoveredCard, int selectedCard,
                             bool hoverBuy, bool hoverSave, bool hoverLoad,
                             SDL_Texture* coinTex, SDL_Texture* cardTex[3]) {
    // 背景
    SDL_SetRenderDrawColor(m_renderer, C_BG.r, C_BG.g, C_BG.b, C_BG.a);
    SDL_RenderClear(m_renderer);

    auto types = getCardTypes();

    // ── 顶栏 ────────────────────────
    // 左上: 作弊按钮 "开滴滴"
    SDL_Rect cheatBtn = {10, 10, 90, 36};
    Color cbCol = C_BUTTON;
    SDL_SetRenderDrawColor(m_renderer, cbCol.r, cbCol.g, cbCol.b, cbCol.a);
    SDL_RenderFillRect(m_renderer, &cheatBtn);
    drawBorder(m_renderer, cheatBtn, C_WHITE, 1);

    SDL_Texture* cheatTex = renderText("开滴滴", C_WHITE, FONT_SIZE_SMALL);
    if (cheatTex) {
        int tw, th;
        SDL_QueryTexture(cheatTex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst = {cheatBtn.x + (cheatBtn.w - tw)/2,
                        cheatBtn.y + (cheatBtn.h - th)/2, tw, th};
        SDL_RenderCopy(m_renderer, cheatTex, nullptr, &dst);
        SDL_DestroyTexture(cheatTex);
    }

    // ── 顶部中央: 纪念品 + 持有方斯 ──
    const int SOUV_ICON_SIZE = 40;
    const int SOUV_GAP       = 80;   // 图标中心间距
    // 三个纪念品图标居中偏左, 余额在右侧
    int souvCenterX = WIN_W / 2 - 40;
    int souvY       = 5;

    for (int i = 0; i < 3; ++i) {
        int sx = souvCenterX - (3 * SOUV_GAP) / 2 + i * SOUV_GAP;
        SDL_Rect iconRect = {sx, souvY, SOUV_ICON_SIZE, SOUV_ICON_SIZE};
        if (cardTex[i]) {
            SDL_RenderCopy(m_renderer, cardTex[i], nullptr, &iconRect);
        } else {
            SDL_SetRenderDrawColor(m_renderer, types[i].bgColor.r,
                types[i].bgColor.g, types[i].bgColor.b, 255);
            SDL_RenderFillRect(m_renderer, &iconRect);
        }
        // 数字放在图片下方居中
        std::string cnt = "x" + std::to_string(save.souvenirs[i]);
        SDL_Texture* cntTex = renderText(cnt.c_str(), C_WHITE, FONT_SIZE_SMALL);
        if (cntTex) {
            int tw, th;
            SDL_QueryTexture(cntTex, nullptr, nullptr, &tw, &th);
            SDL_Rect dst = {sx + (SOUV_ICON_SIZE - tw) / 2,
                            souvY + SOUV_ICON_SIZE + 2, tw, th};
            SDL_RenderCopy(m_renderer, cntTex, nullptr, &dst);
            SDL_DestroyTexture(cntTex);
        }
    }

    // 持有方斯 — 放在纪念品右侧
    int balanceX = souvCenterX + (3 * SOUV_GAP) / 2 + 20;
    std::string balText = "持有: " + fmtMoney(save.balance) + "方斯";
    SDL_Texture* balTex = renderText(balText.c_str(), C_GOLD, FONT_SIZE_SMALL);
    if (balTex) {
        int tw, th;
        SDL_QueryTexture(balTex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst = {balanceX, souvY + (SOUV_ICON_SIZE - th) / 2, tw, th};
        SDL_RenderCopy(m_renderer, balTex, nullptr, &dst);
        SDL_DestroyTexture(balTex);
    }

    // 右上: 盈利/亏损
    std::string plText;
    if (save.profit > 0)
        plText = "你已经盈利了" + fmtMoney(save.profit) + "方斯";
    else if (save.profit < 0)
        plText = "你已经亏损了" + fmtMoney(-save.profit) + "方斯";
    else
        plText = "你目前不赚不赔";

    SDL_Texture* plTex = renderText(plText.c_str(), C_GOLD, FONT_SIZE_SMALL);
    if (plTex) {
        int tw, th;
        SDL_QueryTexture(plTex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst = {WIN_W - tw - 20, 15, tw, th};
        SDL_RenderCopy(m_renderer, plTex, nullptr, &dst);
        SDL_DestroyTexture(plTex);
    }

    // ── 三张卡片 ────────────────────
    int cardW = 200, cardH = 340;
    int totalW = 3 * cardW + 2 * 40;
    int startX = (WIN_W - totalW) / 2;
    int cardY  = 90;

    for (int i = 0; i < 3; ++i) {
        int cx = startX + i * (cardW + 40);
        SDL_Rect cardRect = {cx, cardY, cardW, cardH};
        bool hovered   = (i == hoveredCard);
        bool selected  = (i == selectedCard);

        // 卡片背景
        Color bg = types[i].bgColor;
        if (hovered) {
            bg.r = (Uint8)std::min(255, bg.r + 40);
            bg.g = (Uint8)std::min(255, bg.g + 40);
            bg.b = (Uint8)std::min(255, bg.b + 40);
        } else if (selected) {
            bg.r = (Uint8)std::min(255, bg.r + 20);
            bg.g = (Uint8)std::min(255, bg.g + 20);
            bg.b = (Uint8)std::min(255, bg.b + 20);
        }
        SDL_SetRenderDrawColor(m_renderer, bg.r, bg.g, bg.b, bg.a);
        SDL_RenderFillRect(m_renderer, &cardRect);

        Color borderColor = C_WHITE;
        int   borderThick = 1;
        if (hovered) {
            borderColor = C_GOLD;
            borderThick = 3;
        } else if (selected) {
            borderColor = Color{255, 180, 50, 255};  // 选中状态: 橙色边框
            borderThick = 3;
        }
        drawBorder(m_renderer, cardRect, borderColor, borderThick);

        // 刮刮乐占位图 (正方形)
        SDL_Rect imgRect = {cx + 25, cardY + 20, 150, 150};
        if (cardTex[i]) {
            SDL_RenderCopy(m_renderer, cardTex[i], nullptr, &imgRect);
        } else {
            SDL_SetRenderDrawColor(m_renderer, 30, 30, 50, 255);
            SDL_RenderFillRect(m_renderer, &imgRect);

            SDL_Texture* placeholder = renderText(types[i].name, C_GOLD, FONT_SIZE_LARGE);
            if (placeholder) {
                int tw, th;
                SDL_QueryTexture(placeholder, nullptr, nullptr, &tw, &th);
                SDL_Rect dst = {imgRect.x + (imgRect.w - tw)/2,
                                imgRect.y + (imgRect.h - th)/2, tw, th};
                SDL_RenderCopy(m_renderer, placeholder, nullptr, &dst);
                SDL_DestroyTexture(placeholder);
            }
        }

        // 名称 (上方)
        SDL_Texture* nameTex = renderText(types[i].name, C_WHITE, FONT_SIZE_NORMAL);
        if (nameTex) {
            int tw, th;
            SDL_QueryTexture(nameTex, nullptr, nullptr, &tw, &th);
            SDL_Rect dst = {cx + (cardW - tw)/2, cardY + 180, tw, th};
            SDL_RenderCopy(m_renderer, nameTex, nullptr, &dst);
            SDL_DestroyTexture(nameTex);
        }

        // 机会提示
        char chanceBuf[64];
        snprintf(chanceBuf, sizeof(chanceBuf), "%d次刮开机会", types[i].positions);
        SDL_Texture* chanceTex = renderText(chanceBuf, Color{200,200,200,255}, FONT_SIZE_SMALL);
        if (chanceTex) {
            int tw, th;
            SDL_QueryTexture(chanceTex, nullptr, nullptr, &tw, &th);
            SDL_Rect dst = {cx + (cardW - tw)/2, cardY + 205, tw, th};
            SDL_RenderCopy(m_renderer, chanceTex, nullptr, &dst);
            SDL_DestroyTexture(chanceTex);
        }

        // 价格区域
        int priceY = cardY + cardH - 55;
        SDL_Rect priceBg = {cx, priceY, cardW, 55};
        SDL_SetRenderDrawColor(m_renderer, 20, 20, 40, 255);
        SDL_RenderFillRect(m_renderer, &priceBg);

        // 货币图标 (左侧)
        SDL_Rect coinIcon = {cx + 20, priceY + 8, 36, 36};
        if (coinTex) {
            SDL_RenderCopy(m_renderer, coinTex, nullptr, &coinIcon);
        } else {
            SDL_SetRenderDrawColor(m_renderer, C_GOLD.r, C_GOLD.g, C_GOLD.b, 255);
            SDL_RenderFillRect(m_renderer, &coinIcon);
        }

        // 价格数字 (右侧)
        std::string priceStr = fmtMoney(types[i].cost);
        SDL_Texture* priceTex = renderText(priceStr.c_str(), C_GOLD, FONT_SIZE_LARGE);
        if (priceTex) {
            int tw, th;
            SDL_QueryTexture(priceTex, nullptr, nullptr, &tw, &th);
            SDL_Rect dst = {cx + cardW - tw - 25, priceY + (55 - th)/2, tw, th};
            SDL_RenderCopy(m_renderer, priceTex, nullptr, &dst);
            SDL_DestroyTexture(priceTex);
        }
    }

    // ── 底部 ────────────────────────
    // 左下: 提示
    SDL_Texture* tipTex = renderText("仅供娱乐，理性购买", Color{180,180,180,255}, FONT_SIZE_SMALL);
    if (tipTex) {
        int tw, th;
        SDL_QueryTexture(tipTex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst = {20, WIN_H - th - 15, tw, th};
        SDL_RenderCopy(m_renderer, tipTex, nullptr, &dst);
        SDL_DestroyTexture(tipTex);
    }

    // 底部中央: 保存 / 读取 按钮
    auto drawBottomBtn = [&](int bx, int by, int bw, int bh, const char* label,
                              bool hovered) {
        Color col = hovered ? C_BUTTON_H : C_BUTTON;
        SDL_SetRenderDrawColor(m_renderer, col.r, col.g, col.b, col.a);
        SDL_Rect r = {bx, by, bw, bh};
        SDL_RenderFillRect(m_renderer, &r);
        drawBorder(m_renderer, r, C_WHITE, 2);

        SDL_Texture* t = renderText(label, C_WHITE, FONT_SIZE_SMALL);
        if (t) {
            int tw, th;
            SDL_QueryTexture(t, nullptr, nullptr, &tw, &th);
            SDL_Rect dst = {bx + (bw - tw)/2, by + (bh - th)/2, tw, th};
            SDL_RenderCopy(m_renderer, t, nullptr, &dst);
            SDL_DestroyTexture(t);
        }
    };

    int btnY = WIN_H - 55;
    drawBottomBtn(WIN_W / 2 - 160, btnY, 100, 40, "保存", hoverSave);
    drawBottomBtn(WIN_W / 2 -  50, btnY, 100, 40, "读取", hoverLoad);

    // 右下: 购买按钮
    SDL_Rect buyBtn = {WIN_W - 150, WIN_H - 55, 130, 40};
    Color buyCol = hoverBuy ? C_BUTTON_H : C_BUTTON;
    SDL_SetRenderDrawColor(m_renderer, buyCol.r, buyCol.g, buyCol.b, buyCol.a);
    SDL_RenderFillRect(m_renderer, &buyBtn);
    drawBorder(m_renderer, buyBtn, C_WHITE, 2);

    SDL_Texture* buyTex = renderText("购买", C_WHITE, FONT_SIZE_NORMAL);
    if (buyTex) {
        int tw, th;
        SDL_QueryTexture(buyTex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst = {buyBtn.x + (buyBtn.w - tw)/2,
                        buyBtn.y + (buyBtn.h - th)/2, tw, th};
        SDL_RenderCopy(m_renderer, buyTex, nullptr, &dst);
        SDL_DestroyTexture(buyTex);
    }
}

// ── 绘制刮奖页面 ────────────────────
void Renderer::drawScratchPage(const CardInstance& card, const SaveData& save,
                                bool hoverRevealAll, SDL_Texture* coinTex) {
    // 背景
    SDL_SetRenderDrawColor(m_renderer, C_BG.r, C_BG.g, C_BG.b, C_BG.a);
    SDL_RenderClear(m_renderer);

    // ── 顶栏 ──
    // 标题
    std::string title = std::string(card.config.name) + " - 刮开涂层!";
    SDL_Texture* titleTex = renderText(title.c_str(), C_GOLD, FONT_SIZE_LARGE);
    if (titleTex) {
        int tw, th;
        SDL_QueryTexture(titleTex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst = {(WIN_W - tw)/2, 10, tw, th};
        SDL_RenderCopy(m_renderer, titleTex, nullptr, &dst);
        SDL_DestroyTexture(titleTex);
    }

    // 右上: 余额
    std::string balText = "余额: " + fmtMoney(save.balance) + "方斯";
    SDL_Texture* balTex = renderText(balText.c_str(), C_GOLD, FONT_SIZE_SMALL);
    if (balTex) {
        int tw, th;
        SDL_QueryTexture(balTex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst = {WIN_W - tw - 20, 15, tw, th};
        SDL_RenderCopy(m_renderer, balTex, nullptr, &dst);
        SDL_DestroyTexture(balTex);
    }

    // ── 绘制格子下层 ──
    for (int i = 0; i < (int)card.cells.size(); ++i) {
        SDL_Rect cr = getCellRect(i);
        drawCell(i, card.cells[i], cr.x, cr.y, coinTex);
    }

    // ── 绘制涂层 ──
    if (m_coatingTex) {
        int gridW = GRID_COLS * CELL_W + (GRID_COLS - 1) * CELL_GAP;
        int gridH = m_gridRows * CELL_H + (m_gridRows - 1) * CELL_GAP;
        SDL_Rect coatingRect = {m_gridStartX, m_gridStartY, gridW, gridH};
        SDL_RenderCopy(m_renderer, m_coatingTex, nullptr, &coatingRect);
    }

    // ── 底部 ──
    // 刮开进度
    int totalMask = 0, revealedMask = 0;
    for (auto& c : card.cells) {
        totalMask    += c.totalCount;
        revealedMask += c.revealedCount;
    }
    double pct = totalMask > 0 ? 100.0 * revealedMask / totalMask : 0;
    char progBuf[64];
    snprintf(progBuf, sizeof(progBuf), "刮开进度: %.1f%%", pct);
    SDL_Texture* progTex = renderText(progBuf, C_WHITE, FONT_SIZE_SMALL);
    if (progTex) {
        int tw, th;
        SDL_QueryTexture(progTex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst = {20, WIN_H - th - 15, tw, th};
        SDL_RenderCopy(m_renderer, progTex, nullptr, &dst);
        SDL_DestroyTexture(progTex);
    }

    // 一键全部刮开按钮
    SDL_Rect revealBtn = {WIN_W - 170, WIN_H - 55, 150, 40};
    Color rvCol = hoverRevealAll ? C_BUTTON_H : C_BUTTON;
    SDL_SetRenderDrawColor(m_renderer, rvCol.r, rvCol.g, rvCol.b, rvCol.a);
    SDL_RenderFillRect(m_renderer, &revealBtn);
    drawBorder(m_renderer, revealBtn, C_WHITE, 2);

    SDL_Texture* rvTex = renderText("一键全部刮开", C_WHITE, FONT_SIZE_SMALL);
    if (rvTex) {
        int tw, th;
        SDL_QueryTexture(rvTex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst = {revealBtn.x + (revealBtn.w - tw)/2,
                        revealBtn.y + (revealBtn.h - th)/2, tw, th};
        SDL_RenderCopy(m_renderer, rvTex, nullptr, &dst);
        SDL_DestroyTexture(rvTex);
    }
}

// ── 结果弹窗 ────────────────────────
void Renderer::drawResultPopup(const CardInstance& card, SDL_Texture* coinTex) {
    // 半透明遮罩
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, C_OVERLAY.r, C_OVERLAY.g, C_OVERLAY.b, C_OVERLAY.a);
    SDL_Rect full = {0, 0, WIN_W, WIN_H};
    SDL_RenderFillRect(m_renderer, &full);
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);

    // 弹窗
    int popW = 500, popH = 400;
    SDL_Rect pop = {(WIN_W - popW)/2, (WIN_H - popH)/2, popW, popH};
    SDL_SetRenderDrawColor(m_renderer, C_POPUP_BG.r, C_POPUP_BG.g, C_POPUP_BG.b, C_POPUP_BG.a);
    SDL_RenderFillRect(m_renderer, &pop);
    drawBorder(m_renderer, pop, C_GOLD, 3);

    // 标题
    const char* titleText = card.totalPrize > 0 ? "恭喜中奖!" : "很遗憾";
    SDL_Texture* pTitleTex = renderText(titleText, C_GOLD, FONT_SIZE_TITLE);
    if (pTitleTex) {
        int tw, th;
        SDL_QueryTexture(pTitleTex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst = {pop.x + (popW - tw)/2, pop.y + 20, tw, th};
        SDL_RenderCopy(m_renderer, pTitleTex, nullptr, &dst);
        SDL_DestroyTexture(pTitleTex);
    }

    // 纪念品图案 (顶部中央)
    SDL_Rect souvenirRect = {pop.x + (popW - 100)/2, pop.y + 65, 100, 100};
    auto types = getCardTypes();
    SDL_SetRenderDrawColor(m_renderer,
        types[card.cardTypeIndex].bgColor.r,
        types[card.cardTypeIndex].bgColor.g,
        types[card.cardTypeIndex].bgColor.b, 255);
    SDL_RenderFillRect(m_renderer, &souvenirRect);
    drawBorder(m_renderer, souvenirRect, C_WHITE, 2);

    SDL_Texture* sovTex = renderText("纪念品", C_WHITE, FONT_SIZE_SMALL);
    if (sovTex) {
        int tw, th;
        SDL_QueryTexture(sovTex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst = {souvenirRect.x + (souvenirRect.w - tw)/2,
                        souvenirRect.y + (souvenirRect.h - th)/2, tw, th};
        SDL_RenderCopy(m_renderer, sovTex, nullptr, &dst);
        SDL_DestroyTexture(sovTex);
    }

    // 中奖金额 (如有)
    int yOff = 185;
    if (card.totalPrize > 0) {
        // 货币图标
        SDL_Rect coinIcon = {pop.x + 120, pop.y + yOff, 32, 32};
        if (coinTex) {
            SDL_RenderCopy(m_renderer, coinTex, nullptr, &coinIcon);
        } else {
            SDL_SetRenderDrawColor(m_renderer, C_GOLD.r, C_GOLD.g, C_GOLD.b, 255);
            SDL_RenderFillRect(m_renderer, &coinIcon);
        }

        std::string prizeStr = "中奖金额: " + fmtMoney(card.totalPrize) + "方斯";
        SDL_Texture* prizeTex = renderText(prizeStr.c_str(), C_GOLD, FONT_SIZE_LARGE);
        if (prizeTex) {
            int tw, th;
            SDL_QueryTexture(prizeTex, nullptr, nullptr, &tw, &th);
            SDL_Rect dst = {pop.x + 165, pop.y + yOff + (32 - th)/2, tw, th};
            SDL_RenderCopy(m_renderer, prizeTex, nullptr, &dst);
            SDL_DestroyTexture(prizeTex);
        }
        yOff += 50;
    }

    // 提示文字
    SDL_Texture* hintTex = renderText("点击任意位置返回", Color{180,180,180,255}, FONT_SIZE_SMALL);
    if (hintTex) {
        int tw, th;
        SDL_QueryTexture(hintTex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst = {pop.x + (popW - tw)/2, pop.y + popH - 40, tw, th};
        SDL_RenderCopy(m_renderer, hintTex, nullptr, &dst);
        SDL_DestroyTexture(hintTex);
    }
}

// ── 作弊弹窗 ────────────────────────
void Renderer::drawCheatPopup(const char* message, int amount) {
    // 遮罩
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, C_OVERLAY.r, C_OVERLAY.g, C_OVERLAY.b, C_OVERLAY.a);
    SDL_Rect full = {0, 0, WIN_W, WIN_H};
    SDL_RenderFillRect(m_renderer, &full);
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);

    int popW = 500, popH = 220;
    SDL_Rect pop = {(WIN_W - popW)/2, (WIN_H - popH)/2, popW, popH};
    SDL_SetRenderDrawColor(m_renderer, C_POPUP_BG.r, C_POPUP_BG.g, C_POPUP_BG.b, C_POPUP_BG.a);
    SDL_RenderFillRect(m_renderer, &pop);
    drawBorder(m_renderer, pop, C_GOLD, 3);

    // 消息
    SDL_Texture* msgTex = renderText(message, C_WHITE, FONT_SIZE_LARGE);
    if (msgTex) {
        int tw, th;
        SDL_QueryTexture(msgTex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst = {pop.x + (popW - tw)/2, pop.y + 30, tw, th};
        SDL_RenderCopy(m_renderer, msgTex, nullptr, &dst);
        SDL_DestroyTexture(msgTex);
    }

    // 金额
    if (amount > 0) {
        std::string amtStr = "收入 " + fmtMoney(amount) + " 方斯";
        SDL_Texture* amtTex = renderText(amtStr.c_str(), C_GOLD, FONT_SIZE_LARGE);
        if (amtTex) {
            int tw, th;
            SDL_QueryTexture(amtTex, nullptr, nullptr, &tw, &th);
            SDL_Rect dst = {pop.x + (popW - tw)/2, pop.y + 80, tw, th};
            SDL_RenderCopy(m_renderer, amtTex, nullptr, &dst);
            SDL_DestroyTexture(amtTex);
        }
    }

    // 提示
    SDL_Texture* hintTex = renderText("点击任意位置关闭", Color{180,180,180,255}, FONT_SIZE_SMALL);
    if (hintTex) {
        int tw, th;
        SDL_QueryTexture(hintTex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst = {pop.x + (popW - tw)/2, pop.y + popH - 40, tw, th};
        SDL_RenderCopy(m_renderer, hintTex, nullptr, &dst);
        SDL_DestroyTexture(hintTex);
    }
}

// ── 存档提示 ────────────────────────
void Renderer::drawSaveTip(const char* msg, bool success) {
    Color c = success ? C_GREEN : C_RED;
    SDL_Texture* tex = renderText(msg, c, FONT_SIZE_NORMAL);
    if (tex) {
        int tw, th;
        SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst = {(WIN_W - tw)/2, WIN_H - 50, tw, th};
        SDL_RenderCopy(m_renderer, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    }
    // 注意: 不在此处 Present/Delay, 由主循环统一控制帧率
}

// ── 文件对话框 ────────────────────────
void Renderer::drawFileDialog(char mode, const std::string& path) {
    // 半透明遮罩
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, C_OVERLAY.r, C_OVERLAY.g, C_OVERLAY.b, C_OVERLAY.a);
    SDL_Rect full = {0, 0, WIN_W, WIN_H};
    SDL_RenderFillRect(m_renderer, &full);
    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_NONE);

    int popW = 520, popH = 200;
    SDL_Rect pop = {(WIN_W - popW)/2, (WIN_H - popH)/2, popW, popH};
    SDL_SetRenderDrawColor(m_renderer, C_POPUP_BG.r, C_POPUP_BG.g, C_POPUP_BG.b, C_POPUP_BG.a);
    SDL_RenderFillRect(m_renderer, &pop);
    drawBorder(m_renderer, pop, C_GOLD, 3);

    // 标题
    const char* title = (mode == 'S') ? "保存存档" : "读取存档";
    SDL_Texture* tTex = renderText(title, C_GOLD, FONT_SIZE_TITLE);
    if (tTex) {
        int tw, th;
        SDL_QueryTexture(tTex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst = {pop.x + (popW - tw)/2, pop.y + 15, tw, th};
        SDL_RenderCopy(m_renderer, tTex, nullptr, &dst);
        SDL_DestroyTexture(tTex);
    }

    // 路径标签
    SDL_Texture* lblTex = renderText("文件路径:", C_WHITE, FONT_SIZE_NORMAL);
    int lblW = 0, lblH = 0;
    if (lblTex) {
        SDL_QueryTexture(lblTex, nullptr, nullptr, &lblW, &lblH);
        SDL_Rect dst = {pop.x + 30, pop.y + 65, lblW, lblH};
        SDL_RenderCopy(m_renderer, lblTex, nullptr, &dst);
        SDL_DestroyTexture(lblTex);
    }

    // 输入框背景
    int inputX = pop.x + 30 + lblW + 10;
    int inputW = popW - 70 - lblW;
    SDL_Rect inputBg = {inputX, pop.y + 60, inputW, 35};
    SDL_SetRenderDrawColor(m_renderer, 20, 20, 40, 255);
    SDL_RenderFillRect(m_renderer, &inputBg);
    drawBorder(m_renderer, inputBg, C_WHITE, 1);

    // 当前输入的路径文本
    std::string displayPath = path.empty() ? "guaguale_save.dat" : path;
    // 显示光标
    std::string showText = displayPath + "_";
    SDL_Texture* pathTex = renderText(showText.c_str(), C_GOLD, FONT_SIZE_SMALL);
    if (pathTex) {
        int tw, th;
        SDL_QueryTexture(pathTex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst = {inputX + 5, inputBg.y + (35 - th)/2, tw, th};
        SDL_RenderCopy(m_renderer, pathTex, nullptr, &dst);
        SDL_DestroyTexture(pathTex);
    }

    // 底部提示
    SDL_Texture* hintTex = renderText("Enter=确认  Esc=取消  Backspace=删除",
                                       Color{180,180,180,255}, FONT_SIZE_SMALL);
    if (hintTex) {
        int tw, th;
        SDL_QueryTexture(hintTex, nullptr, nullptr, &tw, &th);
        SDL_Rect dst = {pop.x + (popW - tw)/2, pop.y + popH - 35, tw, th};
        SDL_RenderCopy(m_renderer, hintTex, nullptr, &dst);
        SDL_DestroyTexture(hintTex);
    }
}
