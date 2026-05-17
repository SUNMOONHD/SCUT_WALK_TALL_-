#pragma once

#include "ItemPickup.h"
#include "Enemy.h"
#include "Skill.h"
#include "player/CSPlayer.h"
#include "player/ChemPlayer.h"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/View.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Audio/Music.hpp>
#include <SFML/System/Vector2.hpp>

#include <memory>
#include <optional>
#include <vector>
#include <cstdio>

#include <imgui.h>
#include <imgui-SFML.h>

class Map;

// ============================================================
// 游戏场景状态枚举
// ============================================================
enum class GameState {
    MainMenu,    // 主菜单界面
    CharSelect,  // 角色选择界面
    Gameplay     // 游戏进行中
};

// ============================================================
// BGM 类型枚举
// ============================================================
enum class BGMType {
    Menu,     // 主菜单背景音乐
    Gameplay  // 游戏背景音乐
};

// ============================================================
// 飘字特效结构体
// 用于显示伤害数字、经验获取等临时信息
// ============================================================
struct FloatText {
    std::string text;       // 显示文本
    sf::Vector2f pos;       // 世界坐标位置
    float        life     = 1.5f;   // 存活时间（秒）
    float        elapsed  = 0.f;    // 已经过的时间
    sf::Color    color    = sf::Color(220, 50, 50, 255);  // 默认红色
    bool isDone() const { return elapsed >= life; }
};

// ============================================================
// 游戏主控制器类
// 持有并协调所有子系统（窗口、场景、角色、敌人、技能、地图等）
// ============================================================
class Game {
public:
    Game();
    ~Game();
    void run();

    // 技能系统访问接口：返回敌人列表引用
    std::vector<std::unique_ptr<Enemy>>& getEnemies() { return m_enemies; }

private:
    // 事件处理
    void handleEvents();
    // 每帧更新
    void update(float dt);
    // 每帧渲染
    void render();

    // UI 初始化
    void setupGufengStyle();   // 古风 UI 配色方案
    void setupFont(float scale);  // 字体加载
    void updateUIScale();      // 根据窗口大小更新 UI 缩放

    // ── 主菜单 ──
    void renderMainUI();
    void renderSettingsPanel();
    void renderTopLeftAnimation();
    void loadVideoFrames();
    void updateVideo(float dt);
    void switchFullscreen();
    void loadComingFrames();
    void renderComingSoonPopup();
    void renderCharSelectPopup();    // 角色选择弹窗（主菜单内）

    // ── Gameplay ──
    void enterGameplay();
    void updateGameplay(float dt);
    void renderGameplay();
    void renderHUD();                       // 血条 + 经验条 HUD
    void renderDeathScreen();               // 死亡蒙版 + 统计信息

    // ── 道具系统 ──
    void updatePickups(float dt);
    void renderPickups();
    void spawnRandomPickup();
    void spawnPickup(PickupType type, sf::Vector2f pos, int expValue = 50);
    void pickupItem(const ItemPickup& item);

    // ── 敌人系统 ──
    void initEnemies();
    void updateEnemies(float dt);
    void renderEnemies();

    // ── 技能系统 ──
    void initSkills();
    void updateSkills(float dt);
    void renderSkills();
    void renderSkillLevelUpUI();         // 升级选择 UI（暂停游戏）
    bool showLevelUpUI() const { return m_showLevelUp; }

    // ── 死亡系统 ──
    void checkDeath();

    // ── 音频系统 ──
    void playBGM(BGMType type);
    void stopBGM();
    void updateBGMVolume();

private:
    static constexpr unsigned int DesignWidth  = 1280;
    static constexpr unsigned int DesignHeight = 720;
    static constexpr int TILE_RENDER_SIZE = 32;

    sf::RenderWindow  m_window;
    sf::Clock         m_clock;
    bool m_running;
    bool m_isFullscreen;
    ImFont* m_customFont;

    // ── 主菜单背景 ──
    sf::Texture             m_menuBgTexture;
    std::optional<sf::Sprite> m_menuBgSprite;
    sf::RectangleShape      m_fallbackBg;
    bool                    m_textureLoaded;

    // ── 设置面板 ──
    bool  m_showSettingsPanel;
    float m_masterVolume;
    float m_bgmVolume;
    float m_sfxVolume;
    int   m_masterPct;
    int   m_bgmPct;
    int   m_sfxPct;

    // ── 视频帧动画（左上角设置按钮） ──
    std::vector<sf::Texture>   m_videoFrames;
    std::optional<sf::Sprite>  m_videoSprite;
    bool   m_videoLoaded;
    float  m_videoTimer;
    int    m_currentFrame;
    bool   m_switchFullscreenPending;
    float  m_uiScale;

    // ── Coming Soon 弹窗 ──
    bool m_showComingSoon;
    std::vector<sf::Texture> m_comingFrames;
    bool  m_comingLoaded;
    float m_comingTimer;
    int   m_comingFrame;

