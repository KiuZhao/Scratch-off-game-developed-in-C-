#include "game.h"
#include <cstdio>
#include <cmath>

Game::Game() : m_rng(std::random_device{}()), m_probDist(0.0, 1.0) {}

Game::~Game() {
    autoSave();
    for (int i = 0; i < 3; ++i)
        if (m_cardTex[i]) SDL_DestroyTexture(m_cardTex[i]);
    if (m_coinTex) SDL_DestroyTexture(m_coinTex);
    delete m_renderer;
    SDL_DestroyWindow(m_window);
    SDL_Quit();
    TTF_Quit();
}

bool Game::init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL init failed: %s\n", SDL_GetError());
        return false;
    }
    if (TTF_Init() < 0) {
        printf("TTF init failed: %s\n", TTF_GetError());
        return false;
    }

    m_window = SDL_CreateWindow(WIN_TITLE,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H, SDL_WINDOW_SHOWN);
    if (!m_window) {
        printf("Window create failed: %s\n", SDL_GetError());
        return false;
    }

    m_renderer = new Renderer(m_window);
    if (!m_renderer->initFonts()) {
        printf("Font init failed\n");
        return false;
    }

    m_saveMgr.autoLoad(m_saveData);

    m_coinTex = m_renderer->makeColorTexture(C_GOLD, 64, 64);
    auto types = getCardTypes();
    for (int i = 0; i < 3; ++i) {
        m_cardTex[i] = m_renderer->makeColorTexture(types[i].bgColor, 128, 128);
    }

    return true;
}

// ── 生成卡片实例 ──────────────────────
CardInstance Game::generateCardInstance(int typeIndex) {
    CardInstance card;
    card.cardTypeIndex = typeIndex;
    auto types = getCardTypes();
    card.config = types[typeIndex];

    auto prizes = getPrizeList(card.config.maxPrize);

    std::vector<double> weights;
    for (int p : prizes) weights.push_back(prizeWeight(p));
    std::discrete_distribution<size_t> prizeDist(weights.begin(), weights.end());
    std::uniform_int_distribution<size_t> uniformPrizeDist(0, prizes.size() - 1);

    card.totalPrize = 0;
    card.cells.resize(card.config.positions);

    for (int i = 0; i < card.config.positions; ++i) {
        card.cells[i].prize = 0;
        card.cells[i].isWin = false;
        if (m_probDist(m_rng) < card.config.winProbPerPos) {
            size_t idx = prizeDist(m_rng);
            int prize = prizes[idx];
            card.cells[i].prize = prize;
            card.cells[i].displayPrize = prize;
            card.cells[i].isWin = true;
            card.totalPrize += prize;
        } else {
            card.cells[i].prize = 0;
            card.cells[i].displayPrize = prizes[uniformPrizeDist(m_rng)];  // 均匀分布展示
        }
    }
    return card;
}

// ── 主循环 ────────────────────────────
void Game::run() {
    bool running = true;
    Uint32 lastTime = SDL_GetTicks();

    while (running && m_state != GameState::QUIT) {
        Uint32 now = SDL_GetTicks();
        (void)(now - lastTime);
        lastTime = now;

        processInput();
        update();
        render();

        SDL_Delay(16);
    }
}

