#define _USE_MATH_DEFINES
#include "Enemy.h"
#include <cmath>
#include <algorithm>
#include <random>

static std::mt19937& rng() {
    static std::mt19937 e(std::random_device{}());
    return e;
}

// ============================================================
// Enemy 基类
// ============================================================
Enemy::Enemy(Type type, sf::Vector2f pos)
    : m_type(type), m_pos(pos), m_size(48.f, 48.f) {}

sf::FloatRect Enemy::getBounds() const {
    return sf::FloatRect(m_pos, m_size);
}

// ============================================================
// DashEnemy —— 教务系统服务器机柜
// ============================================================
DashEnemy::DashEnemy(sf::Vector2f pos,
                       sf::Texture* texLeft,
                       sf::Texture* texRight)
    : Enemy(Type::DashServer, pos)
    , m_texLeft(texLeft), m_texRight(texRight)
{
    m_hp    = 60;
    m_maxHp = 60;
    m_dashDamage  = 10;          // 降低初始伤害
    m_slowAmount  = 0.45f;
    m_expValue    = 25;

    sf::Texture* src = texLeft ? texLeft : texRight;
    if (src && src->getSize().y > 0) {
        m_sprite.emplace(*src);
        float scale = 200.f / static_cast<float>(src->getSize().y);
        m_sprite->setScale({scale, scale});
        // origin 设纹理中心（local coord，不受 scale 影响）
        m_sprite->setOrigin(sf::Vector2f(
            static_cast<float>(src->getSize().x) * 0.5f,
            static_cast<float>(src->getSize().y) * 0.5f));
        m_size = sf::Vector2f(src->getSize().x * scale,
                                src->getSize().y * scale);
    } else {
        m_size = sf::Vector2f(50.f, 80.f);
    }

    m_hpBarBg.setSize({m_size.x, 5.f});
    m_hpBarBg.setFillColor(sf::Color(60, 60, 60, 200));
    m_hpBarFill.setSize({m_size.x, 5.f});
    m_hpBarFill.setFillColor(sf::Color(220, 40, 40, 220));
    m_chargeBar.setSize({m_size.x, 4.f});
    m_chargeBar.setFillColor(sf::Color(255, 200, 0, 220));
}

void DashEnemy::enterIdle() {
    m_state = AIState::Idle;
    m_stateTimer = m_idleTime;
}
void DashEnemy::enterAlert() {
    m_state = AIState::Alert;
    m_stateTimer = m_alertTime;
}
void DashEnemy::enterCharge(sf::Vector2f playerPos) {
    m_state = AIState::Charge;
    m_stateTimer = m_chargeTime;
    // 蓄力期间只记录玩家朝向，不锁定冲刺方向
    m_facingRight = (playerPos.x >= m_pos.x);
    m_blinkTimer = 0.f;
    m_blinkOn    = true;
    m_hitPlayer  = false;
}
void DashEnemy::enterDash() {
    m_state = AIState::Dash;
    m_stateTimer  = 0.f;
    m_dashTraveled = 0.f;
    m_hitPlayer   = false;
}
void DashEnemy::enterRecover() {
    m_state = AIState::Recover;
    m_stateTimer = m_recoverTime;
}

bool DashEnemy::isPlayerInSight(sf::Vector2f playerPos) const {
    sf::Vector2f diff = playerPos - m_pos;
    float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);
    
    if (dist > m_detectRange) return false;
    
    // 计算方向角
    float enemyDir = m_facingRight ? 0.f : M_PI;
    float targetAngle = std::atan2(diff.y, diff.x);
    
    // 计算角度差（转为度）
    float angleDiff = std::abs(targetAngle - enemyDir) * 180.f / M_PI;
    // 处理角度环绕
    if (angleDiff > 180.f) angleDiff = 360.f - angleDiff;
    
    return angleDiff <= m_visionAngle / 2.f;
}

