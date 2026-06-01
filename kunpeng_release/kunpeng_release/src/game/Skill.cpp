#include "Skill.h"
#include "Game.h"
#include <cmath>
#include <algorithm>
#include <cstdio>

static bool g_pointerStormTexLoaded = false;
static sf::Texture g_pointerStormTex;
#include <algorithm>

// ============================================================
// PointerStormSkill —— 底层·指针风暴
// ============================================================
PointerStormSkill::PointerStormSkill()
    : Skill(0, "底层·指针风暴", "pointer_storm",
            "“内存即战场，指针即权柄”\n"
           )
{
    m_cooldownMax = 2.0f;
    m_range       = 250.f;       // 扩大范围，原150偏小
    m_damage      = 30;          // 提高伤害，原15偏低
    m_maxTargets  = 3;
}

void PointerStormSkill::update(float dt, const sf::Vector2f& playerPos,
                               Game* gameCtx) {
    // 首次加载素材
    if (!g_pointerStormTexLoaded) {
        g_pointerStormTexLoaded = g_pointerStormTex.loadFromFile("assets/skills/pointer_storm.png");
        if (!g_pointerStormTexLoaded) {
            sf::Image img(sf::Vector2u(48u, 48u), sf::Color(0, 255, 60, 200));
            g_pointerStormTex.loadFromImage(img);
            g_pointerStormTexLoaded = true;
        }
    }

    // ── 更新特效计时器 + 清理 ──
    for (auto& e : m_hitEffects) {
        e.timer -= dt;
    }
    m_hitEffects.erase(
        std::remove_if(m_hitEffects.begin(), m_hitEffects.end(),
                       [](const HitEffect& e) { return e.timer <= 0.f; }),
        m_hitEffects.end());

    // 冷却计时
    m_cooldownTimer -= dt;
    if (m_cooldownTimer > 0.f) return;

    // 冷却完毕 → 触发伤害
    m_cooldownTimer = m_cooldownMax;

    // 获取范围内敌人（距离排序，优先近的）
    auto& enemies = gameCtx->getEnemies();
    struct Target {
        int idx;
        float dist;
    };
    std::vector<Target> targets;
    for (int i = 0; i < static_cast<int>(enemies.size()); ++i) {
        if (enemies[i]->isDead()) continue;
        sf::Vector2f diff = enemies[i]->getPosition() - playerPos;
        float d = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        if (d <= m_range) {
            targets.push_back({i, d});
        }
    }
    // 按距离排序
    std::sort(targets.begin(), targets.end(),
              [](const Target& a, const Target& b) { return a.dist < b.dist; });

    // 最多命中 m_maxTargets 个，直接结算伤害（applySegfault 延迟结算容易出问题）
    int hit = 0;
    for (auto& t : targets) {
        if (hit >= m_maxTargets) break;
        enemies[t.idx]->takeDamage(m_damage);
        // 保留 segfault 视觉效果（冻结动画），不叠加伤害延迟
        enemies[t.idx]->applySegfault(0.6f, 0);
        // 对齐到敌人包围盒中心
        sf::FloatRect bounds = enemies[t.idx]->getBounds();
        sf::Vector2f center = {
            bounds.position.x + bounds.size.x / 2.f,
            bounds.position.y + bounds.size.y / 2.f
        };
        m_hitEffects.push_back({center, 1.5f});
        ++hit;
    }
}

void PointerStormSkill::render(sf::RenderWindow& /*window*/,
                                const sf::Vector2f& /*playerPos*/) {
    // 法阵/地面效果在此渲染（当前无）
}

void PointerStormSkill::renderHitEffects(sf::RenderWindow& window) {
    float texSize = static_cast<float>(g_pointerStormTex.getSize().x);
    if (texSize < 1.f) return;
    float targetSize = 260.f;
    float baseScale = targetSize / texSize;

    for (auto& e : m_hitEffects) {
        if (e.timer <= 0.f) continue;
        float progress = 1.f - (e.timer / 1.5f);
        float alpha = (e.timer / 1.5f) * 255.f;
        float scale = baseScale * (0.6f + progress * 0.8f);
        sf::Sprite spr(g_pointerStormTex);
        spr.setScale({scale, scale});
        sf::FloatRect b = spr.getLocalBounds();
        spr.setOrigin({b.size.x / 2.f, b.size.y / 2.f});
        spr.setPosition(e.pos);
        spr.setColor(sf::Color(255, 255, 255, static_cast<std::uint8_t>(alpha)));
        window.draw(spr);
    }
}

std::vector<SkillUpgrade> PointerStormSkill::getUpgrades() const {
    return {
        {"指针伤害强化",  "damage",    5.f,  0},
        {"风暴范围扩展",  "range",     30.f,  1},
        {"弹幕密度提升",  "maxTargets", 4.f,   2},  // +4飞射符号
    };
}

void PointerStormSkill::applyUpgrade(const std::string& statName, float delta) {
    if (statName == "damage")     m_damage += static_cast<int>(delta);
    if (statName == "range")      m_range += delta;
    if (statName == "maxTargets") m_maxTargets += static_cast<int>(delta);
    ++m_level;
}

