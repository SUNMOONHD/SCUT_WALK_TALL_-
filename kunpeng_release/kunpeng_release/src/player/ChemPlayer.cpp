#include "player/ChemPlayer.h"

ChemPlayer::ChemPlayer()
    : Player(PlayerType::Chem, "化学学生")
{
    init();
}

void ChemPlayer::init() {
    // 化学学院：坦克型
    m_maxHp        = 140;
    m_hp           = 140;
    m_attack       = 10;
    m_defense      = 10;
    m_speed        = 140.0f;   // 速度稍慢
    m_critRate     = 0.05f;
    m_color        = sf::Color(50, 200, 100);  // 化学绿
}

void ChemPlayer::update(float dt) {
    // 移动/动画逻辑由 Game.cpp 统一处理
}

void ChemPlayer::attack() {
    // 普通攻击：高防御减免
}