void DashEnemy::applyDifficulty(float factor) {
    // 用构造函数已设的值乘以 factor，而不是硬编码
    int baseHp = m_hp;            // 构造函数设的初始血量
    int baseDmg = m_dashDamage;   // 构造函数设的初始伤害
    m_hp    = static_cast<int>(baseHp * factor);
    m_maxHp = m_hp;
    m_dashDamage = static_cast<int>(baseDmg * factor);
    m_dashSpeed = 620.f * std::min(factor, 2.5f);   // 速度上限2.5倍
}

void DashEnemy::update(float dt, sf::Vector2f playerPos) {
    if (isDead()) return;

    // 缓存玩家位置
    m_lastPlayerPos = playerPos;

    // 段错误：暂停状态机，倒计时
    if (m_segfaultTimer > 0.f) {
        m_segfaultTimer -= dt;
        // 冻结结束时结算延迟伤害
        if (m_segfaultTimer <= 0.f && m_segfaultPendingDmg > 0) {
            takeDamage(m_segfaultPendingDmg);
            m_segfaultPendingDmg = 0;
        }
        return;
    }

    sf::Vector2f diff = playerPos - m_pos;
    float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);

    switch (m_state) {
    case AIState::Idle:
        m_stateTimer -= dt;
        // 先检查是否在视野范围内
        if (dist < m_detectRange && m_stateTimer <= 0.f) {
            if (isPlayerInSight(playerPos)) {
                // 玩家在视野内，直接进入蓄力
                enterCharge(playerPos);
            } else {
                // 玩家在警戒范围但不在视野内，进入警戒状态
                enterAlert();
            }
        }
        break;
    case AIState::Alert:
        // 警戒状态：转向玩家方向
        m_facingRight = (playerPos.x >= m_pos.x);
        m_stateTimer -= dt;
        if (m_stateTimer <= 0.f) {
            // 警戒结束，检查玩家是否进入视野
            if (isPlayerInSight(playerPos)) {
                enterCharge(playerPos);
            } else {
                // 玩家仍不在视野，回到待机
                enterIdle();
            }
        }
        break;
    case AIState::Charge:
        m_stateTimer -= dt;
        m_blinkTimer += dt;
        if (m_blinkTimer > 0.12f) {
            m_blinkTimer = 0.f;
            m_blinkOn = !m_blinkOn;
        }
        // 蓄力期间持续追踪玩家朝向
        m_facingRight = (playerPos.x >= m_pos.x);
        
        // 如果玩家跑出视野范围，取消蓄力
        if (dist > m_detectRange * 1.2f) {
            enterIdle();
            break;
        }
        
        if (m_stateTimer <= 0.f) {
            // 蓄力结束，锁定玩家当前位置作为冲刺目标
            sf::Vector2f d = playerPos - m_pos;
            float len = std::sqrt(d.x * d.x + d.y * d.y);
            m_dashDir = (len > 0.f) ? d / len : sf::Vector2f(1.f, 0.f);
            m_facingRight = (m_dashDir.x >= 0.f);
            m_dashMaxDist = std::min(len + 80.f, m_dashMaxDist);
            enterDash();
        }
        break;
    case AIState::Dash: {
        float step = m_dashSpeed * dt;
        m_pos += m_dashDir * step;
        m_dashTraveled += step;
        if (m_dashTraveled >= m_dashMaxDist) enterRecover();
        break;
    }
    case AIState::Recover:
        m_stateTimer -= dt;
        if (m_stateTimer <= 0.f) enterIdle();
        break;
    }

    if (m_state != AIState::Dash && m_state != AIState::Charge)
        m_facingRight = (playerPos.x >= m_pos.x);
}

