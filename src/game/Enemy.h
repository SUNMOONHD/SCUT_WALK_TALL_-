#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include <random>
#include <optional>

// ============================================================
// 敌人系统 - 所有敌人类型的头文件
// 
// 系统概述：
// 游戏中包含三种敌人类型，各自具有独特的行为模式和攻击方式：
// 
// 1. DashEnemy（教务系统服务器机柜）：
//    - 蓄力冲撞型敌人
//    - AI 状态机：Idle → Alert → Charge → Dash → Recover
//    - 蓄力期间显示充能条，冲刺时无视地图碰撞
// 
// 2. BlueScreenEnemy（蓝屏怪）：
//    - 叠层型敌人
//    - 接触玩家时累积叠层，达到上限触发眩晕
//    - 移动无视地图碰撞（穿墙设计）
// 
// 3. ThiefEnemy（外卖小偷）：
//    - 偷窃型敌人
//    - 不造成生命伤害，但会偷取玩家经验后逃跑
//    - 被击杀后返还偷取的经验
// 
// 继承关系：
// Enemy（基类，抽象）
//   ├── DashEnemy（派生类）
//   ├── BlueScreenEnemy（派生类）
//   └── ThiefEnemy（派生类）
// ============================================================

// ============================================================
// 敌人基类（抽象类）
// 定义所有敌人的通用属性和接口
// 派生类必须实现：update()、render()
// ============================================================
class Enemy {
public:
    /**
     * @brief 敌人类型枚举
     */
    enum class Type {
        DashServer,  // 蓄力冲撞型
        BlueScreen,  // 叠层眩晕型
        Thief        // 经验偷窃型
    };

    /**
     * @brief 构造函数
     * @param type 敌人类型
     * @param pos 初始位置（世界坐标）
     */
    explicit Enemy(Type type, sf::Vector2f pos);
    
    /**
     * @brief 虚析构函数，确保派生类析构函数正确调用
     */
    virtual ~Enemy() = default;

    // ============================================================
    // 纯虚函数（派生类必须实现）
    // ============================================================

    /**
     * @brief 更新敌人 AI 行为
     * @param dt 时间增量（秒）
     * @param playerPos 玩家当前位置（世界坐标）
     */
    virtual void update(float dt, sf::Vector2f playerPos) = 0;
    
    /**
     * @brief 渲染敌人精灵和血条
     * @param window 渲染窗口
     */
    virtual void render(sf::RenderWindow& window) = 0;

    // ============================================================
    // 通用方法（已实现）
    // ============================================================

    /**
     * @brief 获取敌人碰撞框
     * @return 碰撞框矩形（世界坐标）
     */
    sf::FloatRect getBounds() const;
    
    /**
     * @brief 检查敌人是否死亡
     * @return true 表示死亡，false 表示存活
     */
    bool isDead() const { return m_hp <= 0; }
    
    /**
     * @brief 检查敌人是否造成接触伤害
     * @return true 表示有接触伤害
     */
    bool isTouchDamage() const { return m_touchDamage; }
    
    /**
     * @brief 获取接触伤害 DPS（每秒伤害）
     * @return DPS 值
     */
    int getTouchDps() const { return m_touchDps; }
    
    /**
     * @brief 获取本帧待结算伤害（调用后自动清零）
     * @param dt 时间增量（用于计算累积伤害）
     * @return 待结算伤害值
     */
    virtual int getPendingDamage(float dt) { return 0; }
    
    /**
     * @brief 检查是否已掉落经验
     * @return true 表示已掉落经验
     */
    bool hasDroppedExp() const { return m_expDropped; }
    
    /**
     * @brief 标记为已掉落经验（防止重复掉落）
     */
    void markExpDropped() { m_expDropped = true; }
    
    /**
     * @brief 获取死亡掉落经验值
     * @return 经验值
     */
    int getExpValue() const { return m_expValue; }
    
    /**
     * @brief 获取当前位置
     * @return 世界坐标位置
     */
    sf::Vector2f getPosition() const { return m_pos; }
    
