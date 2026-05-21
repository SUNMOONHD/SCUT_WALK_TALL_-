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
// 定义游戏的主要场景状态
// ============================================================
enum class GameState {
    MainMenu,    // 主菜单界面：显示标题、开始按钮、设置等
    CharSelect,  // 角色选择界面：选择计算机学生或化学学生
    Gameplay     // 游戏进行中：核心游戏逻辑
};

// ============================================================
// BGM 类型枚举
// 定义背景音乐类型
// ============================================================
enum class BGMType {
    Menu,     // 主菜单背景音乐
    Gameplay  // 游戏进行中背景音乐
};

// ============================================================
// 飘字特效结构体
// 用于显示伤害数字、经验获取等临时浮动信息
// ============================================================
struct FloatText {
    std::string text;      // 显示文本内容
    sf::Vector2f pos;      // 世界坐标位置
    float        life      = 1.5f;   // 存活时间（秒）
    float        elapsed   = 0.f;    // 已经过的时间
    sf::Color    color     = sf::Color(220, 50, 50, 255);  // 默认红色（伤害）
    /**
     * @brief 检查是否已过期
     * @return true 表示已过期，需要移除
     */
    bool isDone() const { return elapsed >= life; }
};

// ============================================================
// 游戏主控制器类
// 
// 核心职责：
//   - 持有并协调所有子系统（窗口、场景、角色、敌人、技能、地图等）
//   - 管理游戏状态流转（主菜单 → 角色选择 → 游戏 → 死亡）
//   - 处理输入事件和游戏循环
//   - 渲染所有游戏元素
// 
// 子系统组成：
//   - 窗口系统：SFML RenderWindow
//   - 地图系统：Map 类
//   - 玩家系统：Player（CSPlayer/ChemPlayer）
//   - 敌人系统：Enemy（DashEnemy/BlueScreenEnemy/ThiefEnemy）
//   - 技能系统：Skill（多种派生类）
//   - 道具系统：ItemPickup
//   - UI 系统：ImGui
//   - 音频系统：SFML Music
// ============================================================
class Game {
public:
    /**
     * @brief 构造函数
     * 初始化游戏窗口、UI、资源等
     */
    Game();
    
    /**
     * @brief 析构函数
     * 释放所有资源
     */
    ~Game();
    
    /**
     * @brief 游戏主循环
     * 包含事件处理、更新、渲染三个阶段
     */
    void run();

    /**
     * @brief 获取敌人列表引用（供技能系统使用）
     * @return 敌人列表引用
     */
    std::vector<std::unique_ptr<Enemy>>& getEnemies() { return m_enemies; }

private:
    // ============================================================
    // 核心循环方法
    // ============================================================
    
    /**
     * @brief 处理 SFML 事件（键盘、鼠标、窗口事件等）
     */
    void handleEvents();
    
    /**
     * @brief 每帧更新游戏状态
     * @param dt 时间增量（秒）
     */
    void update(float dt);
    
    /**
     * @brief 每帧渲染游戏画面
     */
    void render();

    // ============================================================
    // UI 初始化方法
    // ============================================================
    
    /**
     * @brief 设置古风 UI 配色方案
     * @note 自定义 ImGui 样式，使用中国风配色
     */
    void setupGufengStyle();
    
    /**
     * @brief 加载字体
     * @param scale UI 缩放比例
     */
    void setupFont(float scale);
    
    /**
     * @brief 根据窗口大小更新 UI 缩放
     * @note 确保 UI 在不同分辨率下正确显示
     */
    void updateUIScale();

    // ============================================================
    // 主菜单相关方法
    // ============================================================
    
    /**
     * @brief 渲染主菜单 UI
     */
    void renderMainUI();
    
    /**
     * @brief 渲染设置面板
     */
    void renderSettingsPanel();
    
    /**
     * @brief 渲染左上角动画
     */
    void renderTopLeftAnimation();
    
    /**
     * @brief 加载视频帧（用于动画效果）
     */
    void loadVideoFrames();
    
    /**
     * @brief 更新视频动画
     * @param dt 时间增量
     */
    void updateVideo(float dt);
    
    /**
     * @brief 切换全屏模式
     */
    void switchFullscreen();
    
    /**
     * @brief 加载即将推出功能的帧动画
     */
    void loadComingFrames();
    