void DashEnemy::render(sf::RenderWindow& window) {
    if (isDead() || !m_sprite.has_value()) return;

    sf::Texture* tex = m_facingRight ? m_texRight : m_texLeft;
    if (tex) m_sprite->setTexture(*tex);

    if (m_state == AIState::Charge) {
        m_sprite->setColor(m_blinkOn
            ? sf::Color(255, 80, 80, 255)
            : sf::Color(255, 255, 255, 255));
    } else if (m_state == AIState::Dash) {
        m_sprite->setColor(sf::Color(255, 200, 100, 230));
    } else if (m_state == AIState::Alert) {
        // 警戒状态：黄色闪烁
        m_sprite->setColor(sf::Color(255, 230, 100, 255));
    } else {
        m_sprite->setColor(sf::Color(255, 255, 255, 255));
    }

    // 设位置：让精灵左上角对齐 m_pos（m_pos 语义保持为左上角）
    if (m_sprite.has_value()) {
        sf::Vector2f o  = m_sprite->getOrigin();
        sf::Vector2f sc = m_sprite->getScale();
        m_sprite->setPosition({m_pos.x + o.x * sc.x,
                                m_pos.y + o.y * sc.y});
    }
    window.draw(*m_sprite);

    if (m_hp < m_maxHp) {
        float ratio = static_cast<float>(m_hp) / static_cast<float>(m_maxHp);
        m_hpBarBg.setPosition({m_pos.x, m_pos.y - 8.f});
        m_hpBarFill.setSize({m_size.x * ratio, 5.f});
        m_hpBarFill.setPosition({m_pos.x, m_pos.y - 8.f});
        window.draw(m_hpBarBg);
        window.draw(m_hpBarFill);
    }

    if (m_state == AIState::Charge) {
        float ratio = 1.f - (m_stateTimer / m_chargeTime);
        m_chargeBar.setPosition({m_pos.x, m_pos.y - 14.f});
        m_chargeBar.setSize({m_size.x * ratio, 4.f});
        window.draw(m_hpBarBg);
        window.draw(m_chargeBar);
    }
}

int DashEnemy::getPendingDamage(float /*dt*/) {
    int dmg = static_cast<int>(m_pendingDmg);
    m_pendingDmg = 0.f;
    return dmg;
}

// ============================================================
// BlueScreenEnemy —— 蓝屏错误窗口（触碰型伤害）
// ============================================================
BlueScreenEnemy::BlueScreenEnemy(sf::Vector2f pos,
                                 sf::Texture* texLeft,
                                 sf::Texture* texRight)
    : Enemy(Type::BlueScreen, pos)
    , m_texLeft(texLeft), m_texRight(texRight)
{
    m_hp    = 40;
    m_maxHp = 40;
    m_touchDamage = true;
    m_touchDps   = 8;             // 降低初始伤害（DPS）
    m_expValue    = 15;
    m_size = sf::Vector2f(200.f, 163.f);  // 增大尺寸
    m_moveSpeed   = 80.f;

    // 优先使用外部纹理
    sf::Texture* src = texLeft ? texLeft : texRight;
    if (src && src->getSize().y > 0) {
        m_useSpriteTex = true;
        m_sprite.emplace(*src);
        float scale = m_size.y / static_cast<float>(src->getSize().y);
        m_sprite->setScale({scale, scale});
        // origin 设纹理中心（local coord，不受 scale 影响）
        m_sprite->setOrigin(sf::Vector2f(
            static_cast<float>(src->getSize().x) * 0.5f,
            static_cast<float>(src->getSize().y) * 0.5f));
        m_size = sf::Vector2f(src->getSize().x * scale,
                               src->getSize().y * scale);
    } else {
        buildTexture();
        m_sprite.emplace(m_texture);
        m_sprite->setScale({m_size.x / 128.f, m_size.y / 104.f});
        // fallback 纹理 128×104，origin 设中心
        m_sprite->setOrigin(sf::Vector2f(64.f, 52.f));
    }

    m_hpBarBg.setSize({m_size.x, 5.f});
    m_hpBarBg.setFillColor(sf::Color(60, 60, 60, 200));
    m_hpBarFill.setSize({m_size.x, 5.f});
    m_hpBarFill.setFillColor(sf::Color(220, 40, 40, 220));
}