// ── 输入处理 ──────────────────────────
void Game::processInput() {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_QUIT:
            m_state = GameState::QUIT;
            break;

        case SDL_KEYDOWN:
            // 全局快捷键
            if (m_state != GameState::FILE_DIALOG) {
                if (ev.key.keysym.sym == SDLK_F5) {
                    manualSave();
                    break;
                } else if (ev.key.keysym.sym == SDLK_F9) {
                    manualLoad();
                    break;
                }
            }

            if (m_state == GameState::FILE_DIALOG) {
                if (ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_KP_ENTER) {
                    // 确认路径
                    std::string path = m_fileDialogPath.empty()
                                       ? std::string(SAVE_FILE) : m_fileDialogPath;
                    bool ok = false;
                    if (m_fileDialogMode == 'S') {
                        ok = m_saveMgr.saveToFile(m_saveData, path.c_str());
                        m_saveTipMsg = ok ? "存档成功! (" + path + ")" : "存档失败!";
                    } else {
                        ok = m_saveMgr.loadFromFile(m_saveData, path.c_str());
                        m_saveTipMsg = ok ? "读档成功! (" + path + ")" : "读档失败!";
                    }
                    m_saveTipSuccess = ok;
                    m_saveTipTimer = 90;
                    m_state = GameState::SAVE_TIP;
                    SDL_StopTextInput();
                    m_fileDialogActive = false;
                } else if (ev.key.keysym.sym == SDLK_ESCAPE) {
                    m_state = GameState::MAIN_MENU;
                    SDL_StopTextInput();
                    m_fileDialogActive = false;
                } else if (ev.key.keysym.sym == SDLK_BACKSPACE) {
                    if (!m_fileDialogPath.empty())
                        m_fileDialogPath.pop_back();
                }
                break;
            }

            // 非对话框状态下的 Esc
            if (ev.key.keysym.sym == SDLK_ESCAPE) {
                if (m_state == GameState::RESULT_POPUP ||
                    m_state == GameState::CHEAT_POPUP) {
                    m_state = GameState::MAIN_MENU;
                } else if (m_state == GameState::SCRATCHING) {
                    m_state = GameState::MAIN_MENU;
                }
            }
            break;

        case SDL_TEXTINPUT:
            if (m_state == GameState::FILE_DIALOG && m_fileDialogActive) {
                m_fileDialogPath += ev.text.text;
            }
            break;

        case SDL_MOUSEMOTION:
            {
                int mx = ev.motion.x, my = ev.motion.y;
                if (m_state == GameState::MAIN_MENU) {
                    m_hoveredCard = -1;
                    m_hoverBuy   = false;
                    m_hoverSaveBtn = false;
                    m_hoverLoadBtn = false;

                    // 卡片区
                    int cardW = 200, cardH = 340;
                    int totalW = 3 * cardW + 2 * 40;
                    int startX = (WIN_W - totalW) / 2;
                    int cardY  = 90;
                    for (int i = 0; i < 3; ++i) {
                        int cx = startX + i * (cardW + 40);
                        SDL_Rect cr = {cx, cardY, cardW, cardH};
                        if (mx >= cr.x && mx < cr.x + cr.w &&
                            my >= cr.y && my < cr.y + cr.h)
                            m_hoveredCard = i;
                    }

                    // 底部按钮
                    SDL_Rect buyBtn  = {WIN_W - 150, WIN_H - 55, 130, 40};
                    SDL_Rect saveBtn = {WIN_W / 2 - 160, WIN_H - 55, 100, 40};
                    SDL_Rect loadBtn = {WIN_W / 2 -  50, WIN_H - 55, 100, 40};

                    if (mx >= buyBtn.x && mx < buyBtn.x + buyBtn.w &&
                        my >= buyBtn.y && my < buyBtn.y + buyBtn.h)
                        m_hoverBuy = true;
                    if (mx >= saveBtn.x && mx < saveBtn.x + saveBtn.w &&
                        my >= saveBtn.y && my < saveBtn.y + saveBtn.h)
                        m_hoverSaveBtn = true;
                    if (mx >= loadBtn.x && mx < loadBtn.x + loadBtn.w &&
                        my >= loadBtn.y && my < loadBtn.y + loadBtn.h)
                        m_hoverLoadBtn = true;

                } else if (m_state == GameState::SCRATCHING) {
                    m_hoverReveal = false;
                    SDL_Rect rv = {WIN_W - 170, WIN_H - 55, 150, 40};
                    if (mx >= rv.x && mx < rv.x + rv.w &&
                        my >= rv.y && my < rv.y + rv.h)
                        m_hoverReveal = true;
                    if (m_mouseDown)
                        handleScratchMouseMove(mx, my);
                }
            }
            break;

        case SDL_MOUSEBUTTONDOWN:
            if (ev.button.button == SDL_BUTTON_LEFT) {
                int mx = ev.button.x, my = ev.button.y;
                switch (m_state) {
                case GameState::MAIN_MENU:
                    // 作弊按钮
                    {
                        SDL_Rect cheatBtn = {10, 10, 90, 36};
                        if (mx >= cheatBtn.x && mx < cheatBtn.x + cheatBtn.w &&
                            my >= cheatBtn.y && my < cheatBtn.y + cheatBtn.h) {
                            triggerCheat();
                            break;
                        }
                    }
                    // 保存按钮
                    if (m_hoverSaveBtn) {
                        m_fileDialogMode = 'S';
                        m_fileDialogPath.clear();
                        m_fileDialogActive = true;
                        SDL_StartTextInput();
                        m_state = GameState::FILE_DIALOG;
                        break;
                    }
                    // 读取按钮
                    if (m_hoverLoadBtn) {
                        m_fileDialogMode = 'L';
                        m_fileDialogPath.clear();
                        m_fileDialogActive = true;
                        SDL_StartTextInput();
                        m_state = GameState::FILE_DIALOG;
                        break;
                    }
                    // 卡片选中
                    if (m_hoveredCard >= 0)
                        m_selectedCard = m_hoveredCard;
                    handleMainMenuClick(mx, my);
                    break;

                case GameState::SCRATCHING:
                    {
                        SDL_Rect rv = {WIN_W - 170, WIN_H - 55, 150, 40};
                        if (mx >= rv.x && mx < rv.x + rv.w &&
                            my >= rv.y && my < rv.y + rv.h) {
                            revealAll();
                            break;
                        }
                    }
                    handleScratchMouseDown(mx, my);
                    break;

                case GameState::RESULT_POPUP:
                    m_state = GameState::MAIN_MENU;
                    break;
                case GameState::CHEAT_POPUP:
                    m_state = GameState::MAIN_MENU;
                    break;
                case GameState::FILE_DIALOG:
                    // 点击对话框外 → 取消
                    break;
                default:
                    break;
                }
            }
            break;

        case SDL_MOUSEBUTTONUP:
            if (ev.button.button == SDL_BUTTON_LEFT) {
                if (m_state == GameState::SCRATCHING)
                    handleScratchMouseUp(ev.button.x, ev.button.y);
                m_mouseDown = false;
            }
            break;
        }
    }
}

