#include "player/CSPlayer.h"

CSPlayer::CSPlayer()
    : Player(PlayerType::CS, "CS学生")
{
    init();
}

void CSPlayer::init() {
    // 计算机学院：快攻型
    m_maxHp        = 80;
    m_hp           = 80;
    m_attack       = 15;
    m_defense      = 4;
    m_speed        = 200.0f;   // 速度最快
    m_critRate     = 0.20f;    // 暴击率高
    m_color        = sf::Color(60, 140, 255);  // 科技蓝
}

void CSPlayer::update(float dt) {
    // 移动/动画逻辑由 Game.cpp 统一处理
}

void CSPlayer::attack() {
    // 普通攻击：单体，高暴击
}