    /**
     * @brief 获取碰撞框尺寸
     * @return 尺寸向量
     */
    sf::Vector2f getSize() const { return m_size; }
    
    /**
     * @brief 设置位置
     * @param p 目标位置（世界坐标）
     */
    void setPosition(sf::Vector2f p) { m_pos = p; }
    
    /**
     * @brief 获取敌人类型
     * @return 敌人类型枚举值
     */
    Type getType() const { return m_type; }
    
    /**
     * @brief 敌人受到伤害
     * @param dmg 伤害值
     */
    void takeDamage(int dmg) { m_hp -= dmg; }
    
    /**
     * @brief 设置地图边界（用于位置限制）
     * @param w 地图宽度
     * @param h 地图高度
     */
    void setMapBounds(float w, float h) { m_mapW = w; m_mapH = h; }
    
    /**
     * @brief 施加段错误效果（眩晕 + 延迟伤害）
     * @param duration 眩晕持续时间（秒）
     * @param pendingDmg 眩晕结束时的结算伤害
     */
    void applySegfault(float duration, int pendingDmg = 0) {
        m_segfaultTimer      = duration;
        m_segfaultPendingDmg = pendingDmg;
    }
    
    /**
     * @brief 检查是否处于段错误状态
     * @return true 表示正在眩晕
     */
    bool isSegfaulted() const { return m_segfaultTimer > 0.f; }
    
    /**
     * @brief 检查是否正在冲刺
     * @return true 表示冲刺中
     */
    virtual bool isDashing() const { return false; }
    
    /**
     * @brief 检查是否无视地图碰撞（穿墙）
     * @return true 表示可以穿墙
     */
    virtual bool ignoreMapCollision() const { return false; }

    /**
     * @brief 应用难度系数（随时间增强敌人属性）
     * @param factor 难度系数（1.0 为初始值，越大越强）
     */
    virtual void applyDifficulty(float factor) {}

    /**
     * @brief 接触伤害判定
     * @param playerHitbox 玩家碰撞框
     * @param dt 时间增量（用于计算伤害累积）
     * @param playerExp 玩家当前经验值（供偷窃型敌人使用）
     * @return 本帧偷取的经验值（正数），未偷取返回 0
     */
    virtual int checkTouchDamage(const sf::FloatRect& playerHitbox,
                                  float dt, int playerExp = 0) { return 0; }

    /**
     * @brief 按距离偷取经验（ThiefEnemy 专用）
     * @param playerExp 玩家当前经验值
     * @param playerPos 玩家当前位置
     */
    virtual void tryStealExp(int /*playerExp*/, sf::Vector2f /*playerPos*/) {}

    /**
     * @brief 检查是否刚偷取成功（Game.cpp 每帧检测并消费）
     * @return true 表示本帧偷取成功
     */
    virtual bool popStolenFlag() { return false; }
    
    /**
     * @brief 获取本次偷取金额
     * @return 偷取的经验值
     */
    virtual int getLastStolenAmount() const { return 0; }

protected:
    // ============================================================
    // 受保护成员变量（派生类可访问）
    // ============================================================
    
    float        m_segfaultTimer      = 0.f;   // 段错误剩余时间（眩晕状态）
    int          m_segfaultPendingDmg = 0;    // 段错误结束时的结算伤害
    Type         m_type;                      // 敌人类型标识
    sf::Vector2f m_pos;                       // 世界坐标位置
    sf::Vector2f m_size;                      // 碰撞框尺寸
    int          m_hp           = 40;          // 当前生命值
    int          m_maxHp        = 40;          // 最大生命值
    bool         m_touchDamage  = false;       // 是否造成接触伤害
    int          m_touchDps     = 0;           // 接触伤害 DPS
    bool         m_expDropped   = false;       // 是否已掉落经验
    int          m_expValue     = 50;          // 死亡掉落经验值
    float        m_pendingDmg   = 0.f;        // 待结算伤害累积
    float        m_mapW = 0.f;                // 地图宽度
    float        m_mapH = 0.f;                // 地图高度