    /**
     * @brief 渲染即将推出弹窗
     */
    void renderComingSoonPopup();
    
    /**
     * @brief 渲染角色选择弹窗（主菜单内）
     */
    void renderCharSelectPopup();

    // ============================================================
    // 游戏进行中相关方法
    // ============================================================
    
    /**
     * @brief 进入游戏场景
     * @note 初始化地图、玩家、敌人等
     */
    void enterGameplay();
    
    /**
     * @brief 更新游戏玩法逻辑
     * @param dt 时间增量
     */
    void updateGameplay(float dt);
    
    /**
     * @brief 渲染游戏场景
     */
    void renderGameplay();
    
    /**
     * @brief 渲染 HUD（血条、经验条等）
     */
    void renderHUD();
    
    /**
     * @brief 渲染死亡界面
     * @note 显示死亡蒙版和统计信息
     */
    void renderDeathScreen();

    // ============================================================
    // 道具系统相关方法
    // ============================================================
    
    /**
     * @brief 更新道具状态
     * @param dt 时间增量
     */
    void updatePickups(float dt);
    
    /**
     * @brief 渲染道具
     */
    void renderPickups();
    
    /**
     * @brief 随机生成道具
     */
    void spawnRandomPickup();
    
    /**
     * @brief 生成指定类型的道具
     * @param type 道具类型
     * @param pos 生成位置
     * @param expValue 经验值（经验道具专用）
     */
    void spawnPickup(PickupType type, sf::Vector2f pos, int expValue = 50);
    
    /**
     * @brief 玩家拾取道具
     * @param item 道具对象
     */
    void pickupItem(const ItemPickup& item);

    // ============================================================
    // 敌人系统相关方法
    // ============================================================
    
    /**
     * @brief 初始化敌人（游戏开始时）
     */
    void initEnemies();
    
    /**
     * @brief 更新敌人状态
     * @param dt 时间增量
     */
    void updateEnemies(float dt);
    
    /**
     * @brief 渲染敌人
     */
    void renderEnemies();

    // ============================================================
    // 技能系统相关方法
    // ============================================================
    
    /**
     * @brief 初始化技能（根据角色类型）
     */
    void initSkills();
    
    /**
     * @brief 更新技能状态
     * @param dt 时间增量
     */
    void updateSkills(float dt);
    
    /**
     * @brief 渲染技能特效
     */
    void renderSkills();
    
    /**
     * @brief 渲染技能升级选择 UI
     * @note 暂停游戏显示升级选项
     */
    void renderSkillLevelUpUI();
    
    /**
     * @brief 检查是否显示升级 UI
     * @return true 表示显示升级 UI
     */
    bool showLevelUpUI() const { return m_showLevelUp; }

    // ============================================================
    // 死亡系统相关方法
    // ============================================================
    
    /**
     * @brief 检查玩家是否死亡
     */
    void checkDeath();

    // ============================================================
    // 音频系统相关方法
    // ============================================================
    
    /**
     * @brief 播放背景音乐
     * @param type BGM 类型
     */
    void playBGM(BGMType type);
    
    /**
     * @brief 停止背景音乐
     */
    void stopBGM();
    
    /**
     * @brief 更新 BGM 音量
     */
    void updateBGMVolume();

private:
    // ============================================================
    // 常量定义
    // ============================================================
    
    static constexpr unsigned int DesignWidth  = 1920;   // 设计分辨率宽度
    static constexpr unsigned int DesignHeight = 1080;  // 设计分辨率高度
    static constexpr int          TILE_RENDER_SIZE = 32; // Tile 渲染尺寸（像素）

    // ============================================================
    // 核心系统成员
    // ============================================================
    
    sf::RenderWindow  m_window;     // SFML 渲染窗口
    sf::Clock         m_clock;      // 游戏时钟（计算 dt）
    bool              m_running;    // 游戏运行标志
    bool              m_isFullscreen; // 是否全屏模式
    ImFont*           m_customFont; // 自定义字体（古风字体）

    // ============================================================
    // 主菜单相关成员
    // ============================================================
    