// ============================================================
// MagicCircleSkill —— 底层·华工法阵
// ============================================================
bool        MagicCircleSkill::s_texLoaded  = false;
sf::Texture MagicCircleSkill::s_emblemTex;
sf::Texture MagicCircleSkill::s_ringTex;

MagicCircleSkill::MagicCircleSkill()
    : Skill(1,
            u8"编译优化·校徽法阵",
            "magic_circle",
            u8"敌入此阵，如代码进入-O3优化：\n要么通过，要么被剔除")
    , m_radius(140.f)
    , m_damage(8)
    , m_tickInterval(0.8f)
    , m_tickTimer(0.f)
    , m_ringAngle(0.f)
    , m_ringSpeed(45.f)   // 45°/s，约8秒转一圈
{
    m_cooldownMax   = 0.f;  // 持续型，无冷却概念
    m_cooldownTimer = 0.f;
}

void MagicCircleSkill::update(float dt, const sf::Vector2f& playerPos,
                               Game* gameCtx) {
    // ── 首次加载纹理 ──
    if (!s_texLoaded) {
        // 校徽：固定圆形图案
        if (!s_emblemTex.loadFromFile("assets/skills/magic_circle_emblem.png")) {
            // fallback：白色半透明圆
            sf::Image img(sf::Vector2u(128u, 128u), sf::Color(255, 220, 80, 160));
            s_emblemTex.loadFromImage(img);
        }
        s_emblemTex.setSmooth(false);

        // 法环：旋转圆环
        if (!s_ringTex.loadFromFile("assets/skills/magic_circle_ring.png")) {
            // fallback：蓝色半透明圆环
            sf::Image img2(sf::Vector2u(128u, 128u), sf::Color(80, 160, 255, 140));
            s_ringTex.loadFromImage(img2);
        }
        s_ringTex.setSmooth(false);

        s_texLoaded = true;
    }

    // ── 法环旋转 ──
    m_ringAngle += m_ringSpeed * dt;
    if (m_ringAngle >= 360.f) m_ringAngle -= 360.f;

    // ── 伤害 tick ──
    m_tickTimer -= dt;
    if (m_tickTimer > 0.f) return;
    m_tickTimer = m_tickInterval;

    // 遍历范围内所有存活敌人并造成伤害
    auto& enemies = gameCtx->getEnemies();
    for (auto& enemy : enemies) {
        if (enemy->isDead()) continue;
        sf::FloatRect bounds = enemy->getBounds();
        sf::Vector2f center = {
            bounds.position.x + bounds.size.x * 0.5f,
            bounds.position.y + bounds.size.y * 0.5f
        };
        sf::Vector2f diff = center - playerPos;
        float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        if (dist <= m_radius) {
            enemy->takeDamage(m_damage);
        }
    }
}

void MagicCircleSkill::render(sf::RenderWindow& window,
                               const sf::Vector2f& playerPos) {
    // playerPos 是玩家精灵视觉中心（由 getGlobalBounds 算出），直接作为法阵圆心
    sf::Vector2f center = playerPos;

    float diameter = m_radius * 2.f;

    // ── 校徽层（固定，不旋转） ──
    {
        sf::Sprite spr(s_emblemTex);
        auto texSz = s_emblemTex.getSize();
        spr.setOrigin({ texSz.x * 0.5f, texSz.y * 0.5f });
        float sc = diameter / static_cast<float>(texSz.x);
        spr.setScale({ sc, sc });
        spr.setPosition(center);
        spr.setColor(sf::Color(255, 255, 255, 200));
        window.draw(spr);
    }

    // ── 法环层（旋转） ──
    {
        sf::Sprite spr(s_ringTex);
        auto texSz = s_ringTex.getSize();
        spr.setOrigin({ texSz.x * 0.5f, texSz.y * 0.5f });
        float sc = diameter / static_cast<float>(texSz.x);
        spr.setScale({ sc, sc });
        spr.setPosition(center);
        spr.setRotation(sf::degrees(m_ringAngle));
        spr.setColor(sf::Color(255, 255, 255, 210));
        window.draw(spr);
    }
}

std::vector<SkillUpgrade> MagicCircleSkill::getUpgrades() const {
    char buf0[64], buf1[64], buf2[64];
    std::snprintf(buf0, sizeof(buf0), u8"法阵灼烧强化\n伤害 %d → %d\n每次触发 +4", m_damage, m_damage + 4);
    std::snprintf(buf1, sizeof(buf1), u8"法阵扩展延伸\n半径 %d → %d px\n覆盖范围 +30", (int)m_radius, (int)m_radius + 30);
    float nextTick = std::max(0.2f, m_tickInterval - 0.15f);
    std::snprintf(buf2, sizeof(buf2), u8"伤害频率强化\n间隔 %.2fs → %.2fs\n伤害更频繁", m_tickInterval, nextTick);
    return {
        {buf0, "damage",  4.f,   0},
        {buf1, "radius",  30.f,  1},
        {buf2, "tick",   -0.15f, 2},
    };
}