    /**
     * @brief 将位置限制在地图边界内
     */
    void clampToMap() {
        if (m_mapW > 0.f) m_pos.x = std::clamp(m_pos.x, 0.f, m_mapW - m_size.x);
        if (m_mapH > 0.f) m_pos.y = std::clamp(m_pos.y, 0.f, m_mapH - m_size.y);
    }
};

// ============================================================
// 教务系统服务器机柜（DashEnemy）
// 继承自 Enemy 基类
// 
// AI 状态机设计：
//   Idle（等待）→ Alert（警戒）→ Charge（蓄力）→ Dash（冲刺）→ Recover（恢复）
// 
// 核心行为：
//   - 待机时随机转向，检测玩家进入视野
//   - 发现玩家后进入警戒状态，短暂延迟后开始蓄力
//   - 蓄力期间显示充能条，精灵闪烁提示
//   - 蓄力完成后向玩家位置冲刺，冲刺时无视地图碰撞
//   - 冲刺结束后进入恢复状态，准备下一轮攻击
// 
// 属性特点：
//   - 中等生命值
//   - 冲刺伤害较高
//   - 接触时造成减速效果
// ============================================================
class DashEnemy : public Enemy {
public:
    /**
     * @brief 构造函数
     * @param pos 初始位置（世界坐标）
     * @param texLeft 左朝向纹理
     * @param texRight 右朝向纹理
     */
    explicit DashEnemy(sf::Vector2f pos,
                       sf::Texture* texLeft,
                       sf::Texture* texRight);

    /**
     * @brief 更新敌人 AI 行为
     * @param dt 时间增量（秒）
     * @param playerPos 玩家当前位置
     * @note 实现状态机逻辑：检测玩家、状态转换、移动控制
     */
    void update(float dt, sf::Vector2f playerPos) override;
    
    /**
     * @brief 渲染敌人精灵、血条和蓄力条
     * @param window 渲染窗口
     */
    void render(sf::RenderWindow& window) override;
    
    /**
     * @brief 获取本帧待结算伤害
     * @param dt 时间增量
     * @return 累积的接触伤害
     */
    int getPendingDamage(float dt) override;
    
    /**
     * @brief 应用难度系数
     * @param factor 难度系数
     * @note 难度提升时增加生命值、冲刺伤害和速度
     */
    void applyDifficulty(float factor) override;
    
    /**
     * @brief 接触伤害判定
     * @param playerHitbox 玩家碰撞框
     * @param dt 时间增量
     * @param playerExp 玩家经验值（本类不使用）
     * @return 偷取的经验值（本类返回0）
     */
    int checkTouchDamage(const sf::FloatRect& playerHitbox,
                          float dt, int playerExp = 0) override;

    /**
     * @brief 检查是否无视地图碰撞
     * @return true 表示冲刺中，可以穿墙
     */
    bool ignoreMapCollision() const override { return m_state == AIState::Dash; }

private:
    /**
     * @brief AI 状态枚举
     */
    enum class AIState {
        Idle,      // 等待状态：随机转向，检测玩家
        Alert,     // 警戒状态：发现玩家，准备蓄力
        Charge,    // 蓄力状态：充能阶段，显示蓄力条
        Dash,      // 冲刺状态：高速移动，无视碰撞
        Recover    // 恢复状态：冲刺结束，重置状态
    };

    /**
     * @brief 进入等待状态
     */
    void enterIdle();
    
    /**
     * @brief 进入警戒状态（发现玩家但未瞄准）
     */
    void enterAlert();
    
    /**
     * @brief 进入蓄力状态
     * @param playerPos 玩家位置（用于计算冲刺方向）
     */
    void enterCharge(sf::Vector2f playerPos);
    
    /**
     * @brief 进入冲刺状态
     */
    void enterDash();
    
    /**
     * @brief 进入恢复状态
     */
    void enterRecover();
    