void BlueScreenEnemy::buildTexture() {
    const int TW = 128, TH = 104;
    sf::Image img(sf::Vector2u(TW, TH), sf::Color(0, 120, 215));

    auto setPixel = [&](unsigned int x, unsigned int y, sf::Color c) {
        if (x < static_cast<unsigned int>(TW) && y < static_cast<unsigned int>(TH))
            img.setPixel(sf::Vector2u(x, y), c);
    };
    auto fillRect = [&](int sx, int sy, int w, int h, sf::Color c) {
        for (int y = sy; y < sy + h; ++y)
            for (int x = sx; x < sx + w; ++x)
                setPixel(static_cast<unsigned int>(x), static_cast<unsigned int>(y), c);
    };

    fillRect(8, 8, TW - 16, 3, sf::Color::White);
    fillRect(8, 16, 60, 3, sf::Color(200, 200, 200));
    fillRect(8, 22, 80, 2, sf::Color(180, 180, 180));
    fillRect(8, 28, 45, 2, sf::Color(180, 180, 180));
    fillRect(TW - 40, TH - 44, 32, 32, sf::Color(255, 255, 255, 80));
    fillRect(TW - 40, TH - 44, 6, 6, sf::Color::White);
    fillRect(TW - 34, TH - 38, 6, 6, sf::Color::White);
    fillRect(TW - 28, TH - 44, 6, 6, sf::Color::White);
    fillRect(8, TH - 20, 20, 3, sf::Color(200, 200, 200));
    fillRect(0, TH - 8, TW, 8, sf::Color(0, 80, 170));

    m_texture.loadFromImage(img);
}

void BlueScreenEnemy::update(float dt, sf::Vector2f playerPos) {
    if (isDead()) return;

    // 段错误：暂停移动，倒计时
    if (m_segfaultTimer > 0.f) {
        m_segfaultTimer -= dt;
        // 冻结结束时结算延迟伤害
        if (m_segfaultTimer <= 0.f && m_segfaultPendingDmg > 0) {
            takeDamage(m_segfaultPendingDmg);
            m_segfaultPendingDmg = 0;
        }
        return;
    }

    // 朝向跟踪
    m_facingRight = (playerPos.x >= m_pos.x);

    sf::Vector2f diff = playerPos - m_pos;
    float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);

    switch (m_state) {
    case AIState::Idle:
        m_idleTimer -= dt;
        if (m_idleTimer <= 0.f) {
            // 检查玩家是否在视野范围内
            if (isPlayerInSight(playerPos)) {
                m_state = AIState::Chase;
            } else {
                // 重置待机时间
                m_idleTimer = m_idleDuration;
            }
        }
        break;
    case AIState::Chase:
        if (dist > m_detectRange * 1.5f) {
            // 玩家超出警戒范围，回到待机
            m_state = AIState::Idle;
            m_idleTimer = m_idleDuration;
            m_velocity = sf::Vector2f(0.f, 0.f);
        } else {
            // 平滑追踪玩家
            if (dist > 1.f) {
                sf::Vector2f targetDir = diff / dist;
                sf::Vector2f desiredVel = targetDir * m_moveSpeed;
                
                // 计算速度变化
                sf::Vector2f velDiff = desiredVel - m_velocity;
                float velDiffLen = std::sqrt(velDiff.x * velDiff.x + velDiff.y * velDiff.y);
                
                if (velDiffLen > 0.f) {
                    sf::Vector2f accDir = velDiff / velDiffLen;
                    float accAmount = std::min(velDiffLen, m_acceleration * dt);
                    m_velocity += accDir * accAmount;
                }
                
                // 限制最大速度
                float velLen = std::sqrt(m_velocity.x * m_velocity.x + m_velocity.y * m_velocity.y);
                if (velLen > m_moveSpeed) {
                    m_velocity = (m_velocity / velLen) * m_moveSpeed;
                }
                
                m_pos += m_velocity * dt;
            } else {
                // 到达目标，减速停止
                float velLen = std::sqrt(m_velocity.x * m_velocity.x + m_velocity.y * m_velocity.y);
                if (velLen > 0.f) {
                    float decelAmount = std::min(velLen, m_deceleration * dt);
                    m_velocity -= (m_velocity / velLen) * decelAmount;
                }
            }
        }
        break;
    }
}