void MagicCircleSkill::applyUpgrade(const std::string& statName, float delta) {
    if (statName == "damage") m_damage += static_cast<int>(delta);
    if (statName == "radius") m_radius += delta;
    if (statName == "tick")   m_tickInterval = std::max(0.2f, m_tickInterval + delta);
    ++m_level;
}

// ============================================================
// 华工锦鲤·编译护盾（KoiShieldSkill）
// ============================================================
bool        KoiShieldSkill::s_texLoaded = false;
sf::Texture KoiShieldSkill::s_koiFrames[3];
KoiShieldSkill::KoiShieldSkill()
    : Skill(2,
             u8"华工锦鲤·守护环绕",
             "koi_shield",
             u8"念念不忘，必有回响\n守护之鳞，环绕不散")
    , m_koiCount(2)
    , m_damage(15)
    , m_orbitSpeed(3.14159265f)   // π rad/s = 180°/s
    , m_respawnCooldown(2.0f)
    , m_koiRenderSize(0.f)         // 由 recalcSizeAndRadius 设置
    , m_radius(0.f)                // 由 recalcSizeAndRadius 设置
{
    recalcSizeAndRadius();         // 根据 m_koiCount 计算尺寸和半径
    m_cooldownMax = 0.f;  // 被动技能，无冷却

    // 加载三帧纹理
    if (!s_texLoaded) {
        s_texLoaded = true;
        const char* files[3] = {
            "assets/skills/koi_frame1.png",
            "assets/skills/koi_frame2.png",
            "assets/skills/koi_frame3.png"
        };
        for (int i = 0; i < 3; ++i) {
            if (!s_koiFrames[i].loadFromFile(files[i])) {
                std::fprintf(stderr, "KoiShield: failed to load %s\n", files[i]);
            }
            s_koiFrames[i].setSmooth(false);  // 像素风格，不模糊
        }
    }

    // 初始锦鲤
    m_koi.resize(m_koiCount);
    float angleStep = 2.f * 3.14159265f / (float)m_koiCount;
    for (int i = 0; i < m_koiCount; ++i) {
        auto& k = m_koi[i];
        k.frame       = 0;
        k.frameTimer  = 0.f;
        k.frameDir    = 1;
        k.orbitAngle  = angleStep * (float)i;
        k.respawnTimer = -1.f;
        k.alive       = true;
        k.initialized = true;
        k.sprite.emplace(s_koiFrames[0]);
        sf::Vector2u sz = s_koiFrames[0].getSize();
        k.sprite->setOrigin(sf::Vector2f((float)sz.x * 0.5f, (float)sz.y * 0.5f));
        k.sprite->setScale(sf::Vector2f(m_koiRenderSize / (float)sz.x,
                                        m_koiRenderSize / (float)sz.y));
    }
}

void KoiShieldSkill::recalcSizeAndRadius() {
    // 锦鲤渲染尺寸随数量增长：base 100px，每多1条+8px
    m_koiRenderSize = 100.f + (m_koiCount - 2) * 8.f;
    // 环绕半径随数量增长：base 160px，每多1条+45px，保证鱼不重叠
    m_radius = 160.f + (m_koiCount - 2) * 45.f;
    m_koi.resize(m_koiCount);
    // 重分配角度
    initKoiPositions(sf::Vector2f{});
}

void KoiShieldSkill::initKoiPositions(const sf::Vector2f& center) {
    (void)center;
    float angleStep = 2.f * 3.14159265f / (float)m_koi.size();
    for (size_t i = 0; i < m_koi.size(); ++i) {
        m_koi[i].orbitAngle = angleStep * (float)i;
    }
}

void KoiShieldSkill::updateKoiAnimation(Koi& koi, float dt) {
    koi.frameTimer += dt;
    if (koi.frameTimer >= 0.15f) {
        koi.frameTimer -= 0.15f;
        koi.frame += koi.frameDir;
        if (koi.frame >= 2) {          // 到达 frame3（索引2）
            koi.frameDir = -1;
            koi.frame = 1;              // 回到 frame2
        } else if (koi.frame <= 0 && koi.frameDir < 0) {
            koi.frameDir = 1;
            koi.frame = 1;              // 回到 frame2
        }
    }
}

sf::Vector2f KoiShieldSkill::getKoiWorldPos(const Koi& koi,
                                             const sf::Vector2f& center) const {
    return {
        center.x + m_radius * std::cos(koi.orbitAngle),
        center.y + m_radius * std::sin(koi.orbitAngle)
    };
}