    /**
     * @brief 检查是否正在冲刺
     * @return true 表示冲刺中
     */
    bool isDashing() const override { return m_state == AIState::Dash; }
    
    /**
     * @brief 检测玩家是否在视野范围内（扇形区域）
     * @param playerPos 玩家位置
     * @return true 表示玩家在视野内
     * @note 使用极坐标计算玩家是否在扇形视野内
     */
    bool isPlayerInSight(sf::Vector2f playerPos) const;

    // ============================================================
    // 状态机相关成员变量
    // ============================================================
    
    AIState      m_state       = AIState::Idle;   // 当前 AI 状态
    float        m_stateTimer  = 0.f;             // 当前状态持续时间
    float        m_detectRange = 480.f;           // 警戒范围（像素）
    float        m_visionAngle = 120.f;           // 视野角度（度）
    float        m_idleTime    = 0.4f;            // 等待状态持续时间
    float        m_alertTime   = 0.5f;            // 警戒状态持续时间
    float        m_chargeTime  = 1.2f;            // 蓄力状态持续时间
    float        m_dashSpeed   = 620.f;           // 冲刺速度（像素/秒）
    float        m_dashMaxDist = 700.f;           // 最大冲刺距离
    float        m_recoverTime = 1.0f;            // 恢复状态持续时间
    sf::Vector2f m_dashDir;                       // 冲刺方向向量
    float        m_dashTraveled = 0.f;            // 已冲刺距离
    bool         m_facingRight  = true;           // 朝向标记（true=向右）
    float        m_blinkTimer   = 0.f;            // 蓄力闪烁计时
    bool         m_blinkOn      = true;           // 闪烁状态
    sf::Vector2f m_lastPlayerPos;                 // 缓存玩家位置（用于追踪）

    // ============================================================
    // 伤害相关成员变量
    // ============================================================
    
    int   m_dashDamage = 10;    // 冲刺伤害
    float m_slowAmount = 0.45f; // 接触减速比例（0~1）
    bool  m_hitPlayer  = false; // 是否已命中玩家（防止重复伤害）

    // ============================================================
    // 渲染相关成员变量
    // ============================================================
    
    sf::Texture*             m_texLeft;     // 左朝向纹理
    sf::Texture*             m_texRight;    // 右朝向纹理
    std::optional<sf::Sprite> m_sprite;     // 精灵对象
    sf::RectangleShape       m_hpBarBg;     // 血条背景
    sf::RectangleShape       m_hpBarFill;   // 血条填充
    sf::RectangleShape       m_chargeBar;   // 蓄力条（蓄力状态显示）
};

// ============================================================
// 蓝屏怪（BlueScreenEnemy）
// 继承自 Enemy 基类
// 
// 核心机制：
//   - 接触玩家时累积叠层（Stack），每层造成少量伤害
//   - 叠层达到上限（10层）时触发眩晕效果，造成大量伤害
//   - 移动无视地图碰撞（穿墙设计），始终追踪玩家
// 
// AI 状态机：
//   Idle（待机）→ Chase（追踪）
// 
// 属性特点：
//   - 移动速度较慢
//   - 警戒范围较广（220度视野）
//   - 接触伤害通过叠层机制实现
//   - 眩晕效果对玩家造成硬控
// ============================================================
class BlueScreenEnemy : public Enemy {
public:
    /**
     * @brief 构造函数
     * @param pos 初始位置（世界坐标）
     * @param texLeft 左朝向纹理
     * @param texRight 右朝向纹理
     */
    explicit BlueScreenEnemy(sf::Vector2f pos,
                             sf::Texture* texLeft,
                             sf::Texture* texRight);

    /**
     * @brief 更新敌人 AI 行为
     * @param dt 时间增量（秒）
     * @param playerPos 玩家当前位置
     * @note 实现追踪逻辑和叠层状态更新
     */
    void update(float dt, sf::Vector2f playerPos) override;
    