bool BlueScreenEnemy::isPlayerInSight(sf::Vector2f playerPos) const {
    sf::Vector2f diff = playerPos - m_pos;
    float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);
    
    if (dist > m_detectRange) return false;
    
    // 全向视野检测
    if (m_visionAngle >= 360.f || m_visionAngle >= 180.f) {
        return true;
    }
    
    // 计算方向角
    float enemyDir = m_facingRight ? 0.f : M_PI;
    float targetAngle = std::atan2(diff.y, diff.x);
    
    // 计算角度差（转为度）
    float angleDiff = std::abs(targetAngle - enemyDir) * 180.f / M_PI;
    if (angleDiff > 180.f) angleDiff = 360.f - angleDiff;
    
    return angleDiff <= m_visionAngle / 2.f;
}

int BlueScreenEnemy::checkTouchDamage(const sf::FloatRect& playerHitbox, float dt, int) {
    if (isDead() || m_segfaultTimer > 0.f) return 0;
    sf::FloatRect eBounds = getBounds();
    m_touching = eBounds.findIntersection(playerHitbox).has_value();

    // 叠层冷却倒计时
    if (m_touchCdTimer > 0.f) {
        m_touchCdTimer -= dt;
    }

    // 触碰中且冷却结束且未满层 → 叠一层 + 扣血
    if (m_touching && m_touchCdTimer <= 0.f && m_stackCount < kMaxStack) {
        m_stackCount++;
        m_touchCdTimer = kTouchCd;
        m_stackEvent = true;   // 通知 Game.cpp
        m_pendingDmg += static_cast<float>(m_touchDps) * kTouchCd;  // 每次叠层结算 DPS*冷却 的伤害
    }
    return 0;
}

void BlueScreenEnemy::applyDifficulty(float factor) {
    int baseHp = m_hp;
    int baseDps = m_touchDps;
    float baseSpd = m_moveSpeed;
    m_hp    = static_cast<int>(baseHp * factor);
    m_maxHp = m_hp;
    m_touchDps  = static_cast<int>(baseDps * factor);
    m_moveSpeed = baseSpd * std::min(factor, 2.5f);   // 速度上限2.5倍
}

void BlueScreenEnemy::render(sf::RenderWindow& window) {
    if (isDead() || !m_sprite.has_value()) return;

    // 切换纹理朝向
    if (m_useSpriteTex) {
        sf::Texture* tex = m_facingRight ? m_texRight : m_texLeft;
        if (tex) m_sprite->setTexture(*tex);
    }

    // 设位置：让精灵左上角对齐 m_pos（m_pos 语义保持为左上角）
    if (m_sprite.has_value()) {
        sf::Vector2f o  = m_sprite->getOrigin();
        sf::Vector2f sc = m_sprite->getScale();
        m_sprite->setPosition({m_pos.x + o.x * sc.x,
                                m_pos.y + o.y * sc.y});
    }
    m_sprite->setColor(sf::Color(255, 255, 255, 255));
    window.draw(*m_sprite);

    if (m_hp < m_maxHp) {
        float ratio = static_cast<float>(m_hp) / static_cast<float>(m_maxHp);
        m_hpBarBg.setPosition({m_pos.x, m_pos.y - 8.f});
        m_hpBarFill.setSize({m_size.x * ratio, 5.f});
        m_hpBarFill.setPosition({m_pos.x, m_pos.y - 8.f});
        window.draw(m_hpBarBg);
        window.draw(m_hpBarFill);
    }
}

