#include "game/ItemPickup.h"
#include <cmath>

ItemPickup::ItemPickup(PickupType type,
                       const sf::Texture& texture,
                       sf::Vector2f pos,
                       float size,
                       int expValue)
    : m_type(type)
    , m_position(pos)
    , m_size(size)
    , m_expValue(expValue)
    , m_sprite(texture)
{
    // 将精灵居中于位置
    float texW = static_cast<float>(texture.getSize().x);
    float texH = static_cast<float>(texture.getSize().y);
    float scale = size / std::max(texW, texH);
    m_sprite.setScale({ scale, scale });
    m_sprite.setOrigin({ texW * 0.5f, texH * 0.5f });
}

void ItemPickup::update(float dt) {
    m_age += dt;

    // 上下浮动动画
    m_bobOffset = std::sin(m_age * 3.0f) * 3.0f;

    // 超时消失
    if (m_age >= m_lifetime) {
        m_alive = false;
    }
}

void ItemPickup::render(sf::RenderWindow& window) {
    sf::Vector2f drawPos = {
        m_position.x,
        m_position.y + m_bobOffset
    };
    m_sprite.setPosition(drawPos);
    window.draw(m_sprite);
}

sf::FloatRect ItemPickup::getBounds() const {
    float half = m_size * 0.5f;
    return sf::FloatRect(sf::Vector2f(m_position.x - half, m_position.y - half),
                         sf::Vector2f(m_size, m_size));
}