void KoiShieldSkill::update(float dt,
                            const sf::Vector2f& playerPos,
                            Game* gameCtx) {
    // 如果锦鲤数量变化，重新分配
    if ((int)m_koi.size() != m_koiCount) {
        m_koi.resize(m_koiCount);
        float angleStep = 2.f * 3.14159265f / (float)m_koiCount;
        for (size_t i = 0; i < m_koi.size(); ++i) {
            auto& k = m_koi[i];
            if (!k.initialized) {
                k.frame       = 0;
                k.frameTimer  = 0.f;
                k.frameDir    = 1;
                k.respawnTimer = -1.f;
                k.alive       = true;
                k.initialized = true;
                k.sprite.emplace(s_koiFrames[0]);
                sf::Vector2u sz = s_koiFrames[0].getSize();
                k.sprite->setOrigin(sf::Vector2f((float)sz.x * 0.5f, (float)sz.y * 0.5f));
                k.sprite->setScale(sf::Vector2f(m_koiRenderSize / (float)sz.x,
                                                m_koiRenderSize / (float)sz.y));
            }
            k.orbitAngle = angleStep * (float)i;
        }
    }

    for (auto& koi : m_koi) {
        if (koi.alive) {
            // 公转
            koi.orbitAngle += m_orbitSpeed * dt;
            while (koi.orbitAngle > 2.f * 3.14159265f)
                koi.orbitAngle -= 2.f * 3.14159265f;
            while (koi.orbitAngle < 0.f)
                koi.orbitAngle += 2.f * 3.14159265f;

            // 帧动画
            updateKoiAnimation(koi, dt);

            // 更新纹理帧
            if (koi.sprite) koi.sprite->setTexture(s_koiFrames[koi.frame]);

            // 碰撞检测：与所有敌人
            sf::Vector2f koiPos = getKoiWorldPos(koi, playerPos);

            // 碰撞半径（渲染尺寸的 40%）
            float collisionR = m_koiRenderSize * 0.4f;

            for (auto& en : gameCtx->getEnemies()) {
                if (en->isDead()) continue;
                sf::Vector2f ep = en->getPosition();
                float exr = en->getSize().x * 0.5f;
                sf::Vector2f diff = { koiPos.x - ep.x, koiPos.y - ep.y };
                float dist2 = diff.x * diff.x + diff.y * diff.y;
                float thresh = (collisionR + exr) * (collisionR + exr);
                if (dist2 < thresh) {
                    // 命中！造成伤害，锦鲤消失
                    en->takeDamage(m_damage);
                    koi.alive = false;
                    koi.respawnTimer = m_respawnCooldown;
                    break;  // 一条锦鲤一次只打一个敌人
                }
            }
        } else {
            // 重生计时
            koi.respawnTimer -= dt;
            if (koi.respawnTimer <= 0.f) {
                koi.alive = true;
                koi.respawnTimer = -1.f;
                // 先均匀分配角度，再微调到缺口位置
                initKoiPositions(playerPos);
            }
        }
    }
}

// 渲染：按 Y 排序，分两次调用（behind / in-front）
// 由 Game::renderGameplay() 在合适时机调用
void KoiShieldSkill::render(sf::RenderWindow& window,
                            const sf::Vector2f& playerPos) {
    // 按 Y 排序收集位置
    struct DrawEntry {
        sf::Vector2f pos;
        int   frame;
        float angle;
        float y;
    };
    std::vector<DrawEntry> entries;
    for (const auto& koi : m_koi) {
        if (!koi.alive) continue;
        sf::Vector2f p = getKoiWorldPos(koi, playerPos);
        entries.emplace_back(DrawEntry{p, koi.frame, koi.orbitAngle, p.y});
    }
    // 按 Y 升序（上面的先画 = 在后面）
    std::sort(entries.begin(), entries.end(),
              [](const DrawEntry& a, const DrawEntry& b) { return a.y < b.y; });

    for (const auto& e : entries) {
        sf::Sprite sp(s_koiFrames[e.frame]);
        sf::Vector2u sz = s_koiFrames[e.frame].getSize();
        sp.setOrigin(sf::Vector2f((float)sz.x * 0.5f, (float)sz.y * 0.5f));
        // 应用与构造函数一致的缩放（纹理1254px → 渲染80px）
        sp.setScale(sf::Vector2f(m_koiRenderSize / (float)sz.x,
                                 m_koiRenderSize / (float)sz.y));
        // 鱼头朝右（原图），旋转角 = 公转角 + 90°，让鱼头指向切线方向
        sp.setRotation(sf::degrees(e.angle * 57.2957795f + 90.f));
        sp.setPosition(e.pos);
        window.draw(sp);
    }
}

std::vector<SkillUpgrade> KoiShieldSkill::getUpgrades() const {
    char buf0[160], buf1[160], buf2[160];
    // 数量升级：显示下一级数和尺寸/半径变化
    float nextSize   = 100.f + (m_koiCount) * 8.f;   // 升级后 m_koiCount+1 对应的尺寸
    float nextRadius = 160.f + (m_koiCount) * 45.f;   // 升级后对应的半径
    std::snprintf(buf0, sizeof(buf0),
                  u8"召唤锦鲤\n数量 %d → %d 条\n鱼身 %.0f→%.0fpx 半径 %.0f→%.0fpx",
                  m_koiCount, m_koiCount + 1,
                  m_koiRenderSize, nextSize, m_radius, nextRadius);
    std::snprintf(buf1, sizeof(buf1),
                  u8"锦鲤强化\n伤害 %d → %d\n每条锦鲤更强力",
                  m_damage, m_damage + 4);
    float nextCooldown = std::max(0.5f, m_respawnCooldown - 0.3f);
    std::snprintf(buf2, sizeof(buf2),
                  u8"重生加速\n冷却 %.1f → %.1f 秒\n被消耗后更快归来",
                  m_respawnCooldown, nextCooldown);
    return {
        {buf0, "count",    1.f,  0},
        {buf1, "damage",   4.f,  1},
        {buf2, "cooldown", -0.3f, 2},
    };
}