    sf::Texture              m_menuBgTexture;   // 主菜单背景纹理
    std::optional<sf::Sprite> m_menuBgSprite;   // 主菜单背景精灵
    sf::RectangleShape       m_fallbackBg;      // 背景纹理加载失败时的备用背景
    bool                     m_textureLoaded;   // 背景纹理是否加载成功

    // ============================================================
    // 设置面板相关成员
    // ============================================================
    
    bool  m_showSettingsPanel;  // 是否显示设置面板
    float m_masterVolume;       // 主音量（0~1）
    float m_bgmVolume;          // BGM 音量（0~1）
    float m_sfxVolume;          // SFX 音量（0~1）
    int   m_masterPct;          // 主音量百分比（0~100）
    int   m_bgmPct;             // BGM 音量百分比（0~100）
    int   m_sfxPct;             // SFX 音量百分比（0~100）

    // ============================================================
    // 视频帧动画相关成员（左上角设置按钮）
    // ============================================================
    
    std::vector<sf::Texture>  m_videoFrames;          // 视频帧纹理列表
    std::optional<sf::Sprite> m_videoSprite;          // 视频帧精灵
    bool                      m_videoLoaded;          // 视频帧是否加载成功
    float                     m_videoTimer;           // 视频播放计时
    int                       m_currentFrame;         // 当前播放帧索引
    bool                      m_switchFullscreenPending; // 全屏切换待处理
    float                     m_uiScale;              // UI 缩放比例

    // ============================================================
    // Coming Soon 弹窗相关成员
    // ============================================================
    
    bool                      m_showComingSoon;  // 是否显示 Coming Soon 弹窗
    std::vector<sf::Texture>  m_comingFrames;    // Coming Soon 动画帧
    bool                      m_comingLoaded;    // 动画帧是否加载成功
    float                     m_comingTimer;     // 动画播放计时
    int                       m_comingFrame;     // 当前动画帧索引

    // ============================================================
    // 角色选择弹窗相关成员
    // ============================================================
    
    bool        m_showCharSelect;         // 是否显示角色选择弹窗
    sf::Texture m_csAvatarTexture;        // 计算机学生头像纹理
    bool        m_csAvatarLoaded;         // 计算机学生头像是否加载成功
    sf::Texture m_chemAvatarTexture;      // 化学学生头像纹理
    bool        m_chemAvatarLoaded;       // 化学学生头像是否加载成功

    // ============================================================
    // 场景状态相关成员
    // ============================================================
    
    GameState   m_state;           // 当前游戏状态
    BGMType     m_currentBGM = BGMType::Menu; // 当前播放的 BGM 类型

    // ============================================================
    // 音频系统相关成员
    // ============================================================
    
    std::unique_ptr<sf::Music> m_bgmMusic;  // 当前播放的 BGM 对象
    std::string                m_bgmMenuPath  = "assets/audio/BGM/bgm_menu.ogg";    // 菜单 BGM 路径
    std::string                m_bgmGamePath  = "assets/audio/BGM/bgm_gameplay.ogg"; // 游戏 BGM 路径

    // ============================================================
    // 角色选择相关成员
    // ============================================================
    
    int m_selectedCharIdx;  // 选中角色索引（-1=未选，0=CS学生，1=化学学生）

    // ============================================================
    // 游戏进行中相关成员
    // ============================================================
    
    std::unique_ptr<Map>      m_gameMap;       // 当前地图
    std::unique_ptr<Player>   m_activePlayer;  // 当前上场角色（CS 或 Chem）

    // ============================================================
    // 玩家渲染相关成员
    // ============================================================
    
    sf::RectangleShape        m_playerShape;       // 颜色方块（无精灵图时使用）
    std::optional<sf::Sprite> m_playerSprite;     // 角色精灵图
    bool                      m_playerSpriteLoaded = false; // 精灵图是否加载成功
    sf::Texture               m_charDown;          // 正面朝下纹理
    sf::Texture               m_charUp;            // 背面朝上纹理
    sf::Texture               m_charLeft;          // 侧面朝左纹理
    sf::Texture               m_charRight;         // 侧面朝右纹理（可选）
    sf::FloatRect             m_charCollisionBox;  // 碰撞箱（相对于精灵左上角）
    float                     m_spriteOffsetX = 0.f; // 精灵水平偏移（居中）
    float                     m_spriteOffsetY = 0.f; // 精灵垂直偏移（向上）
    float                     m_charSpriteScale = 0.4f; // 角色精灵缩放
    sf::Vector2f              m_playerPos;         // 玩家位置（世界坐标）
    float                     m_playerSpeed;       // 玩家移动速度（像素/秒）
    sf::View                  m_camera;           // 游戏摄像机
    sf::Vector2f              m_facingDir;        // 面朝方向（精灵切换用）

