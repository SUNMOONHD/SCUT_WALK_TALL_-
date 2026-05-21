#pragma once
#include "player/Player.h"

// ============================================================
// 计算机学院学生（玩家派生类）
// 继承自 Player 基类
// 
// 职业特点：
// - 移动速度快（适合快速机动和风筝敌人）
// - 攻击力偏高（输出型角色）
// - 代表色：科技蓝（#4A90D9）
// 
// 属性成长：
// - 等级提升时，攻击力和速度增长较快
// - 生命值和防御力增长较慢
// ============================================================
class CSPlayer : public Player {
public:
    /**
     * @brief 构造函数
     * 调用父类构造函数，设置玩家类型为 CS，名称为 "Computer Science Student"
     */
    CSPlayer();
    
    /**
     * @brief 析构函数
     * 默认实现，确保正确释放资源
     */
    ~CSPlayer() override = default;

    /**
     * @brief 初始化属性
     * 设置初始属性值：
     * - 生命值：较低
     * - 攻击力：较高
     * - 防御力：较低
     * - 移动速度：较高
     * - 暴击率：中等
     * - 代表色：科技蓝
     */
    void init() override;
    
    /**
     * @brief 更新玩家状态
     * @param dt 时间增量（秒）
     * 处理计算机学生特有的状态更新逻辑
     */
    void update(float dt) override;
    
    /**
     * @brief 执行攻击
     * 实现计算机学生的攻击逻辑，可能包含技能特效或特殊攻击方式
     */
    void attack() override;
};