void KoiShieldSkill::applyUpgrade(const std::string& statName, float delta) {
    if (statName == "count") {
        ++m_koiCount;                // 无上限
        recalcSizeAndRadius();       // 尺寸和半径联动
    }
    if (statName == "damage") {
        m_damage += static_cast<int>(delta);
    }
    if (statName == "cooldown") {   // delta 为负数（冷却缩短）
        m_respawnCooldown += delta;
        if (m_respawnCooldown < 0.5f) m_respawnCooldown = 0.5f;
    }
    ++m_level;
}

// ============================================================
// ChemCircleSkill —— 化学·分子法阵
// ============================================================
bool        ChemCircleSkill::s_texLoaded = false;
sf::Texture ChemCircleSkill::s_emblemTex;
sf::Texture ChemCircleSkill::s_ringTex;

ChemCircleSkill::ChemCircleSkill()
    : Skill(1,
            u8"催化循环·分子法阵",
            "chem_circle",
            u8"分子碰撞，连锁反应\n入阵者，键断而崩解")
    , m_radius(140.f)
    , m_damage(8)
    , m_tickInterval(0.8f)
    , m_tickTimer(0.f)
    , m_ringAngle(0.f)
    , m_ringSpeed(45.f)
{
    m_cooldownMax   = 0.f;
    m_cooldownTimer = 0.f;
}

void ChemCircleSkill::update(float dt, const sf::Vector2f& playerPos,
                              Game* gameCtx) {
    if (!s_texLoaded) {
        // 优先化学素材，fallback 到法阵素材
        if (!s_emblemTex.loadFromFile("assets/skills/chem_circle_emblem.png")) {
            if (!s_emblemTex.loadFromFile("assets/skills/magic_circle_emblem.png")) {
                sf::Image img(sf::Vector2u(128u, 128u), sf::Color(160, 80, 220, 160));
                s_emblemTex.loadFromImage(img);
            }
        }
        s_emblemTex.setSmooth(false);

        if (!s_ringTex.loadFromFile("assets/skills/chem_circle_ring.png")) {
            if (!s_ringTex.loadFromFile("assets/skills/magic_circle_ring.png")) {
                sf::Image img2(sf::Vector2u(128u, 128u), sf::Color(200, 60, 60, 140));
                s_ringTex.loadFromImage(img2);
            }
        }
        s_ringTex.setSmooth(false);

        s_texLoaded = true;
    }

    m_ringAngle += m_ringSpeed * dt;
    if (m_ringAngle >= 360.f) m_ringAngle -= 360.f;

    m_tickTimer -= dt;
    if (m_tickTimer > 0.f) return;
    m_tickTimer = m_tickInterval;

    auto& enemies = gameCtx->getEnemies();
    for (auto& enemy : enemies) {
        if (enemy->isDead()) continue;
        sf::FloatRect bounds = enemy->getBounds();
        sf::Vector2f center = {
            bounds.position.x + bounds.size.x * 0.5f,
            bounds.position.y + bounds.size.y * 0.5f
        };
        sf::Vector2f diff = center - playerPos;
        float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        if (dist <= m_radius) {
            enemy->takeDamage(m_damage);
        }
    }
}

void ChemCircleSkill::render(sf::RenderWindow& window,
                              const sf::Vector2f& playerPos) {
    sf::Vector2f center = playerPos;
    float diameter = m_radius * 2.f;

    {
        sf::Sprite spr(s_emblemTex);
        auto texSz = s_emblemTex.getSize();
        spr.setOrigin({ texSz.x * 0.5f, texSz.y * 0.5f });
        float sc = diameter / static_cast<float>(texSz.x);
        spr.setScale({ sc, sc });
        spr.setPosition(center);
        spr.setColor(sf::Color(255, 255, 255, 200));
        window.draw(spr);
    }

    {
        sf::Sprite spr(s_ringTex);
        auto texSz = s_ringTex.getSize();
        spr.setOrigin({ texSz.x * 0.5f, texSz.y * 0.5f });
        float sc = diameter / static_cast<float>(texSz.x);
        spr.setScale({ sc, sc });
        spr.setPosition(center);
        spr.setRotation(sf::degrees(m_ringAngle));
        spr.setColor(sf::Color(255, 255, 255, 210));
        window.draw(spr);
    }
}