    // ============================================================
    // HUD 动画相关成员
    // ============================================================
    
    float m_hpBarSmooth    = 1.0f;   // 血条平滑显示比例（0~1）
    float m_expBarSmooth   = 0.0f;   // 经验条平滑显示比例（0~1）
    float m_gameplayElapsed = 0.0f;  // 游戏进行时间（用于难度计算）

    // ============================================================
    // 道具系统相关成员
    // ============================================================
    
    sf::Texture             m_healItemTexture;   // 回血道具纹理
    sf::Texture             m_speedItemTexture;  // 加速道具纹理
    sf::Texture             m_expItemTexture;    // 经验道具纹理
    bool                    m_healItemLoaded    = false; // 回血道具纹理是否加载
    bool                    m_speedItemLoaded   = false; // 加速道具纹理是否加载
    bool                    m_expItemLoaded     = false; // 经验道具纹理是否加载
    std::vector<ItemPickup> m_pickups;          // 道具列表
    float                   m_pickupSpawnTimer  = 0.0f;  // 道具生成计时
    float                   m_nextSpawnInterval = 7.5f;  // 下次道具生成间隔
    float                   m_speedBuffTimer    = 0.0f;  // 加速 buff 剩余时间
    float                   m_expBuffTimer      = 0.0f;  // 经验 buff 剩余时间

    // ============================================================
    // 敌人系统相关成员
    // ============================================================
    
    sf::Texture                         m_dashEnemyTexLeft;   // DashEnemy 左朝向纹理
    sf::Texture                         m_dashEnemyTexRight;  // DashEnemy 右朝向纹理
    bool                                m_dashEnemyTexLoaded = false;
    sf::Texture                         m_bsodTexLeft;       // BlueScreenEnemy 左朝向纹理
    sf::Texture                         m_bsodTexRight;      // BlueScreenEnemy 右朝向纹理
    bool                                m_bsodTexLoaded = false;
    sf::Texture                         m_thiefTexLeft;      // ThiefEnemy 左朝向纹理
    sf::Texture                         m_thiefTexRight;     // ThiefEnemy 右朝向纹理
    bool                                m_thiefTexLoaded = false;
    std::vector<std::unique_ptr<Enemy>> m_enemies;           // 敌人列表
    float                               m_enemySlowTimer   = 0.f;  // 被减速剩余时间
    float                               m_gameTime         = 0.f;  // 游戏进行时间（秒）
    float                               m_enemySpawnTimer  = 0.f;  // 敌人刷新计时
    float                               m_enemySpawnInterval = 20.f; // 初始刷新间隔
    std::vector<FloatText>              m_floatTexts;       // 飘字特效列表（伤害、经验等）

    // ============================================================
    // 蓝屏叠层僵直系统相关成员
    // ============================================================
    
    int   m_blueStackCount = 0;   // 当前叠层数（跨所有蓝屏敌人累计）
    float m_blueStunTimer  = 0.f; // 僵直剩余时间（>0时玩家无法移动）

    // ============================================================
    // 死亡系统相关成员
    // ============================================================
    
    bool        m_isDead          = false;  // 玩家是否死亡
    int         m_killCount       = 0;      // 击杀敌人数量
    float       m_difficultyFactor = 1.0f;  // 敌人强度系数（随时间增长）

    // ============================================================
    // 技能系统相关成员
    // ============================================================
    
    std::vector<std::unique_ptr<Skill>> m_skills;               // 角色技能列表
    bool                                m_showLevelUp   = false; // 是否显示升级选择 UI
    int                                 m_levelUpChoice = -1;   // 玩家选择的升级选项
    int                                 m_prevPlayerLevel = 0;  // 上次检查时的等级
    bool                                m_levelUpRandomized = false; // 当前升级页是否已随机属性
    std::vector<size_t>                 m_cachedRandomPicks;   // 缓存的随机属性索引
};
