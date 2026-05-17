#pragma once
#include "player/Player.h"

// ======================
// 计算机学院学生
// 特点：移动速度快，攻击偏高
// 代表色：科技蓝
// ======================
class CSPlayer : public Player {
public:
    CSPlayer();
    ~CSPlayer() override = default;

    void init()           override;
    void update(float dt) override;
    void attack()         override;
};
