#include "player/Player.h"
#include <algorithm>

Player::Player(PlayerType type, const std::string& name)
    : m_type(type)
    , m_name(name)
    , m_hp(100)
    , m_maxHp(100)
    , m_attack(10)
    , m_defense(5)
    , m_speed(150.0f)
    , m_critRate(0.1f)
    , m_color(sf::Color::White)
    , m_position(0.f, 0.f)
    , m_level(1)
    , m_exp(0)
    , m_expToNext(100)
{}

void Player::takeDamage(int damage) {
    int dmg = std::max(0, damage - m_defense);
    m_hp    = std::max(0, m_hp - dmg);
}

void Player::heal(int amount) {
    m_hp = std::min(m_maxHp, m_hp + amount);
}

bool Player::isAlive() const {
    return m_hp > 0;
}

PlayerType Player::getType() const         { return m_type; }
const std::string& Player::getName() const { return m_name; }
int  Player::getHp()       const           { return m_hp; }
int  Player::getMaxHp()    const           { return m_maxHp; }
int  Player::getAttack()   const           { return m_attack; }
int  Player::getDefense()  const           { return m_defense; }
float Player::getSpeed()   const           { return m_speed; }
sf::Color Player::getColor() const         { return m_color; }
const sf::Vector2f& Player::getPosition() const { return m_position; }

void Player::setPosition(const sf::Vector2f& pos) { m_position = pos; }
void Player::setHp(int hp)                        { m_hp = std::clamp(hp, 0, m_maxHp); }

int   Player::getLevel()     const { return m_level; }
int   Player::getExp()       const { return m_exp; }
int   Player::getExpToNext() const { return m_expToNext; }

void Player::addExp(int amount) {
    m_exp += amount;
    // 被偷经验时不跌级，最低 clamp 到 0
    if (m_exp < 0) m_exp = 0;
    while (m_exp >= m_expToNext) {
        m_exp -= m_expToNext;
        m_level++;
        m_expToNext = static_cast<int>(m_expToNext * 1.35f);
        m_maxHp += 10;
        m_hp = m_maxHp; // 升级回满血
        m_attack += 2;
        m_defense += 1;
    }
}