std::vector<SkillUpgrade> ChemCircleSkill::getUpgrades() const {
    char buf0[64], buf1[64], buf2[64];
    std::snprintf(buf0, sizeof(buf0), u8"催化强化\n伤害 %d → %d\n反应更剧烈", m_damage, m_damage + 4);
    std::snprintf(buf1, sizeof(buf1), u8"反应范围扩展\n半径 %d → %d px\n分子扩散更远", (int)m_radius, (int)m_radius + 30);
    float nextTick = std::max(0.2f, m_tickInterval - 0.15f);
    std::snprintf(buf2, sizeof(buf2), u8"碰撞频率提升\n间隔 %.2fs → %.2fs\n连锁反应更快", m_tickInterval, nextTick);
    return {
        {buf0, "damage",  4.f,    0},
        {buf1, "radius",  30.f,   1},
        {buf2, "tick",    -0.15f, 2},
    };
}

void ChemCircleSkill::applyUpgrade(const std::string& statName, float delta) {
    if (statName == "damage") m_damage += static_cast<int>(delta);
    if (statName == "radius") m_radius += delta;
    if (statName == "tick")   m_tickInterval = std::max(0.2f, m_tickInterval + delta);
    ++m_level;
}

// ============================================================
// ExothermicStormSkill —— 放热反应·试剂风暴
// ============================================================
static bool g_exothermicTexLoaded = false;
static sf::Texture g_exothermicTex;

ExothermicStormSkill::ExothermicStormSkill()
    : Skill(0, u8"放热反应·试剂风暴", "exothermic_storm",
            u8"能量释放，炽焰飞溅\n试剂如箭，穿裂一切")
{
    m_cooldownMax = 2.0f;
    m_range       = 150.f;
    m_damage      = 15;
    m_maxTargets  = 3;
}

void ExothermicStormSkill::update(float dt, const sf::Vector2f& playerPos,
                                   Game* gameCtx) {
    if (!g_exothermicTexLoaded) {
        // 优先化学素材，fallback 到指针风暴素材
        if (!g_exothermicTex.loadFromFile("assets/skills/exothermic_storm.png")) {
            if (!g_exothermicTex.loadFromFile("assets/skills/pointer_storm.png")) {
                sf::Image img(sf::Vector2u(48u, 48u), sf::Color(220, 80, 40, 200));
                g_exothermicTex.loadFromImage(img);
                g_exothermicTexLoaded = true;
            }
        }
        if (!g_exothermicTexLoaded) g_exothermicTexLoaded = true;
    }

    for (auto& e : m_hitEffects) {
        e.timer -= dt;
    }
    m_hitEffects.erase(
        std::remove_if(m_hitEffects.begin(), m_hitEffects.end(),
                       [](const HitEffect& e) { return e.timer <= 0.f; }),
        m_hitEffects.end());

    m_cooldownTimer -= dt;
    if (m_cooldownTimer > 0.f) return;
    m_cooldownTimer = m_cooldownMax;

    auto& enemies = gameCtx->getEnemies();
    struct Target {
        int idx;
        float dist;
    };
    std::vector<Target> targets;
    for (int i = 0; i < static_cast<int>(enemies.size()); ++i) {
        if (enemies[i]->isDead()) continue;
        sf::Vector2f diff = enemies[i]->getPosition() - playerPos;
        float d = std::sqrt(diff.x * diff.x + diff.y * diff.y);
        if (d <= m_range) {
            targets.push_back({i, d});
        }
    }
    std::sort(targets.begin(), targets.end(),
              [](const Target& a, const Target& b) { return a.dist < b.dist; });

    int hit = 0;
    for (auto& t : targets) {
        if (hit >= m_maxTargets) break;
        enemies[t.idx]->applySegfault(0.75f, m_damage);
        sf::FloatRect bounds = enemies[t.idx]->getBounds();
        sf::Vector2f center = {
            bounds.position.x + bounds.size.x / 2.f,
            bounds.position.y + bounds.size.y / 2.f
        };
        m_hitEffects.push_back({center, 1.5f});
        ++hit;
    }
}

void ExothermicStormSkill::render(sf::RenderWindow& /*window*/,
                                   const sf::Vector2f& /*playerPos*/) {
    // 法阵/地面效果在此渲染（当前无）
}

void ExothermicStormSkill::renderHitEffects(sf::RenderWindow& window) {
    float texSize = static_cast<float>(g_exothermicTex.getSize().x);
    if (texSize < 1.f) return;
    float targetSize = 260.f;
    float baseScale = targetSize / texSize;

    for (auto& e : m_hitEffects) {
        if (e.timer <= 0.f) continue;
        float progress = 1.f - (e.timer / 1.5f);
        float alpha = (e.timer / 1.5f) * 255.f;
        float scale = baseScale * (0.6f + progress * 0.8f);
        sf::Sprite spr(g_exothermicTex);
        spr.setScale({scale, scale});
        sf::FloatRect b = spr.getLocalBounds();
        spr.setOrigin({b.size.x / 2.f, b.size.y / 2.f});
        spr.setPosition(e.pos);
        spr.setColor(sf::Color(255, 255, 255, static_cast<std::uint8_t>(alpha)));
        window.draw(spr);
    }
}

std::vector<SkillUpgrade> ExothermicStormSkill::getUpgrades() const {
    return {
        {u8"反应热强化",   "damage",     5.f,  0},
        {u8"飞溅范围扩展", "range",      30.f, 1},
        {u8"试剂浓度提升", "maxTargets", 4.f,  2},
    };
}

