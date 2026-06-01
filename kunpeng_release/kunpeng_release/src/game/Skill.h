#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <functional>
#include <optional>

// ============================================================
// 技能系统 - 所有技能类的头文件
// 
// 系统概述：
// 游戏中的技能分为两类：
//   1. CD型技能（m_cooldownMax > 0）：有冷却时间，需要等待充能
//   2. 被动型技能（m_cooldownMax = 0）：持续生效，自动触发
// 
// 职业专属技能：
// - 计算机学院学生（CSPlayer）：
//   - PointerStormSkill（指针风暴）：释放指针弹幕攻击敌人
//   - MagicCircleSkill（校徽法阵）：法阵持续伤害
//   - KoiShieldSkill（锦鲤护盾）：锦鲤环绕保护玩家
// 
// - 化学学院学生（ChemPlayer）：
//   - ExothermicStormSkill（试剂风暴）：释放试剂攻击敌人
//   - ChemCircleSkill（分子法阵）：法阵持续伤害
//   - ChemOrbitSkill（炼金环绕）：炼金球环绕保护玩家
// 
// 继承关系：
// Skill（基类，抽象）
//   ├── PointerStormSkill
//   ├── MagicCircleSkill
//   ├── KoiShieldSkill
//   ├── ChemCircleSkill
//   ├── ExothermicStormSkill
//   └── ChemOrbitSkill
// ============================================================

// ============================================================
// 技能升级选项结构体
// ============================================================
struct SkillUpgrade {
    std::string label;      // 升级选项显示文本（如"伤害 +10%"）
    std::string statName;   // 属性名称（与 applyUpgrade 方法对应）
    float       delta;      // 属性变化量
    int         iconId;     // 图标 ID（预留，用于 UI 显示）
};

// ============================================================
// 技能基类（抽象类）
// 定义所有技能的通用属性和接口
// 派生类必须实现：update()、render()、getUpgrades()、applyUpgrade()
// ============================================================
class Skill {
public:
    /**
     * @brief 构造函数
     * @param id 技能唯一标识
     * @param name 技能名称（显示用）
     * @param internalName 内部名称（用于资源路径）
     * @param desc 技能描述文本
     */
    Skill(int id, const std::string& name, const std::string& internalName,
          const std::string& desc)
        : m_id(id), m_name(name), m_internalName(internalName), m_desc(desc) {}
    
    /**
     * @brief 虚析构函数
     */
    virtual ~Skill() = default;

    // ============================================================
    // 纯虚函数（派生类必须实现）
    // ============================================================

    /**
     * @brief 更新技能状态
     * @param dt 时间增量（秒）
     * @param playerPos 玩家当前位置（世界坐标）
     * @param gameCtx 游戏上下文（用于访问敌人列表等）
     * @note 处理冷却计时、特效更新、伤害判定等
     */
    virtual void update(float dt, const sf::Vector2f& playerPos,
                        class Game* gameCtx) = 0;
    
    /**
     * @brief 渲染技能特效
     * @param window 渲染窗口
     * @param playerPos 玩家当前位置（世界坐标）
     */
    virtual void render(sf::RenderWindow& window,
                        const sf::Vector2f& playerPos) = 0;
    
    /**
     * @brief 渲染命中特效（在敌人之上）
     * @param window 渲染窗口
     * @note 默认空实现，需要时派生类重写
     */
    virtual void renderHitEffects(sf::RenderWindow& window) {}

    // ============================================================
    // 通用方法（已实现）
    // ============================================================

    /** @return 技能唯一标识 */
    int getId() const { return m_id; }
    
    /** @return 技能当前等级 */
    int getLevel() const { return m_level; }
    
    /** @return 冷却进度（0~1，0 表示冷却完成） */
    float getCooldownRatio() const {
        return (m_cooldownMax > 0.f) ? m_cooldownTimer / m_cooldownMax : 1.f;
    }
    
    /** @return 技能名称 */
    const std::string& getName() const { return m_name; }
    
    /** @return 技能描述 */
    const std::string& getDesc() const { return m_desc; }

    /**
     * @brief 获取升级选项图片路径
     * @return 图片路径（assets/skills/levelup/{internalName}.png）
     */
    std::string getLevelupImagePath() const {
        return "assets/skills/levelup/" + m_internalName + ".png";
    }