int BlueScreenEnemy::getPendingDamage(float dt) {
    // 累积伤害在 update() 里已完成，这里只取整数部分
    (void)dt;
    int dmg = static_cast<int>(m_pendingDmg);
    m_pendingDmg -= static_cast<float>(dmg);
    return dmg;
}

int DashEnemy::checkTouchDamage(const sf::FloatRect& playerHitbox, float /*dt*/, int) {
    if (isDead()) return 0;
    if (m_state != AIState::Dash) return 0; // 仅在冲刺时检测
    if (m_hitPlayer) return 0; // 已命中
    sf::FloatRect eBounds = getBounds();
    if (eBounds.findIntersection(playerHitbox).has_value()) {
        m_hitPlayer = true;
        m_pendingDmg = static_cast<float>(m_dashDamage);
    }
    return 0;
}

// ============================================================
// ThiefEnemy —— 偷外卖贼
// ============================================================
ThiefEnemy::ThiefEnemy(sf::Vector2f pos,
                       sf::Texture* texLeft,
                       sf::Texture* texRight)
    : Enemy(Type::Thief, pos)
    , m_texLeft(texLeft), m_texRight(texRight)
{
    m_hp    = 45;
    m_maxHp = 45;
    m_touchDamage = true;    // 触碰扣血（同时叠层）
    m_expValue    = 0;       // 击杀基础经验为0，全靠返还偷走的经验
    m_size        = sf::Vector2f(160.f, 160.f);
    m_baseSpeed   = 120.f;   // 追踪速度（Game.cpp会用玩家速度*0.6覆盖）
    m_moveSpeed   = m_baseSpeed;

    sf::Texture* src = texLeft ? texLeft : texRight;
    if (src && src->getSize().y > 0) {
        m_useSpriteTex = true;
        m_sprite.emplace(*src);
        float scale = m_size.y / static_cast<float>(src->getSize().y);
        m_sprite->setScale({scale, scale});
        // origin 设纹理中心（local coord，不受 scale 影响）
        m_sprite->setOrigin(sf::Vector2f(
            static_cast<float>(src->getSize().x) * 0.5f,
            static_cast<float>(src->getSize().y) * 0.5f));
        m_size = sf::Vector2f(src->getSize().x * scale, src->getSize().y * scale);
    } else {
        buildTexture();
        m_sprite.emplace(m_texture);
        m_sprite->setScale({m_size.x / 64.f, m_size.y / 64.f});
        // fallback 纹理 64×64，origin 设中心
        m_sprite->setOrigin(sf::Vector2f(32.f, 32.f));
    }

    m_hpBarBg.setSize({m_size.x, 5.f});
    m_hpBarBg.setFillColor(sf::Color(60, 60, 60, 200));
    m_hpBarFill.setSize({m_size.x, 5.f});
    m_hpBarFill.setFillColor(sf::Color(220, 180, 40, 220));  // 金色血条
}

void ThiefEnemy::buildTexture() {
    // Fallback：深色卫衣蒙面人简笔画
    const int TW = 64, TH = 64;
    sf::Image img(sf::Vector2u(TW, TH), sf::Color(0, 0, 0, 0));

    auto setPixel = [&](int x, int y, sf::Color c) {
        if (x >= 0 && x < TW && y >= 0 && y < TH)
            img.setPixel(sf::Vector2u(x, y), c);
    };
    auto fillRect = [&](int sx, int sy, int w, int h, sf::Color c) {
        for (int y = sy; y < sy + h; ++y)
            for (int x = sx; x < sx + w; ++x)
                setPixel(x, y, c);
    };

    // 身体（深灰卫衣）
    fillRect(20, 28, 24, 28, sf::Color(50, 50, 60));
    // 头（蒙面深色）
    fillRect(22, 10, 20, 20, sf::Color(40, 40, 50));
    // 帽檐
    fillRect(16, 10, 32, 5, sf::Color(30, 30, 35));
    // 外卖袋（白色）
    fillRect(34, 34, 14, 16, sf::Color(240, 240, 240, 200));
    fillRect(36, 32, 10, 4,  sf::Color(240, 240, 240, 200));

    m_texture.loadFromImage(img);
}

