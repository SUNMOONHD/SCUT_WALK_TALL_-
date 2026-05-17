#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
#include <random>
#include <optional>

// ============================================================
// 敌人系统 - 所有敌人类型的头文件
// 三种敌人类型：
//   1. DashEnemy：蓄力冲撞型，追踪玩家后冲刺攻击
//   2. BlueScreenEnemy：叠层型，接触叠层后触发眩晕
//   3. ThiefEnemy：偷窃型，偷取玩家经验后逃跑
// ============================================================

// ============================================================
// 敌人基类
// 派生类必须实现：update()、render()
// ============================================================
class Enemy {
public:
    enum class Type { DashServer, BlueScreen, Thief };

    explicit Enemy(Type type, sf::Vector2f pos);
    virtual ~Enemy() = default;

    // 每帧调用：更新敌人 AI 行为
    virtual void update(float dt, sf::Vector2f playerPos) = 0;
    // 每帧调用：渲染敌人精灵和血条
    virtual void render(sf::RenderWindow& window) = 0;

    // 获取敌人碰撞框
    sf::FloatRect getBounds() const;
    // 是否死亡
    bool          isDead() const { return m_hp <= 0; }
    // 是否造成接触伤害
    bool          isTouchDamage() const { return m_touchDamage; }
    // 获取接触 DPS
    int           getTouchDps() const { return m_touchDps; }
    // 获取本帧待结算伤害（调用后自动清零）
    virtual int   getPendingDamage(float dt) { return 0; }
    // 是否已掉落经验
    bool          hasDroppedExp() const { return m_expDropped; }
    // 标记为已掉落经验
    void          markExpDropped() { m_expDropped = true; }
    // 获取死亡掉落经验值
    int           getExpValue() const { return m_expValue; }
    // 获取位置
    sf::Vector2f getPosition() const { return m_pos; }
    // 获取尺寸
    sf::Vector2f getSize() const { return m_size; }
    // 设置位置
    void          setPosition(sf::Vector2f p) { m_pos = p; }
    // 获取类型
    Type          getType() const { return m_type; }
    // 受到伤害
    void          takeDamage(int dmg) { m_hp -= dmg; }
    // 设置地图边界
    void          setMapBounds(float w, float h) { m_mapW = w; m_mapH = h; }
    // 施加段错误效果（眩晕 + 延迟伤害）
    void          applySegfault(float duration, int pendingDmg = 0) {
        m_segfaultTimer      = duration;
        m_segfaultPendingDmg = pendingDmg;
    }
    // 是否处于段错误状态
    bool          isSegfaulted() const { return m_segfaultTimer > 0.f; }
    // 是否正在冲刺（用于判断是否无视碰撞）
    virtual bool  isDashing() const { return false; }
    // 是否无视地图碰撞（穿墙）
    virtual bool  ignoreMapCollision() const { return false; }

    // 随时间增强难度：factor = 1.0 为初始值，越大越强
    virtual void  applyDifficulty(float factor) {}

    // 接触伤害判定：返回本帧偷取的经验值（正数），未偷取返回 0
    virtual int  checkTouchDamage(const sf::FloatRect& playerHitbox,
                                  float dt, int playerExp = 0) { return 0; }

    // 按距离偷取经验（ThiefEnemy 专用）
    virtual void tryStealExp(int /*playerExp*/, sf::Vector2f /*playerPos*/) {}

    // 是否刚偷取成功（Game.cpp 每帧检测并消费）
    virtual bool popStolenFlag() { return false; }
    // 获取本次偷取金额
    virtual int  getLastStolenAmount() const { return 0; }

protected:
    float        m_segfaultTimer      = 0.f;   // 段错误剩余时间
    int          m_segfaultPendingDmg = 0;    // 段错误结束时的结算伤害
    Type         m_type;                      // 敌人类型
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

    // 将位置限制在地图边界内
    void clampToMap() {
        if (m_mapW > 0.f) m_pos.x = std::clamp(m_pos.x, 0.f, m_mapW - m_size.x);
        if (m_mapH > 0.f) m_pos.y = std::clamp(m_pos.y, 0.f, m_mapH - m_size.y);
    }
};

// ============================================================
// 教务系统服务器机柜（Dash Enemy）
// AI 状态机：Idle（等待）-> Charge（蓄力）-> Dash（冲刺）-> Recover（恢复）
// 蓄力期间闪烁提示，冲刺时无视碰撞穿墙
// ============================================================
class DashEnemy : public Enemy {
public:
    explicit DashEnemy(sf::Vector2f pos,
                       sf::Texture* texLeft,
                       sf::Texture* texRight);

    void update(float dt, sf::Vector2f playerPos) override;
    void render(sf::RenderWindow& window) override;
    int  getPendingDamage(float dt) override;
    void applyDifficulty(float factor) override;
    int checkTouchDamage(const sf::FloatRect& playerHitbox,
                          float dt, int playerExp = 0) override;

    // 冲刺状态下无视地图碰撞
    bool ignoreMapCollision() const override { return m_state == AIState::Dash; }

private:
    // AI 状态枚举
    enum class AIState { Idle, Alert, Charge, Dash, Recover };