    /**
     * @brief 获取本轮可选择的升级选项
     * @return 升级选项列表（3个选项）
     */
    virtual std::vector<SkillUpgrade> getUpgrades() const = 0;
    
    /**
     * @brief 执行升级效果
     * @param statName 属性名称
     * @param delta 属性变化量
     */
    virtual void applyUpgrade(const std::string& statName, float delta) = 0;

protected:
    // ============================================================
    // 受保护成员变量（派生类可访问）
    // ============================================================
    
    int         m_id;              // 技能唯一标识
    std::string m_name;            // 技能名称（显示用）
    std::string m_internalName;    // 内部名称（用于资源路径）
    std::string m_desc;            // 技能描述文本
    int         m_level = 1;       // 当前等级（初始为1）
    float       m_cooldownTimer = 0.f;  // 当前冷却计时
    float       m_cooldownMax   = 3.0f; // 冷却时间上限（秒）
};

// ============================================================
// 指针风暴（CS 学生专属技能）
// 周期性释放指针弹幕，攻击范围内最近的多个敌人
// ============================================================
class PointerStormSkill : public Skill {
public:
    explicit PointerStormSkill();

    void update(float dt, const sf::Vector2f& playerPos,
                class Game* gameCtx) override;
    void render(sf::RenderWindow& window,
                const sf::Vector2f& playerPos) override;
    void renderHitEffects(sf::RenderWindow& window) override;

    std::vector<SkillUpgrade> getUpgrades() const override;
    void applyUpgrade(const std::string& statName, float delta) override;

private:
    float m_range;       // 技能范围（像素）
    int   m_damage;      // 每次命中伤害
    int   m_maxTargets;  // 最大命中敌人数

    // 命中特效结构体
    struct HitEffect {
        sf::Vector2f pos;   // 特效位置
        float        timer; // 剩余显示时间
    };
    std::vector<HitEffect> m_hitEffects;  // 当前活跃的命中特效
};

// ============================================================
// 校徽法阵（CS 学生专属技能）
// 双层法阵围绕玩家：内圈校徽固定，外圈法环旋转
// 范围内敌人每 tickInterval 秒受到一次伤害
// ============================================================
class MagicCircleSkill : public Skill {
public:
    explicit MagicCircleSkill();

    void update(float dt, const sf::Vector2f& playerPos,
                class Game* gameCtx) override;
    void render(sf::RenderWindow& window,
                const sf::Vector2f& playerPos) override;

    std::vector<SkillUpgrade> getUpgrades() const override;
    void applyUpgrade(const std::string& statName, float delta) override;

private:
    float m_radius;          // 法阵半径（影响伤害判定和渲染大小）
    int   m_damage;          // 每次 tick 的伤害值
    float m_tickInterval;    // 伤害 tick 间隔（秒）
    float m_tickTimer;       // 距下次伤害的计时器

    float m_ringAngle;       // 法环当前旋转角度
    float m_ringSpeed;       // 法环旋转速度（度/秒）

    // 静态纹理（程序生命周期内只加载一次）
    static bool       s_texLoaded;
    static sf::Texture s_emblemTex;   // 校徽纹理（固定层）
    static sf::Texture s_ringTex;     // 法环纹理（旋转层）
};

// ============================================================
// 锦鲤护盾（CS 学生专属技能）
// 锦鲤环绕玩家游动，碰撞敌人时造成伤害后消失
// 消失后 2 秒在原位置重生，继续环绕
// ============================================================
class KoiShieldSkill : public Skill {
public:
    explicit KoiShieldSkill();

    void update(float dt, const sf::Vector2f& playerPos,
                class Game* gameCtx) override;
    void render(sf::RenderWindow& window,
                const sf::Vector2f& playerPos) override;

    std::vector<SkillUpgrade> getUpgrades() const override;
    void applyUpgrade(const std::string& statName, float delta) override;

private:
    // 单条锦鲤的状态
    struct Koi {
        int   frame = 0;          // 当前帧（0/1/2）
        float frameTimer = 0.f;   // 帧动画计时
        int   frameDir = 1;       // 帧动画方向（+1 或 -1）
        float orbitAngle = 0.f;   // 公转角度（弧度）
        float respawnTimer = -1.f; // 重生冷却（-1 表示存活）
        bool  alive = true;       // 是否存活
        bool  initialized = false; // 是否已初始化
        std::optional<sf::Sprite> sprite;  // SFML3 要求使用 optional
    };