void ExothermicStormSkill::applyUpgrade(const std::string& statName, float delta) {
    if (statName == "damage")     m_damage += static_cast<int>(delta);
    if (statName == "range")      m_range += delta;
    if (statName == "maxTargets") m_maxTargets += static_cast<int>(delta);
    ++m_level;
}

// ============================================================
// 化学·炼金环绕（ChemOrbitSkill）
// 完全移植锦鲤逻辑，使用化学主题
// ============================================================
bool        ChemOrbitSkill::s_texLoaded = false;
sf::Texture ChemOrbitSkill::s_orbFrames[3];

ChemOrbitSkill::ChemOrbitSkill()
    : Skill(3,
             u8"炼金护盾·烬焰绕行",
             "chem_orbit",
             u8"冻结与爆炸，皆在掌中\n炽焰弹珠，环绕护体")
    , m_orbCount(2)
    , m_damage(15)
    , m_orbitSpeed(3.14159265f)
    , m_respawnCooldown(2.0f)
    , m_orbRenderSize(0.f)
    , m_radius(0.f)
{
    recalcSizeAndRadius();
    m_cooldownMax = 0.f;

    if (!s_texLoaded) {
        s_texLoaded = true;
        // 优先使用化学专属素材，无则 fallback 到锦鲤素材
        const char* files[3] = {
            "assets/skills/chem_frame1.png",
            "assets/skills/chem_frame2.png",
            "assets/skills/chem_frame3.png"
        };
        const char* fallback[3] = {
            "assets/skills/koi_frame1.png",
            "assets/skills/koi_frame2.png",
            "assets/skills/koi_frame3.png"
        };
        for (int i = 0; i < 3; ++i) {
            if (!s_orbFrames[i].loadFromFile(files[i])) {
                if (!s_orbFrames[i].loadFromFile(fallback[i])) {
                    std::fprintf(stderr, "ChemOrbit: failed to load frame %d\n", i);
                }
            }
            s_orbFrames[i].setSmooth(false);
        }
    }

    m_orbs.resize(m_orbCount);
    float angleStep = 2.f * 3.14159265f / (float)m_orbCount;
    for (int i = 0; i < m_orbCount; ++i) {
        auto& o = m_orbs[i];
        o.frame       = 0;
        o.frameTimer  = 0.f;
        o.frameDir    = 1;
        o.orbitAngle  = angleStep * (float)i;
        o.respawnTimer = -1.f;
        o.alive       = true;
        o.initialized = true;
        o.sprite.emplace(s_orbFrames[0]);
        sf::Vector2u sz = s_orbFrames[0].getSize();
        o.sprite->setOrigin(sf::Vector2f((float)sz.x * 0.5f, (float)sz.y * 0.5f));
        o.sprite->setScale(sf::Vector2f(m_orbRenderSize / (float)sz.x,
                                        m_orbRenderSize / (float)sz.y));
    }
}

void ChemOrbitSkill::recalcSizeAndRadius() {
    m_orbRenderSize = 100.f + (m_orbCount - 2) * 8.f;
    m_radius        = 160.f + (m_orbCount - 2) * 45.f;
    m_orbs.resize(m_orbCount);
    initOrbPositions(sf::Vector2f{});
}

void ChemOrbitSkill::initOrbPositions(const sf::Vector2f& center) {
    (void)center;
    float angleStep = 2.f * 3.14159265f / (float)m_orbs.size();
    for (size_t i = 0; i < m_orbs.size(); ++i) {
        m_orbs[i].orbitAngle = angleStep * (float)i;
    }
}

void ChemOrbitSkill::updateOrbAnimation(Orb& orb, float dt) {
    orb.frameTimer += dt;
    if (orb.frameTimer >= 0.15f) {
        orb.frameTimer -= 0.15f;
        orb.frame += orb.frameDir;
        if (orb.frame >= 2) {
            orb.frameDir = -1;
            orb.frame = 1;
        } else if (orb.frame <= 0 && orb.frameDir < 0) {
            orb.frameDir = 1;
            orb.frame = 1;
        }
    }
}

sf::Vector2f ChemOrbitSkill::getOrbWorldPos(const Orb& orb,
                                             const sf::Vector2f& center) const {
    return {
        center.x + m_radius * std::cos(orb.orbitAngle),
        center.y + m_radius * std::sin(orb.orbitAngle)
    };
}

