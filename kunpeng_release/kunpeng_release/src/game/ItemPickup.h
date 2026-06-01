#pragma once

#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/System/Clock.hpp>

// ======================
// 掉落道具类型
// ======================
enum class PickupType {
    Heal,   // 华农牛奶 · 回血
    Speed,  // 加速道具
    Exp     // 经验道具（敌人掉落）
};

// ======================
// 地图上的掉落道具
// ======================
class ItemPickup {
public:
    ItemPickup() = default;
    ItemPickup(PickupType type,
               const sf::Texture& texture,
               sf::Vector2f pos,
               float size,
               int expValue = 50);

    void update(float dt);
    void render(sf::RenderWindow& window);

    // 碰撞矩形（世界坐标）
    sf::FloatRect getBounds() const;

    PickupType getType() const { return m_type; }
    bool isAlive() const { return m_alive; }
    void kill() { m_alive = false; }

    // 浮动动画效果
    float getBobOffset() const { return m_bobOffset; }
    int  getExpValue() const { return m_expValue; }

private:
    sf::Sprite  m_sprite;
    sf::Vector2f m_position;
    PickupType   m_type;
    bool         m_alive = true;
    float        m_size;
    int          m_expValue = 50;
    float        m_age = 0.0f;       // 存在时间（用于浮动动画）
    float        m_bobOffset = 0.0f; // 上下浮动偏移
    float        m_lifetime = 30.0f; // 最大存在时间（秒）
};