    void initKoiPositions(const sf::Vector2f& center);   // 初始化锦鲤位置
    void updateKoiAnimation(Koi& koi, float dt);          // 更新帧动画
    sf::Vector2f getKoiWorldPos(const Koi& koi, const sf::Vector2f& center) const;  // 计算世界坐标
    void recalcSizeAndRadius();  // 根据锦鲤数量重算尺寸和半径

    float m_radius;           // 环绕半径（像素）
    int   m_koiCount;        // 当前锦鲤数量（可升级增加）
    int   m_damage;          // 每条锦鲤的伤害
    float m_orbitSpeed;      // 公转速度（弧度/秒）
    float m_respawnCooldown;  // 重生冷却时间（秒）
    float m_koiRenderSize;   // 锦鲤渲染尺寸（像素）

    std::vector<Koi> m_koi;  // 所有锦鲤的状态

    // 静态纹理（程序生命周期内只加载一次）
    static bool        s_texLoaded;
    static sf::Texture s_koiFrames[3];  // 3 帧动画纹理
};

// ============================================================
// 分子法阵（化学学院学生专属技能）
// 与校徽法阵逻辑相同，仅素材和描述不同
// ============================================================
class ChemCircleSkill : public Skill {
public:
    explicit ChemCircleSkill();

    void update(float dt, const sf::Vector2f& playerPos,
                class Game* gameCtx) override;
    void render(sf::RenderWindow& window,
                const sf::Vector2f& playerPos) override;

    std::vector<SkillUpgrade> getUpgrades() const override;
    void applyUpgrade(const std::string& statName, float delta) override;

private:
    float m_radius;
    int   m_damage;
    float m_tickInterval;
    float m_tickTimer;

    float m_ringAngle;
    float m_ringSpeed;

    static bool       s_texLoaded;
    static sf::Texture s_emblemTex;
    static sf::Texture s_ringTex;
};

// ============================================================
// 试剂风暴（化学学院学生专属技能）
// 与指针风暴逻辑相同，仅素材和描述不同
// ============================================================
class ExothermicStormSkill : public Skill {
public:
    explicit ExothermicStormSkill();

    void update(float dt, const sf::Vector2f& playerPos,
                class Game* gameCtx) override;
    void render(sf::RenderWindow& window,
                const sf::Vector2f& playerPos) override;
    void renderHitEffects(sf::RenderWindow& window) override;

    std::vector<SkillUpgrade> getUpgrades() const override;
    void applyUpgrade(const std::string& statName, float delta) override;

private:
    float m_range;
    int   m_damage;
    int   m_maxTargets;

    struct HitEffect {
        sf::Vector2f pos;
        float        timer;
    };
    std::vector<HitEffect> m_hitEffects;
};

// ============================================================
// 炼金环绕（化学学院学生专属技能）
// 与锦鲤护盾逻辑相同，仅素材和描述不同
// ============================================================
class ChemOrbitSkill : public Skill {
public:
    explicit ChemOrbitSkill();

    void update(float dt, const sf::Vector2f& playerPos,
                class Game* gameCtx) override;
    void render(sf::RenderWindow& window,
                const sf::Vector2f& playerPos) override;

    std::vector<SkillUpgrade> getUpgrades() const override;
    void applyUpgrade(const std::string& statName, float delta) override;

private:
    struct Orb {
        int   frame = 0;
        float frameTimer = 0.f;
        int   frameDir = 1;
        float orbitAngle = 0.f;
        float respawnTimer = -1.f;
        bool  alive = true;
        bool  initialized = false;
        std::optional<sf::Sprite> sprite;
    };

    void initOrbPositions(const sf::Vector2f& center);
    void updateOrbAnimation(Orb& orb, float dt);
    sf::Vector2f getOrbWorldPos(const Orb& orb, const sf::Vector2f& center) const;
    void recalcSizeAndRadius();

    float m_radius;
    int   m_orbCount;
    int   m_damage;
    float m_orbitSpeed;
    float m_respawnCooldown;
    float m_orbRenderSize;

    std::vector<Orb> m_orbs;

    static bool        s_texLoaded;
    static sf::Texture s_orbFrames[3];
};