void ChemOrbitSkill::update(float dt,
                             const sf::Vector2f& playerPos,
                             Game* gameCtx) {
    if ((int)m_orbs.size() != m_orbCount) {
        m_orbs.resize(m_orbCount);
        float angleStep = 2.f * 3.14159265f / (float)m_orbCount;
        for (size_t i = 0; i < m_orbs.size(); ++i) {
            auto& o = m_orbs[i];
            if (!o.initialized) {
                o.frame       = 0;
                o.frameTimer  = 0.f;
                o.frameDir    = 1;
                o.respawnTimer = -1.f;
                o.alive       = true;
                o.initialized = true;
                o.sprite.emplace(s_orbFrames[0]);
                sf::Vector2u sz = s_orbFrames[0].getSize();
                o.sprite->setOrigin(sf::Vector2f((float)sz.x * 0.5f, (float)sz.y * 0.5f));
                o.sprite->setScale(sf::Vector2f(m_orbRenderSize / (float)sz.x,
                                                m_orbRenderSize / (float)sz.y));
            }
            o.orbitAngle = angleStep * (float)i;
        }
    }

    for (auto& orb : m_orbs) {
        if (orb.alive) {
            orb.orbitAngle += m_orbitSpeed * dt;
            while (orb.orbitAngle > 2.f * 3.14159265f)
                orb.orbitAngle -= 2.f * 3.14159265f;
            while (orb.orbitAngle < 0.f)
                orb.orbitAngle += 2.f * 3.14159265f;

            updateOrbAnimation(orb, dt);

            if (orb.sprite) orb.sprite->setTexture(s_orbFrames[orb.frame]);

            sf::Vector2f orbPos = getOrbWorldPos(orb, playerPos);
            float collisionR = m_orbRenderSize * 0.4f;

            for (auto& en : gameCtx->getEnemies()) {
                if (en->isDead()) continue;
                sf::Vector2f ep  = en->getPosition();
                float        exr = en->getSize().x * 0.5f;
                sf::Vector2f diff = { orbPos.x - ep.x, orbPos.y - ep.y };
                float dist2  = diff.x * diff.x + diff.y * diff.y;
                float thresh = (collisionR + exr) * (collisionR + exr);
                if (dist2 < thresh) {
                    en->takeDamage(m_damage);
                    orb.alive        = false;
                    orb.respawnTimer = m_respawnCooldown;
                    break;
                }
            }
        } else {
            orb.respawnTimer -= dt;
            if (orb.respawnTimer <= 0.f) {
                orb.alive        = true;
                orb.respawnTimer = -1.f;
                initOrbPositions(playerPos);
            }
        }
    }
}

void ChemOrbitSkill::render(sf::RenderWindow& window,
                             const sf::Vector2f& playerPos) {
    struct DrawEntry {
        sf::Vector2f pos;
        int   frame;
        float angle;
        float y;
    };
    std::vector<DrawEntry> entries;
    for (const auto& orb : m_orbs) {
        if (!orb.alive) continue;
        sf::Vector2f p = getOrbWorldPos(orb, playerPos);
        entries.emplace_back(DrawEntry{p, orb.frame, orb.orbitAngle, p.y});
    }
    std::sort(entries.begin(), entries.end(),
              [](const DrawEntry& a, const DrawEntry& b) { return a.y < b.y; });

    for (const auto& e : entries) {
        sf::Sprite sp(s_orbFrames[e.frame]);
        sf::Vector2u sz = s_orbFrames[e.frame].getSize();
        sp.setOrigin(sf::Vector2f((float)sz.x * 0.5f, (float)sz.y * 0.5f));
        sp.setScale(sf::Vector2f(m_orbRenderSize / (float)sz.x,
                                 m_orbRenderSize / (float)sz.y));
        sp.setRotation(sf::degrees(e.angle * 57.2957795f + 90.f));
        sp.setPosition(e.pos);
        window.draw(sp);
    }
}

std::vector<SkillUpgrade> ChemOrbitSkill::getUpgrades() const {
    char buf0[160], buf1[160], buf2[160];
    float nextSize   = 100.f + (m_orbCount) * 8.f;
    float nextRadius = 160.f + (m_orbCount) * 45.f;
    std::snprintf(buf0, sizeof(buf0),
                  u8"增加弹珠\n数量 %d → %d 颗\n弹珠 %.0f→%.0fpx 半径 %.0f→%.0fpx",
                  m_orbCount, m_orbCount + 1,
                  m_orbRenderSize, nextSize, m_radius, nextRadius);
    std::snprintf(buf1, sizeof(buf1),
                  u8"爆炸强化\n伤害 %d → %d\n每颗弹珠更具破坏力",
                  m_damage, m_damage + 4);
    float nextCooldown = std::max(0.5f, m_respawnCooldown - 0.3f);
    std::snprintf(buf2, sizeof(buf2),
                  u8"快速合成\n冷却 %.1f → %.1f 秒\n弹珠消耗后更快归来",
                  m_respawnCooldown, nextCooldown);
    return {
        {buf0, "count",    1.f,  0},
        {buf1, "damage",   4.f,  1},
        {buf2, "cooldown", -0.3f, 2},
    };
}

void ChemOrbitSkill::applyUpgrade(const std::string& statName, float delta) {
    if (statName == "count") {
        ++m_orbCount;
        recalcSizeAndRadius();
    }
    if (statName == "damage") {
        m_damage += static_cast<int>(delta);
    }
    if (statName == "cooldown") {
        m_respawnCooldown += delta;
        if (m_respawnCooldown < 0.5f) m_respawnCooldown = 0.5f;
    }
    ++m_level;
}