    /**
     * @brief 渲染敌人精灵和血条
     * @param window 渲染窗口
     */
    void render(sf::RenderWindow& window) override;
    
    /**
     * @brief 获取本帧待结算伤害
     * @param dt 时间增量
     * @return 累积的叠层伤害
     */
    int getPendingDamage(float dt) override;
    
    /**
     * @brief 应用难度系数
     * @param factor 难度系数
     * @note 难度提升时增加生命值和叠层伤害
     */
    void applyDifficulty(float factor) override;
    
    /**
     * @brief 接触伤害判定
     * @param playerHitbox 玩家碰撞框
     * @param dt 时间增量
     * @param playerExp 玩家经验值（本类不使用）
     * @return 偷取的经验值（本类返回0）
     * @note 检测碰撞并更新叠层数
     */
    int checkTouchDamage(const sf::FloatRect& playerHitbox,
                          float dt, int playerExp = 0) override;

    /**
     * @brief 检查是否无视地图碰撞
     * @return true（蓝屏怪始终可以穿墙）
     */
    bool ignoreMapCollision() const override { return true; }

    /**
     * @brief 弹出叠层事件（Game.cpp 每帧检测）
     * @return true 表示本帧有新叠层
     */
    bool popStackEvent() { if (m_stackEvent) { m_stackEvent = false; return true; } return false; }
    
    /**
     * @brief 获取当前叠层数
     * @return 叠层数（0 ~ kMaxStack）
     */
    int getStackCount() const { return m_stackCount; }
    
    /**
     * @brief 重置叠层数和冷却
     */
    void resetStack() { m_stackCount = 0; m_touchCdTimer = 0.f; }

private:
    /**
     * @brief AI 状态枚举
     */
    enum class AIState {
        Idle,   // 待机状态：短暂停留后开始追踪
        Chase   // 追踪状态：持续向玩家移动
    };

    // ============================================================
    // 纹理相关成员变量
    // ============================================================
    
    sf::Texture* m_texLeft   = nullptr;  // 左朝向纹理
    sf::Texture* m_texRight  = nullptr;  // 右朝向纹理
    bool         m_facingRight = true;   // 朝向标记
    bool         m_useSpriteTex = false; // 是否使用精灵纹理

    // ============================================================
    // 移动相关成员变量
    // ============================================================
    
    float        m_moveSpeed    = 80.f;   // 追踪速度（像素/秒）
    float        m_detectRange  = 550.f;  // 警戒范围
    float        m_visionAngle  = 220.f;  // 视野角度（度，220表示更宽）
    AIState      m_state        = AIState::Idle;  // 当前状态
    float        m_idleTimer    = 0.f;    // 待机计时
    float        m_idleDuration = 1.5f;   // 待机持续时间
    sf::Vector2f m_velocity;              // 当前速度（平滑移动用）
    float        m_acceleration = 200.f;  // 加速度
    float        m_deceleration = 150.f;  // 减速度

    // ============================================================
    // 叠层系统相关成员变量
    // ============================================================
    
    int          m_stackCount   = 0;      // 当前叠层数（0 ~ kMaxStack）
    float        m_touchCdTimer = 0.f;    // 叠层冷却计时
    static constexpr float kTouchCd  = 1.0f;  // 叠层间隔（秒）
    static constexpr int   kMaxStack = 10;    // 叠层上限（触发眩晕）
    bool         m_stackEvent   = false;  // 叠层事件标志

    // ============================================================
    // 渲染相关成员变量
    // ============================================================
    
    std::optional<sf::Sprite> m_sprite;   // 精灵对象
    sf::RectangleShape       m_hpBarBg;   // 血条背景
    sf::RectangleShape       m_hpBarFill; // 血条填充
    sf::Texture              m_texture;   // Fallback 纹理（程序生成）
    bool                     m_touching   = false; // 是否正在接触玩家

    /**
     * @brief 构建蓝屏纹理（程序生成）
     */
    void buildTexture();
    
