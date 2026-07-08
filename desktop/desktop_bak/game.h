#ifndef GAME_H
#define GAME_H

#include "config.h"
#include "save.h"
#include "render.h"
#include <SDL.h>
#include <random>

enum class GameState {
    MAIN_MENU,
    SCRATCHING,
    RESULT_POPUP,
    CHEAT_POPUP,
    SAVE_TIP,
    FILE_DIALOG,
    QUIT
};

class Game {
public:
    Game();
    ~Game();

    bool init();
    void run();

private:
    SDL_Window*   m_window   = nullptr;
    Renderer*     m_renderer = nullptr;
    SaveManager   m_saveMgr;
    SaveData      m_saveData;

    // 随机数
    std::mt19937 m_rng;
    std::uniform_real_distribution<double> m_probDist;

    // 状态
    GameState     m_state = GameState::MAIN_MENU;
    CardInstance  m_currentCard;
    int           m_hoveredCard  = -1;
    int           m_selectedCard = -1;   // 持久选中 (离开卡片区域不丢失)
    bool          m_hoverBuy     = false;
    bool          m_hoverSaveBtn = false;
    bool          m_hoverLoadBtn = false;
    bool          m_hoverReveal  = false;
    bool          m_mouseDown    = false;

    // 弹窗消息
    std::string   m_popupMessage;
    int           m_popupAmount = 0;
    int           m_saveTipTimer = 0;
    std::string   m_saveTipMsg;
    bool          m_saveTipSuccess = false;

    // 文件对话框
    char          m_fileDialogMode = 'S'; // 'S'=save, 'L'=load
    std::string   m_fileDialogPath;
    bool          m_fileDialogActive = false;

    // 纹理缓存 (占位)
    SDL_Texture*  m_coinTex   = nullptr;
    SDL_Texture*  m_cardTex[3] = {nullptr, nullptr, nullptr};

    // ── 方法 ──
    void processInput();
    void update();
    void render();

    // 生成卡片
    CardInstance generateCardInstance(int typeIndex);

    // 状态处理
    void handleMainMenuClick(int mx, int my);
    void handleScratchMouseDown(int mx, int my);
    void handleScratchMouseMove(int mx, int my);
    void handleScratchMouseUp(int mx, int my);
    void revealAll();

    // 作弊
    void triggerCheat();

    // 手动存/读档
    void manualSave();
    void manualLoad();

    // 自动保存
    void autoSave() { m_saveMgr.autoSave(m_saveData); }
};

#endif