void ThiefEnemy::update(float dt, sf::Vector2f playerPos) {
    if (isDead()) return;

    m_lastPlayerPos = playerPos;  // 缓存玩家位置

    if (m_segfaultTimer > 0.f) {
        m_segfaultTimer -= dt;
        if (m_segfaultTimer <= 0.f && m_segfaultPendingDmg > 0) {
            takeDamage(m_segfaultPendingDmg);
            m_segfaultPendingDmg = 0;
        }
        return;
    }

    sf::Vector2f diff = playerPos - m_pos;
    float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);

    switch (m_state) {
    case ThiefState::Idle:
        m_stateTimer -= dt;
        if (m_stateTimer <= 0.f) {
            // 检查玩家是否在视野范围内
            if (dist < m_detectRange && isPlayerInSight(playerPos)) {
                m_state = ThiefState::Alert;
                m_stateTimer = 0.3f;  // 短暂警戒
            } else {
                m_stateTimer = 1.0f;  // 重置待机时间
            }
        }
        break;
    case ThiefState::Alert:
        m_facingRight = (playerPos.x >= m_pos.x);
        m_stateTimer -= dt;
        if (m_stateTimer <= 0.f) {
            // 进入追踪状态
            m_state = ThiefState::Chase;
        }
        break;
    case ThiefState::Chase:
        if (dist > m_detectRange * 2.f) {
            // 玩家超出警戒范围，回到待机
            m_state = ThiefState::Idle;
            m_stateTimer = 1.5f;
        } else {
            // 追踪玩家
            if (dist > 1.f) {
                sf::Vector2f dir = diff / dist;
                m_pos += dir * m_moveSpeed * dt;
                m_facingRight = (dir.x >= 0.f);
            }
        }
        break;
    case ThiefState::Flee:
        // 偷完后逃跑
        if (m_fleeTimer > 0.f) {
            m_fleeTimer -= dt;
        }
        
        // 计算逃跑方向
        sf::Vector2f fleeDir = calculateFleeDirection(playerPos);
        float fleeSpd = (m_fleeTimer > 0.f) ? m_fleeSpeed * 1.5f : m_fleeSpeed;
        
        m_pos += fleeDir * fleeSpd * dt;
        m_facingRight = (fleeDir.x >= 0.f);
        
        clampToMap();
        break;
    }

    // 切换纹理朝向
    if (m_useSpriteTex && m_sprite.has_value()) {
        sf::Texture* tex = m_facingRight ? m_texRight : m_texLeft;
        if (tex && tex->getSize().y > 0) {
            m_sprite->setTexture(*tex);
        }
    }
}

bool ThiefEnemy::isPlayerInSight(sf::Vector2f playerPos) const {
    sf::Vector2f diff = playerPos - m_pos;
    float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);
    
    if (dist > m_detectRange) return false;
    
    // 计算方向角
    float enemyDir = m_facingRight ? 0.f : M_PI;
    float targetAngle = std::atan2(diff.y, diff.x);
    
    // 计算角度差（转为度）
    float angleDiff = std::abs(targetAngle - enemyDir) * 180.f / M_PI;
    if (angleDiff > 180.f) angleDiff = 360.f - angleDiff;
    
    return angleDiff <= m_visionAngle / 2.f;
}