    /**
     * @brief 检测玩家是否在视野范围内
     * @param playerPos 玩家位置
     * @return true 表示玩家在视野内
     */
    bool isPlayerInSight(sf::Vector2f playerPos) const;
};

// ============================================================
// 外卖小偷（Thief Enemy）
// 不造成生命伤害，但会偷取玩家经验后逃跑
// 被击杀后，被偷取的经验会返还给玩家
// ============================================================
class ThiefEnemy : public Enemy {
public:
    explicit ThiefEnemy(sf::Vector2f pos,
                        sf::Texture* texLeft,
                        sf::Texture* texRight);

    void update(float dt, sf::Vector2f playerPos) override;
    void render(sf::RenderWindow& window) override;
    int  getPendingDamage(float dt) override { return 0; }  // 不造成生命伤害
    void applyDifficulty(float factor) override;
    int checkTouchDamage(const sf::FloatRect& playerHitbox,
                          float dt, int playerExp = 0) override;

    // 是否刚偷取成功（Game.cpp 每帧检测并消费）
    bool popStolenFlag() override;
    // 获取本次偷取金额
    int  getLastStolenAmount() const override { return m_lastStolenAmount; }

    // 按距离尝试偷经验（无需触碰）
    void tryStealExp(int playerExp, sf::Vector2f playerPos) override;
    // 击杀后应返还的经验量
    int  getStolenExp() const { return m_stolenExpTotal; }
    // 设置追踪基础速度（由 Game.cpp 按玩家速度计算后传入）
    void setBaseSpeed(float speed) { m_baseSpeed = speed; m_moveSpeed = speed; }
    void setFleeSpeed(float speed) { m_fleeSpeed = speed; }  // 逃跑速度
    // 累加偷走的经验
    void addStolenExp(int amount) { m_stolenExpTotal += amount; }

private:
    sf::Texture* m_texLeft  = nullptr;   // 左朝向纹理
    sf::Texture* m_texRight = nullptr;  // 右朝向纹理
    bool         m_facingRight = true;   // 朝向
    bool         m_useSpriteTex = false;
    float        m_moveSpeed   = 0.f;   // 当前移动速度

    // 状态机
    enum class ThiefState { Idle, Alert, Chase, Flee };
    ThiefState m_state       = ThiefState::Idle;  // 当前状态
    float      m_stateTimer  = 0.f;    // 状态计时器
    float      m_fleeTimer   = 0.f;    // 瞬移后恢复追踪的缓冲时间
    float      m_baseSpeed   = 0.f;    // 追踪速度（玩家速度 60%）
    float      m_fleeSpeed   = 0.f;    // 逃跑速度（玩家速度 90%）
    float      m_detectRange = 480.f;  // 警戒范围（增大）
    float      m_visionAngle = 120.f;   // 视野角度（度）
    sf::Vector2f m_lastPlayerPos;      // 上一帧玩家位置（逃跑方向计算用）
    sf::Vector2f m_fleeTarget;         // 逃跑目标点
    bool         m_hasFleeTarget      = false; // 是否有逃跑目标

    // 偷取状态
    bool  m_hasStolen       = false;   // 已偷取过（防止重复偷取）
    bool  m_stolenThisFrame = false;   // 本帧刚偷成功（待 Game.cpp 消费）
    int   m_stolenExpTotal  = 0;      // 累计偷走的经验量
    int   m_lastStolenAmount = 0;     // 本次偷取金额

    std::optional<sf::Sprite> m_sprite;  // 精灵
    sf::RectangleShape m_hpBarBg;   // 血条背景
    sf::RectangleShape m_hpBarFill;  // 血条填充
    sf::Texture  m_texture;     // Fallback 纹理

    void buildTexture();  // 构建小偷纹理
    
    // 检测玩家是否在视野范围内
    bool isPlayerInSight(sf::Vector2f playerPos) const;
    // 计算最优逃跑方向
    sf::Vector2f calculateFleeDirection(sf::Vector2f playerPos) const;
};
