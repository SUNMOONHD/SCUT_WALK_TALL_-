#pragma once

#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>
#include <memory>
#include <string>

// ============================================================
// 角色职业枚举
// 定义游戏中两种可选角色类型
// ============================================================
enum class PlayerType {
    CS,    // 计算机学院学生：特点是移动速度快、攻击力高，适合快速输出
    Chem   // 化学学院学生：特点是血量厚、防御力高，适合抗伤害
};

// ============================================================
// 玩家基类（抽象类）
// 作为所有玩家角色的基类，定义通用接口和属性
// 派生类：CSPlayer（计算机学生）、ChemPlayer（化学学生）
// ============================================================
class Player {
public:
    /**
     * @brief 构造函数
     * @param type 玩家类型（CS 或 Chem）
     * @param name 玩家名称
     */
    Player(PlayerType type, const std::string& name);
    
    /**
     * @brief 虚析构函数，确保派生类析构函数正确调用
     */
    virtual ~Player() = default;

    // ============================================================
    // 纯虚函数（派生类必须实现）
    // ============================================================
    
    /**
     * @brief 初始化玩家属性
     * 派生类需在此设置初始 hp、attack、defense、speed 等属性
     */
    virtual void init() = 0;
    
    /**
     * @brief 更新玩家状态
     * @param dt 时间增量（秒）
     */
    virtual void update(float dt) = 0;
    
    /**
     * @brief 执行攻击动作
     * 派生类需实现具体攻击逻辑（如冷却、伤害计算等）
     */
    virtual void attack() = 0;

    // ============================================================
    // 通用方法（已实现）
    // ============================================================
    
    /**
     * @brief 玩家受到伤害
     * @param damage 伤害值（已扣除防御力）
     */
    void takeDamage(int damage);
    
    /**
     * @brief 玩家恢复生命值
     * @param amount 恢复量
     */
    void heal(int amount);
    
    /**
     * @brief 检查玩家是否存活
     * @return true 表示存活，false 表示死亡
     */
    bool isAlive() const;

    // ============================================================
    // Getter 方法
    // ============================================================
    
    /** @return 玩家类型 */
    PlayerType getType() const;
    
    /** @return 玩家名称 */
    const std::string& getName() const;
    
    /** @return 当前生命值 */
    int getHp() const;
    
    /** @return 最大生命值 */
    int getMaxHp() const;
    
    /** @return 当前等级 */
    int getLevel() const;
    
    /** @return 当前经验值 */
    int getExp() const;
    
    /** @return 升级所需经验值 */
    int getExpToNext() const;
    
    /** @return 攻击力 */
    int getAttack() const;
    
    /** @return 防御力 */
    int getDefense() const;
    
    /** @return 移动速度（像素/秒） */
    float getSpeed() const;
    
    /** @return 代表色（用于方块渲染和UI显示） */
    sf::Color getColor() const;
    
    /** @return 当前位置（世界坐标） */
    const sf::Vector2f& getPosition() const;

    // ============================================================
    // Setter 方法
    // ============================================================
    
    /**
     * @brief 设置玩家位置
     * @param pos 目标位置（世界坐标）
     */
    void setPosition(const sf::Vector2f& pos);
    
    /**
     * @brief 设置生命值（直接赋值，用于复活等场景）
     * @param hp 新生命值
     */
    void setHp(int hp);

    /**
     * @brief 添加经验值
     * @param amount 经验值增量（可为负数，表示扣除经验）
     * @note 经验值达到上限时自动升级，等级提升后重置经验并增加属性
     */
    void addExp(int amount);

protected:
    // ============================================================
    // 受保护成员变量（派生类可访问）
    // 子类应在 init() 方法中初始化这些属性
    // ============================================================
    
    int   m_hp;          // 当前生命值
    int   m_maxHp;       // 最大生命值
    int   m_attack;      // 攻击力（影响普攻和技能伤害）
    int   m_defense;     // 防御力（减少受到的伤害）
    float m_speed;       // 移动速度（像素/秒）
    float m_critRate;    // 暴击率（0~1，攻击时有概率造成额外伤害）
    sf::Color m_color;   // 代表色（用于方块渲染和UI主题色）

    // ============================================================
    // 经验等级系统
    // ============================================================
    
    int   m_level;       // 当前等级（初始为1）
    int   m_exp;         // 当前经验值
    int   m_expToNext;   // 升级到下一级所需经验值

private:
    // ============================================================
    // 私有成员变量（仅基类可访问）
    // ============================================================
    
    PlayerType  m_type;       // 玩家类型标识
    std::string m_name;       // 玩家名称（用于显示和日志）
    sf::Vector2f m_position;  // 当前位置（世界坐标）
};
