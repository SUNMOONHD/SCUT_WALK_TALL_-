#pragma once
#include "player/Player.h"

// ======================
// 化学学院学生
// 特点：血量厚，防御高
// 代表色：化学绿
// ======================
class ChemPlayer : public Player {
public:
    ChemPlayer();
    ~ChemPlayer() override = default;

    void init()           override;
    void update(float dt) override;
    void attack()         override;
};