// ── 更新 ──────────────────────────────
void Game::update() {
    if (m_state == GameState::SAVE_TIP) {
        m_saveTipTimer--;
        if (m_saveTipTimer <= 0)
            m_state = GameState::MAIN_MENU;
    }
}

// ── 渲染 ──────────────────────────────
void Game::render() {
    SDL_SetRenderDrawColor(m_renderer->getSDLRenderer(), 0, 0, 0, 255);
    SDL_RenderClear(m_renderer->getSDLRenderer());

    switch (m_state) {
    case GameState::MAIN_MENU:
        m_renderer->drawMainMenu(m_saveData, m_hoveredCard, m_selectedCard,
                                  m_hoverBuy, m_hoverSaveBtn, m_hoverLoadBtn,
                                  m_coinTex, m_cardTex);
        break;
    case GameState::SCRATCHING:
        m_renderer->drawScratchPage(m_currentCard, m_saveData,
                                     m_hoverReveal, m_coinTex);
        break;
    case GameState::RESULT_POPUP:
        m_renderer->drawScratchPage(m_currentCard, m_saveData,
                                     false, m_coinTex);
        m_renderer->drawResultPopup(m_currentCard, m_coinTex);
        break;
    case GameState::CHEAT_POPUP:
        m_renderer->drawMainMenu(m_saveData, -1, m_selectedCard,
                                  false, false, false,
                                  m_coinTex, m_cardTex);
        m_renderer->drawCheatPopup(m_popupMessage.c_str(), m_popupAmount);
        break;
    case GameState::SAVE_TIP:
        m_renderer->drawMainMenu(m_saveData, -1, m_selectedCard,
                                  false, false, false,
                                  m_coinTex, m_cardTex);
        m_renderer->drawSaveTip(m_saveTipMsg.c_str(), m_saveTipSuccess);
        break;
    case GameState::FILE_DIALOG:
        m_renderer->drawMainMenu(m_saveData, -1, m_selectedCard,
                                  false, false, false,
                                  m_coinTex, m_cardTex);
        m_renderer->drawFileDialog(m_fileDialogMode, m_fileDialogPath);
        break;
    default:
        break;
    }

    SDL_RenderPresent(m_renderer->getSDLRenderer());
}

// ── 主菜单点击处理 ────────────────────
void Game::handleMainMenuClick(int mx, int my) {
    if (!m_hoverBuy || m_selectedCard < 0) return;
    (void)mx; (void)my;

    auto types = getCardTypes();
    if (m_selectedCard >= (int)types.size()) return;

    const auto& cfg = types[m_selectedCard];

    if (m_saveData.balance < cfg.cost) {
        m_popupMessage = "余额不足!";
        m_popupAmount = 0;
        m_state = GameState::CHEAT_POPUP;
        return;
    }

    m_saveData.balance -= cfg.cost;
    m_saveData.totalSpent += cfg.cost;
    m_saveData.totalBought++;

    m_currentCard = generateCardInstance(m_selectedCard);
    m_saveData.souvenirs[m_selectedCard]++;

    if (m_currentCard.totalPrize > 0) {
        m_saveData.totalWon++;
        m_saveData.totalPrizes += m_currentCard.totalPrize;
    }
    m_saveData.profit = (int64_t)m_saveData.totalPrizes - (int64_t)m_saveData.totalSpent;

    m_renderer->createCoatingTexture(m_currentCard);
    m_state = GameState::SCRATCHING;
    m_mouseDown = false;
}

