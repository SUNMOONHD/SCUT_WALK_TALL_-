#pragma once

#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Vector2.hpp>
#include <memory>
#include <string>

// ======================
// 角色职业枚举
// 华工计算机学院学生 vs 化学学院学生
// ======================
enum class PlayerType {
    CS,    // 计算机学院学生（高速 / 高攻击）
    Chem   // 化学学院学生（高血量 / 高防御）
};

class Player {
public:
    Player(PlayerType type, const std::string& name);
    virtual ~Player() = default;

    virtual void init()           = 0;
    virtual void update(float dt) = 0;
    virtual void attack()         = 0;

    void takeDamage(int damage);
    void heal(int amount);
    bool isAlive() const;

    // ── Getters ──
    PlayerType        getType()     const;
    const std::string& getName()    const;
    int               getHp()       const;
    int               getMaxHp()    const;
    int               getLevel()    const;
    int               getExp()      const;
    int               getExpToNext() const;
    int               getAttack()   const;
    int               getDefense()  const;
    float             getSpeed()    const;
    sf::Color         getColor()    const;        // 代表色（用于方块渲染）
    const sf::Vector2f& getPosition() const;

    // ── Setters ──
    void setPosition(const sf::Vector2f& pos);
    void setHp(int hp);

    // ── 经验值 ──
    void addExp(int amount);

protected:
    // 子类在 init() 中赋值
    int   m_hp;
    int   m_maxHp;
    int   m_attack;
    int   m_defense;
    float m_speed;
    float m_critRate;
    sf::Color m_color;       // 代表色

    // ── 经验等级系统 ──
    int   m_level;
    int   m_exp;
    int   m_expToNext;

private:
    PlayerType  m_type;
    std::string m_name;
    sf::Vector2f m_position;
};