sf::Vector2f ThiefEnemy::calculateFleeDirection(sf::Vector2f playerPos) const {
    // 主逃跑方向：远离玩家
    sf::Vector2f awayDir = m_pos - playerPos;
    float awayDist = std::sqrt(awayDir.x * awayDir.x + awayDir.y * awayDir.y);
    
    if (awayDist < 1.f) {
        // 如果玩家在同一位置，随机逃跑
        float angle = static_cast<float>(rng()()) / static_cast<float>(std::mt19937::max()) * M_PI * 2.f;
        return sf::Vector2f(std::cos(angle), std::sin(angle));
    }
    
    awayDir /= awayDist;
    
    // 考虑地图边界，优先选择远离边界的方向
    float mapCenterX = m_mapW / 2.f;
    float mapCenterY = m_mapH / 2.f;
    sf::Vector2f centerDir = m_pos - sf::Vector2f(mapCenterX, mapCenterY);
    float centerDist = std::sqrt(centerDir.x * centerDir.x + centerDir.y * centerDir.y);
    
    if (centerDist > 1.f) {
        centerDir /= centerDist;
        // 混合逃跑方向：主要远离玩家，次要朝向地图边缘
        return (awayDir * 0.8f + centerDir * 0.2f);
    }
    
    return awayDir;
}

void ThiefEnemy::render(sf::RenderWindow& window) {
    if (isDead() || !m_sprite.has_value()) return;

    // 设位置：让精灵左上角对齐 m_pos（m_pos 语义保持为左上角）
    if (m_sprite.has_value()) {
        sf::Vector2f o  = m_sprite->getOrigin();
        sf::Vector2f sc = m_sprite->getScale();
        m_sprite->setPosition({m_pos.x + o.x * sc.x,
                                m_pos.y + o.y * sc.y});
    }
    window.draw(*m_sprite);

    // 血条
    float hpRatio = static_cast<float>(m_hp) / static_cast<float>(m_maxHp);
    m_hpBarBg.setPosition({m_pos.x, m_pos.y - 8.f});
    m_hpBarFill.setSize({m_size.x * hpRatio, 5.f});
    m_hpBarFill.setPosition({m_pos.x, m_pos.y - 8.f});
    window.draw(m_hpBarBg);
    window.draw(m_hpBarFill);
}

int ThiefEnemy::checkTouchDamage(const sf::FloatRect& playerHitbox, float dt, int) {
    (void)dt;
    if (isDead() || m_segfaultTimer > 0.f) return 0;
    // 偷经验已移至 tryStealExp()，此处触碰不再触发偷取
    // 触碰本身不造成额外伤害（getPendingDamage 返回 0）
    return 0;
}

bool ThiefEnemy::popStolenFlag() {
    if (m_stolenThisFrame) {
        m_stolenThisFrame = false;
        return true;
    }
    return false;
}

void ThiefEnemy::tryStealExp(int playerExp, sf::Vector2f playerPos) {
    if (isDead() || m_segfaultTimer > 0.f) return;
    if (m_hasStolen) return;      // 已偷过，不再偷
    if (playerExp <= 0) return;   // 玩家没经验，不偷，继续追踪

    // 距离 < 80px 时触发偷取
    sf::Vector2f diff = playerPos - m_pos;
    float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);
    if (dist > 80.f) return;

    // 偷取成功
    m_hasStolen       = true;
    m_stolenThisFrame = true;

    int stolenAmt = static_cast<int>(playerExp * 0.15f);
    if (stolenAmt < 1) stolenAmt = 1;
    m_lastStolenAmount = stolenAmt;  // 记录本次偷取金额
    addStolenExp(stolenAmt);

    // 进入永久逃跑状态
    m_state     = ThiefState::Flee;
    m_fleeTimer = 0.f;
    if (m_fleeSpeed <= 0.f) m_fleeSpeed = m_baseSpeed * 1.5f;

    // 瞬移：沿远离玩家方向跳跃120px
    sf::Vector2f runDiff = m_pos - m_lastPlayerPos;
    float runDist = std::sqrt(runDiff.x * runDiff.x + runDiff.y * runDiff.y);
    if (runDist > 1.f) {
        m_pos += (runDiff / runDist) * 120.f;
    } else {
        m_pos.x += 120.f;
    }
    clampToMap();
}

void ThiefEnemy::applyDifficulty(float factor) {
    m_hp    = static_cast<int>(45 * factor);
    m_maxHp = m_hp;
    // 速度不随难度提升（偷贼定位是快跑，平衡不需要加速）
}