// ── 刮开交互 ──────────────────────────
void Game::handleScratchMouseDown(int mx, int my) {
    m_mouseDown = true;
    handleScratchMouseMove(mx, my);
}

void Game::handleScratchMouseMove(int mx, int my) {
    int startX = m_renderer->gridStartX();
    int startY = m_renderer->gridStartY();
    for (int i = 0; i < (int)m_currentCard.cells.size(); ++i) {
        SDL_Rect cr = {
            startX + (i % GRID_COLS) * (CELL_W + CELL_GAP),
            startY + (i / GRID_COLS) * (CELL_H + CELL_GAP),
            CELL_W, CELL_H
        };
        if (mx >= cr.x && mx < cr.x + cr.w && my >= cr.y && my < cr.y + cr.h) {
            m_renderer->scratchAt(i, mx - cr.x, my - cr.y, 240, m_currentCard);
            break;
        }
    }
    if (m_renderer->checkRevealPercent(m_currentCard)) {
        m_saveData.balance += m_currentCard.totalPrize;
        m_mouseDown = false;
        m_state = GameState::RESULT_POPUP;
    }
}

void Game::handleScratchMouseUp(int /*mx*/, int /*my*/) {
    m_mouseDown = false;
    if (m_renderer->checkRevealPercent(m_currentCard)) {
        m_saveData.balance += m_currentCard.totalPrize;
        m_state = GameState::RESULT_POPUP;
    }
}

// ── 一键刮开 ──────────────────────────
void Game::revealAll() {
    for (int i = 0; i < (int)m_currentCard.cells.size(); ++i) {
        auto& cell = m_currentCard.cells[i];
        for (int mr = 0; mr < MASK_ROWS; ++mr) {
            for (int mc = 0; mc < MASK_COLS; ++mc) {
                if (!cell.mask[mr][mc]) {
                    cell.mask[mr][mc] = true;
                    cell.revealedCount++;
                }
            }
        }
        m_renderer->scratchAt(i, CELL_W/2, CELL_H/2, CELL_W, m_currentCard);
    }
    m_saveData.balance += m_currentCard.totalPrize;
    m_state = GameState::RESULT_POPUP;
}

// ── 作弊系统 ──────────────────────────
void Game::triggerCheat() {
    m_saveData.cheatClicks++;
    int clicks = m_saveData.cheatClicks;

    if (clicks == 20 && !m_saveData.cheatDialog20) {
        m_popupMessage = "背着店长偷偷把伊波恩抵押了，这一世我定要……";
        m_popupAmount = 10000000;
        m_saveData.cheatMoney += m_popupAmount;
        m_saveData.balance   += m_popupAmount;
        m_saveData.cheatDialog20 = true;
    } else if (clicks == 10 && !m_saveData.cheatDialog10) {
        m_popupMessage = "管理局给娜娜莉的医疗补贴下来了";
        m_popupAmount = 1000000;
        m_saveData.cheatMoney += m_popupAmount;
        m_saveData.balance   += m_popupAmount;
        m_saveData.cheatDialog10 = true;
    } else {
        m_popupMessage = "开了一天滴滴，收入10万方斯";
        m_popupAmount = 100000;
        m_saveData.cheatMoney += m_popupAmount;
        m_saveData.balance   += m_popupAmount;
    }

    m_state = GameState::CHEAT_POPUP;
}

// ── 手动存/读档 ───────────────────────
void Game::manualSave() {
    if (m_saveMgr.autoSave(m_saveData)) {
        m_saveTipMsg = "存档成功!";
        m_saveTipSuccess = true;
    } else {
        m_saveTipMsg = "存档失败!";
        m_saveTipSuccess = false;
    }
    m_saveTipTimer = 90;
    m_state = GameState::SAVE_TIP;
}

void Game::manualLoad() {
    if (m_saveMgr.autoLoad(m_saveData)) {
        m_saveTipMsg = "读档成功!";
        m_saveTipSuccess = true;
    } else {
        m_saveTipMsg = "读档失败!";
        m_saveTipSuccess = false;
    }
    m_saveTipTimer = 90;
    m_state = GameState::SAVE_TIP;
}