    // ── 角色选择弹窗 ──
    bool m_showCharSelect;
    sf::Texture m_csAvatarTexture;   // CS学生头像
    bool        m_csAvatarLoaded;
    sf::Texture m_chemAvatarTexture; // 化学学生头像
    bool        m_chemAvatarLoaded;

    // ── 场景状态 ──
    GameState m_state;
    BGMType   m_currentBGM = BGMType::Menu;

    // ── 音频 ──
    std::unique_ptr<sf::Music> m_bgmMusic;   // 当前播放的 BGM
    std::string m_bgmMenuPath  = "assets/audio/BGM/bgm_menu.ogg";
    std::string m_bgmGamePath  = "assets/audio/BGM/bgm_gameplay.ogg";

    // ── 角色选择 ──
    // -1 = 未选  0 = CS学生  1 = 化学学生
    int m_selectedCharIdx;

    // ── Gameplay ──
    std::unique_ptr<Map>   m_gameMap;
    std::unique_ptr<Player> m_activePlayer;  // 当前上场角色（CS or Chem）

    // 渲染用：颜色方块（无精灵图时使用）
    sf::RectangleShape m_playerShape;
    std::optional<sf::Sprite> m_playerSprite;  // 角色精灵图
    bool              m_playerSpriteLoaded = false;
    sf::Texture       m_charDown;        // 正面朝下
    sf::Texture       m_charUp;          // 背面朝上
    sf::Texture       m_charLeft;        // 侧面朝左
    sf::Texture       m_charRight;       // 侧面朝右（可选）
    // 碰撞箱（基于非透明像素精确包围盒，相对于精灵左上角）
    sf::FloatRect      m_charCollisionBox;
    float              m_spriteOffsetX = 0.f; // 精灵相对 m_playerPos 的水平偏移（居中）
    float              m_spriteOffsetY = 0.f; // 精灵相对 m_playerPos 的垂直偏移（向上）
    float              m_charSpriteScale = 0.25f; // 角色精灵缩放
    sf::Vector2f       m_playerPos;
    float              m_playerSpeed;
    sf::View           m_camera;
    sf::Vector2f       m_facingDir;  // 面朝方向（精灵切换用）

    // ── HUD 动画 ──
    float m_hpBarSmooth = 1.0f;    // 血条平滑显示比例（0~1）
    float m_expBarSmooth = 0.0f;   // 经验条平滑显示比例（0~1）
    float m_gameplayElapsed = 0.0f;

    // ── 道具系统 ──
    sf::Texture              m_healItemTexture;
    sf::Texture              m_speedItemTexture;
    sf::Texture              m_expItemTexture;
    bool                     m_healItemLoaded = false;
    bool                     m_speedItemLoaded = false;
    bool                     m_expItemLoaded = false;
    std::vector<ItemPickup>  m_pickups;
    float                    m_pickupSpawnTimer = 0.0f;
    float                    m_nextSpawnInterval = 7.5f;
    float                    m_speedBuffTimer = 0.0f;
    float                    m_expBuffTimer = 0.0f;

    // ── 敌人系统 ──
    sf::Texture                         m_dashEnemyTexLeft;
    sf::Texture                         m_dashEnemyTexRight;
    bool                                m_dashEnemyTexLoaded = false;
    sf::Texture                         m_bsodTexLeft;
    sf::Texture                         m_bsodTexRight;
    bool                                m_bsodTexLoaded = false;
    sf::Texture                         m_thiefTexLeft;
    sf::Texture                         m_thiefTexRight;
    bool                                m_thiefTexLoaded = false;
    std::vector<std::unique_ptr<Enemy>> m_enemies;
    float                               m_enemySlowTimer = 0.f;  // 被减速剩余时间
    float                               m_gameTime       = 0.f;  // 游戏进行时间（秒）
    float                               m_enemySpawnTimer= 0.f;  // 敌人刷新计时
    float                               m_enemySpawnInterval = 20.f; // 初始刷新间隔
    std::vector<FloatText>              m_floatTexts;            // 飘字特效列表

    // ── 蓝屏叠层僵直 ──
    int   m_blueStackCount  = 0;     // 当前叠层数（跨所有蓝屏敌人累计）
    float m_blueStunTimer   = 0.f;   // 僵直剩余时间（>0时玩家无法移动）

    // ── 死亡系统 ──
    bool                                m_isDead        = false; // 玩家是否死亡
    int                                 m_killCount     = 0;    // 击杀敌人数量
    float                               m_difficultyFactor = 1.0f; // 敌人强度系数（随时间增长）

    // ── 技能系统 ──
    std::vector<std::unique_ptr<Skill>> m_skills;               // 角色技能列表
    bool                                m_showLevelUp   = false; // 是否显示升级选择 UI
    int                                 m_levelUpChoice  = -1;  // 玩家选择的升级选项
    int                                 m_prevPlayerLevel = 0;  // 上次检查时的等级
    bool                                m_levelUpRandomized = false; // 当前升级页是否已随机属性
    std::vector<size_t>                 m_cachedRandomPicks;   // 缓存的随机属性索引
};