    void enterIdle();          // 进入等待状态
    void enterAlert();         // 进入警戒状态（发现玩家但未瞄准）
    void enterCharge(sf::Vector2f playerPos);  // 进入蓄力状态
    void enterDash();          // 进入冲刺状态
    void enterRecover();       // 进入恢复状态
    bool isDashing() const override { return m_state == AIState::Dash; }
    
    // 检测玩家是否在视野范围内（扇形区域）
    bool isPlayerInSight(sf::Vector2f playerPos) const;

    AIState      m_state       = AIState::Idle;   // 当前 AI 状态
    float        m_stateTimer  = 0.f;             // 状态持续时间
    float        m_detectRange = 480.f;           // 警戒范围（像素）
    float        m_visionAngle = 120.f;           // 视野角度（度）
    float        m_idleTime     = 0.4f;           // 等待持续时间
    float        m_alertTime    = 0.5f;           // 警戒状态持续时间
    float        m_chargeTime   = 1.2f;           // 蓄力持续时间
    float        m_dashSpeed    = 620.f;          // 冲刺速度（像素/秒）
    float        m_dashMaxDist  = 700.f;          // 最大冲刺距离
    float        m_recoverTime  = 1.0f;           // 恢复持续时间
    sf::Vector2f m_dashDir;                      // 冲刺方向
    float        m_dashTraveled = 0.f;            // 已冲刺距离
    bool         m_facingRight  = true;           // 朝向
    float        m_blinkTimer   = 0.f;            // 闪烁计时
    bool         m_blinkOn      = true;           // 闪烁状态
    sf::Vector2f m_lastPlayerPos;                 // 缓存玩家位置（用于追踪）

    int   m_dashDamage = 10;    // 冲刺伤害
    float m_slowAmount = 0.45f; // 接触减速比例
    bool  m_hitPlayer  = false; // 是否已命中玩家

    sf::Texture* m_texLeft;     // 左朝向纹理
    sf::Texture* m_texRight;    // 右朝向纹理
    std::optional<sf::Sprite> m_sprite;  // 精灵
    sf::RectangleShape m_hpBarBg;   // 血条背景
    sf::RectangleShape m_hpBarFill;  // 血条填充
    sf::RectangleShape m_chargeBar;  // 蓄力条
};

// ============================================================
// 蓝屏怪（Blue Screen Enemy）
// 接触玩家时叠层，每层造成伤害。叠层达到上限时触发眩晕效果
// 移动无视地图碰撞（穿墙）
// ============================================================
class BlueScreenEnemy : public Enemy {
public:
    explicit BlueScreenEnemy(sf::Vector2f pos,
                             sf::Texture* texLeft,
                             sf::Texture* texRight);

    void update(float dt, sf::Vector2f playerPos) override;
    void render(sf::RenderWindow& window) override;
    int  getPendingDamage(float dt) override;
    void applyDifficulty(float factor) override;
    int checkTouchDamage(const sf::FloatRect& playerHitbox,
                          float dt, int playerExp = 0) override;

    // 穿墙设计
    bool ignoreMapCollision() const override { return true; }

    // 叠层事件：Game.cpp 每帧 poll
    bool popStackEvent() { if (m_stackEvent) { m_stackEvent = false; return true; } return false; }
    int  getStackCount() const { return m_stackCount; }
    void resetStack() { m_stackCount = 0; m_touchCdTimer = 0.f; }

private:
    // AI 状态枚举
    enum class AIState { Idle, Chase };

    sf::Texture* m_texLeft  = nullptr;   // 左朝向纹理
    sf::Texture* m_texRight = nullptr;  // 右朝向纹理
    bool         m_facingRight = true;   // 朝向
    bool         m_useSpriteTex = false; // 是否使用精灵纹理

    float  m_moveSpeed = 80.f;  // 追踪速度（像素/秒）
    float  m_detectRange = 350.f; // 警戒范围
    float  m_visionAngle = 180.f; // 视野角度（度，180表示全向）
    
    AIState m_state = AIState::Idle;     // 当前状态
    float   m_idleTimer = 0.f;           // 待机计时
    float   m_idleDuration = 1.5f;       // 待机持续时间
    sf::Vector2f m_velocity;             // 当前速度（用于平滑移动）
    float   m_acceleration = 200.f;      // 加速度
    float   m_deceleration = 150.f;      // 减速度

    int    m_stackCount    = 0;    // 当前叠层数（0 ~ kMaxStack）
    float  m_touchCdTimer  = 0.f;  // 叠层冷却计时
    static constexpr float kTouchCd  = 1.0f;  // 叠层间隔 1.0 秒
    static constexpr int   kMaxStack = 10;       // 10 层触发眩晕
    bool   m_stackEvent    = false; // 叠层事件标志

    std::optional<sf::Sprite> m_sprite;  // 精灵
    sf::RectangleShape m_hpBarBg;   // 血条背景
    sf::RectangleShape m_hpBarFill;  // 血条填充
    sf::Texture  m_texture;     // Fallback 纹理
    bool         m_touching = false;  // 是否正在接触玩家

    void buildTexture();  // 构建蓝屏纹理
    
    // 检测玩家是否在视野范围内
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
    float      m_detectRange = 300.f;  // 警戒范围
    float      m_visionAngle = 90.f;   // 视野角度（度）
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
