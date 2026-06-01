#include "Game.h"
#include "map/Map.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>

Game::Game()
    : m_window(sf::VideoMode({ DesignWidth, DesignHeight }), "SCUT WALK TALL",
               sf::Style::Titlebar | sf::Style::Close)
    , m_running(true)
    , m_isFullscreen(false)
    , m_textureLoaded(false)
    , m_showSettingsPanel(false)
    , m_masterVolume(1.0f)
    , m_bgmVolume(0.8f)
    , m_sfxVolume(0.9f)
    , m_masterPct(100)
    , m_bgmPct(80)
    , m_sfxPct(90)
    , m_videoLoaded(false)
    , m_videoTimer(0.0f)
    , m_currentFrame(0)
    , m_switchFullscreenPending(false)
    , m_uiScale(1.0f)
    , m_showComingSoon(false)
    , m_comingLoaded(false)
    , m_comingTimer(0.0f)
    , m_comingFrame(0)
    , m_showCharSelect(false)
    , m_csAvatarLoaded(false)
    , m_chemAvatarLoaded(false)
    , m_state(GameState::MainMenu)
    , m_selectedCharIdx(-1)
    , m_playerSpeed(160.0f)
    , m_pickupSpawnTimer(0.0f)
    , m_nextSpawnInterval(15.0f)
    , m_speedBuffTimer(0.0f)
    , m_expBuffTimer(0.0f)
{
    sf::Image icon;
    if (icon.loadFromFile("assets/sprites/ui/window_icon.png")) {
        m_window.setIcon(icon);
    }

    if (m_menuBgTexture.loadFromFile("assets/sprites/ui/main_menu_bg.png")) {
        m_menuBgTexture.setSmooth(true);   // 开启纹理平滑提高清晰度
        m_menuBgSprite.emplace(m_menuBgTexture);
        m_textureLoaded = true;
    }

    if (!m_textureLoaded) {
        m_fallbackBg.setSize({ 1280, 720 });
        m_fallbackBg.setFillColor(sf::Color(40, 25, 20));
    }

    ImGui::SFML::Init(m_window);

    setupFont(1.0f);

    setupGufengStyle();
    loadVideoFrames();
    loadComingFrames();

    // 加载角色头像
    m_csAvatarLoaded = m_csAvatarTexture.loadFromFile("assets/images/cs_student.png");
    m_chemAvatarLoaded = m_chemAvatarTexture.loadFromFile("assets/images/chem_student.png");

    // ── 加载主菜单 BGM ──
    playBGM(BGMType::Menu);
}

void Game::setupFont(float scale) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 2;
    m_customFont = io.Fonts->AddFontFromFileTTF(
        "assets/fonts/main_fonts.ttf", 22.0f * scale, &cfg,
        io.Fonts->GetGlyphRangesChineseFull()
    );
    if (!m_customFont) {
        m_customFont = io.Fonts->AddFontDefault();
    }
    io.Fonts->Build();
    ImGui::SFML::UpdateFontTexture();
}

// ======================
// 根据当前窗口大小计算 UI 缩放比例，并重建 ImGui 上下文
// 注意：调用前必须已 Shutdown ImGui
// ======================
void Game::updateUIScale() {
    auto winSize = m_window.getSize();
    float scaleX = (float)winSize.x / DesignWidth;
    float scaleY = (float)winSize.y / DesignHeight;
    m_uiScale = std::min(scaleX, scaleY);

    ImGui::SFML::Init(m_window);
    setupFont(m_uiScale);
    setupGufengStyle();
}

// ======================
// 🔥 中国古风UI + 全局TTF字体
// ======================
void Game::setupGufengStyle()
{
    // ======================
    // 字体已在构造函数中加载，这里只设样式
    // ======================

    // ======================
    // 古风样式（根据背景图配色优化）
    // ======================
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 12;       // 窗口圆角
    style.FrameRounding = 10;        // 按钮、滑块、输入框圆角
    style.GrabRounding = 6;          // 滑块手柄圆角
    style.PopupRounding = 12;        // 弹窗圆角
    style.ScrollbarRounding = 8;     // 滚动条圆角
    style.TabRounding = 8;           // Tab 圆角
    style.ChildRounding = 10;        // 子窗口圆角

    style.WindowBorderSize = 0.5f;
    style.FrameBorderSize = 0;
    style.WindowPadding = { 10, 10 };
    style.FramePadding = { 14, 10 };
    style.ItemSpacing = { 10, 8 };
    style.ItemInnerSpacing = { 6, 6 };
    style.ButtonTextAlign = { 0.5f, 0.5f };

    // 配色方案：中国传统色多层次组合
    // 宣纸白(底)、墨黛色(文)、朱砂红(印)、胭脂红(花)、
    // 赭石色(土)、藤黄(金)、石绿(玉)、靛蓝(天)、藕荷(紫)
    ImVec4* colors = style.Colors;

    // ── 窗口 ── 宣纸白底，微微透出背景
    colors[ImGuiCol_WindowBg]        = ImVec4(0.96f, 0.93f, 0.87f, 0.50f);
    colors[ImGuiCol_PopupBg]         = ImVec4(0.97f, 0.94f, 0.89f, 0.92f);
    colors[ImGuiCol_Border]          = ImVec4(0.72f, 0.18f, 0.21f, 0.35f);   // 朱砂红淡边
    colors[ImGuiCol_BorderShadow]    = ImVec4(0.85f, 0.78f, 0.68f, 0.15f);   // 赭石色柔影

    // ── 文字 ── 墨黛色主文，淡墨灰次文
    colors[ImGuiCol_Text]            = ImVec4(0.20f, 0.20f, 0.26f, 1.0f);    // 墨黛色
    colors[ImGuiCol_TextDisabled]    = ImVec4(0.60f, 0.56f, 0.52f, 0.55f);   // 淡墨

    // ── 按钮 ── 象牙白底 → 藕荷色悬浮 → 胭脂红按下
    colors[ImGuiCol_Button]          = ImVec4(0.98f, 0.96f, 0.92f, 0.65f);   // 象牙白
    colors[ImGuiCol_ButtonHovered]   = ImVec4(0.82f, 0.55f, 0.68f, 0.80f);   // 藕荷色
    colors[ImGuiCol_ButtonActive]    = ImVec4(0.80f, 0.24f, 0.38f, 0.90f);   // 胭脂红

    // ── 输入框/滑块轨道 ── 月白底，鼠标悬停变天青色（汝窑）
    colors[ImGuiCol_FrameBg]         = ImVec4(0.86f, 0.89f, 0.92f, 0.75f);   // 月白（与天青同属冷调）
    colors[ImGuiCol_FrameBgHovered]  = ImVec4(0.68f, 0.85f, 0.88f, 0.80f);   // 天青色（汝窑）
    colors[ImGuiCol_FrameBgActive]   = ImVec4(0.58f, 0.78f, 0.82f, 0.95f);   // 天青深（开片）

    // ── 滑块手柄 ── 天青色（汝窑质感）
    colors[ImGuiCol_SliderGrab]      = ImVec4(0.62f, 0.80f, 0.84f, 0.92f);   // 天青
    colors[ImGuiCol_SliderGrabActive]= ImVec4(0.72f, 0.86f, 0.88f, 1.0f);    // 天青亮

    // ── 头部/选中 ── 靛蓝 + 石绿
    colors[ImGuiCol_Header]          = ImVec4(0.82f, 0.55f, 0.68f, 0.35f);   // 藕荷色
    colors[ImGuiCol_HeaderHovered]   = ImVec4(0.85f, 0.45f, 0.55f, 0.55f);   // 藕荷深
    colors[ImGuiCol_HeaderActive]    = ImVec4(0.72f, 0.18f, 0.21f, 0.75f);   // 朱砂红

    // ── 复选框 ── 石绿 + 藤黄
    colors[ImGuiCol_CheckMark]       = ImVec4(0.28f, 0.60f, 0.42f, 1.0f);    // 石绿
    colors[ImGuiCol_ModalWindowDimBg]= ImVec4(0.20f, 0.18f, 0.16f, 0.45f);   // 墨色遮罩

    // ── 滚动条 ── 藤黄点缀
    colors[ImGuiCol_ScrollbarBg]     = ImVec4(0.92f, 0.88f, 0.82f, 0.30f);
    colors[ImGuiCol_ScrollbarGrab]   = ImVec4(0.88f, 0.72f, 0.32f, 0.60f);   // 藤黄
    colors[ImGuiCol_ScrollbarGrabHovered]= ImVec4(0.85f, 0.65f, 0.28f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.78f, 0.58f, 0.22f, 1.0f);

    // ── Tab / 分隔线 ── 靛蓝
    colors[ImGuiCol_Tab]             = ImVec4(0.92f, 0.90f, 0.85f, 0.60f);   // 宣纸淡
    colors[ImGuiCol_TabHovered]      = ImVec4(0.82f, 0.55f, 0.68f, 0.70f);   // 藕荷
    colors[ImGuiCol_TabActive]       = ImVec4(0.98f, 0.96f, 0.92f, 0.90f);   // 象牙白
    colors[ImGuiCol_Separator]       = ImVec4(0.72f, 0.18f, 0.21f, 0.25f);   // 朱砂淡线
    colors[ImGuiCol_SeparatorHovered]= ImVec4(0.85f, 0.72f, 0.32f, 0.50f);   // 藤黄
}
Game::~Game() {
    ImGui::SFML::Shutdown();
}

void Game::run() {
    m_window.setFramerateLimit(60); // 限制帧率，防止 CPU 满载和长时间运行不稳定
    while (m_running && m_window.isOpen()) {
        float dt = m_clock.restart().asSeconds();
        dt = std::min(dt, 0.05f); // 防止大 dt 导致逻辑异常
        handleEvents();
        update(dt);
        render();
    }
}

void Game::handleEvents() {
    while (auto event = m_window.pollEvent()) {
        ImGui::SFML::ProcessEvent(m_window, *event);
        if (event->is<sf::Event::Closed>()) {
            m_window.close();
            m_running = false;
        }
        if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
            if (key->code == sf::Keyboard::Key::F11) {
                m_switchFullscreenPending = true;
            }
            if (m_isDead && key->code == sf::Keyboard::Key::Escape) {
                m_isDead = false;
                m_state = GameState::MainMenu;
                playBGM(BGMType::Menu);
            }
        }
        if (event->is<sf::Event::Resized>()) {
            if (m_state == GameState::Gameplay) {
                m_camera.setSize(static_cast<sf::Vector2f>(m_window.getSize()));
            }
        }
    }
}

void Game::update(float dt) {
    ImGui::SFML::Update(m_window, sf::seconds(dt));

    if (m_state == GameState::MainMenu) {
        updateVideo(dt);

        // Coming Soon 动画帧推进（15fps，循环播放）
        if (m_showComingSoon && m_comingLoaded) {
            m_comingTimer += dt;
            if (m_comingTimer >= 1.0f / 15.0f) {
                m_comingTimer = 0.0f;
                m_comingFrame = (m_comingFrame + 1) % (int)m_comingFrames.size();
            }
        }

        renderMainUI();
        renderTopLeftAnimation();
        if (m_showSettingsPanel) renderSettingsPanel();
        if (m_showComingSoon)    renderComingSoonPopup();
        if (m_showCharSelect)    renderCharSelectPopup();

    }
    else if (m_state == GameState::Gameplay) {
        updateGameplay(dt);
    }

    // 全屏切换延迟到状态更新末尾执行，避免在事件处理/绘制中重建窗口造成显示异常
    if (m_switchFullscreenPending) {
        m_switchFullscreenPending = false;
        switchFullscreen();
    }
}

void Game::loadVideoFrames() {
    m_videoFrames.clear();
    for (int i = 0; i < 62; i++) {
        char path[256];
        std::snprintf(path, sizeof(path), "assets/frames/setting_frame/1_ezgif-frame-%03d.png", i);
        sf::Texture tex;
        if (tex.loadFromFile(path)) {
            m_videoFrames.push_back(tex);
        }
    }

    if (!m_videoFrames.empty()) {
        m_videoLoaded = true;
        m_videoSprite.emplace(m_videoFrames[0]);
    }
}

// ======================
// 加载 Coming Soon 动画帧
// ======================
void Game::loadComingFrames() {
    m_comingFrames.clear();
    for (int i = 1; i <= 61; i++) {
        char path[256];
        std::snprintf(path, sizeof(path),
            "assets/frames/coming_frame/1_ezgif-frame-%03d.png", i);
        sf::Texture tex;
        if (tex.loadFromFile(path)) {
            m_comingFrames.push_back(std::move(tex));
        }
    }
    m_comingLoaded = !m_comingFrames.empty();
}

// ======================
// Coming Soon 弹窗（Character Select / Map Select 共用）
// ======================
void Game::renderComingSoonPopup() {
    if (!m_showComingSoon) return;

    float s = m_uiScale;

    // 全屏半透明遮罩（可点击，点击空白处关闭）
    const float winW = (float)m_window.getSize().x;
    const float winH = (float)m_window.getSize().y;
    ImGui::SetNextWindowPos({ 0, 0 });
    ImGui::SetNextWindowSize({ winW, winH });
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.08f, 0.06f, 0.60f));
    ImGui::Begin("##overlay", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBringToFrontOnFocus
    );
    // 点击遮罩（弹窗外空白处）关闭
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        m_showComingSoon = false;
        m_comingFrame    = 0;
        m_comingTimer    = 0.0f;
    }
    ImGui::End();
    ImGui::PopStyleColor();

    // 弹窗主体：居中，基于窗口实际尺寸自适应
    // 弹窗占屏幕 50% 宽、65% 高，上限 480x480
    const float popW = std::min(winW * 0.50f, 480.0f * s);
    const float popH = std::min(winH * 0.65f, 480.0f * s);
    const float popX = (winW - popW) * 0.5f;
    const float popY = (winH - popH) * 0.5f;

    ImGui::SetNextWindowPos({ popX, popY });
    ImGui::SetNextWindowSize({ popW, popH });

    // 弹窗背景：宣纸白，微透明
    ImGui::PushStyleColor(ImGuiCol_WindowBg,   ImVec4(0.96f, 0.93f, 0.87f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border,     ImVec4(0.72f, 0.18f, 0.21f, 0.60f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("##comingSoon", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar
    );

    // ── 动画区（弹窗内水平+垂直居中） ──
    if (m_comingLoaded && !m_comingFrames.empty()) {
        const float padding = 20 * s;
        const float availW = popW - padding * 2;
        const float availH = popH - padding * 2;
        const float imgSize = std::min(availW, availH);

        const float imgCenterX = popW * 0.5f;
        const float imgCenterY = popH * 0.30f;
        ImGui::SetCursorPos({ imgCenterX - imgSize * 0.5f, imgCenterY - imgSize * 0.5f });
        ImGui::Image(m_comingFrames[m_comingFrame], { imgSize, imgSize });
    }

    // ── 动画下方文字 ──
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.18f, 0.21f, 1.0f)); // 朱砂红
    const char* title = "正在努力研发中......";
    float tx = (popW - ImGui::CalcTextSize(title).x) * 0.5f;
    ImGui::SetCursorPosX(tx);
    ImGui::Text("%s", title);
    ImGui::PopStyleColor();

    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

// ======================
// 角色选择弹窗（类似 Coming Soon 风格，点击空白处关闭）
// ======================
// ======================
// 角色选择弹窗（主菜单内调用）
// ======================
void Game::renderCharSelectPopup() {
    if (!m_showCharSelect) return;

    // 角色选择页面用平方根缓和缩放，全屏字不会太大
    float s = std::sqrt(m_uiScale);
    const float winW = (float)m_window.getSize().x;
    const float winH = (float)m_window.getSize().y;

    // ── 全屏遮罩（只拦截弹窗外的点击来关闭） ──
    float popW = std::min(winW * 0.72f, 820.0f * s);
    float popH = std::min(winH * 0.78f, 560.0f * s);
    float popX = (winW - popW) * 0.5f;
    float popY = (winH - popH) * 0.5f;

    ImGui::SetNextWindowPos({ 0, 0 });
    ImGui::SetNextWindowSize({ winW, winH });
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.08f, 0.06f, 0.55f));
    ImGui::Begin("##csOverlay", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoInputs
    );
    ImGui::End();
    ImGui::PopStyleColor();

    // 检测点击弹窗外区域 → 关闭
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        ImVec2 mouse = ImGui::GetMousePos();
        if (mouse.x < popX || mouse.x > popX + popW ||
            mouse.y < popY || mouse.y > popY + popH) {
            m_showCharSelect = false;
            return;
        }
    }

    // ── 弹窗主体（单窗口，内含两个 Child 卡片） ──
    ImGui::SetNextWindowPos({ popX, popY });
    ImGui::SetNextWindowSize({ popW, popH });
    ImGui::PushStyleColor(ImGuiCol_WindowBg,   ImVec4(0.96f, 0.93f, 0.87f, 0.97f));
    ImGui::PushStyleColor(ImGuiCol_Border,     ImVec4(0.72f, 0.18f, 0.21f, 0.55f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20 * s, 16 * s));

    ImGui::Begin("##charSelectPop", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar
    );

    // 标题
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.18f, 0.21f, 1.0f));
    ImGui::SetWindowFontScale(1.15f * s);
    float ttl = ImGui::CalcTextSize("选择你的角色").x;
    ImGui::SetCursorPosX((popW - ttl) * 0.5f);
    ImGui::Text("选择你的角色");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::Dummy({ 0, 12 * s });

    // 角色数据（优化版：属性分组 + 图标 + 类型标签 + 简介文案）
    struct CharStat { const char* icon; const char* label; const char* value; };
    struct CharInfo {
        const char* name;
        const char* faculty;
        const char* typeTag;      // 如 "快攻型"
        const char* subTag;       // 如 "科技爆发"
        ImVec4      tagColor;     // 标签渐变色
        CharStat    stats[5];
        const char* quote;        // 角色简介
        ImVec4      color;
    };
    CharInfo chars[2] = {
        {
            "玄码执律", "计算机科学与工程学院",
            "快攻型", "科技爆发",
            ImVec4(0.18f, 0.48f, 0.92f, 1.0f),  // 科技蓝
            {
                { "", "HP",      "80"    },
                { "", "攻击",    "15"    },
                { "", "暴击",    "20%"   },
                { "", "速度",    "200"   },
            },
            "\"代码如律，迅如闪电。\"",
            ImVec4(0.15f, 0.35f, 0.80f, 1.0f),
        },
        {
            "丹烬凝霜", "化学与化工学院",
            "坦克型", "化学爆破",
            ImVec4(0.35f, 0.18f, 0.72f, 1.0f),  // 化学（冰火红蓝混合）
            {
                { "", "HP",      "140"   },
                { "", "攻击",    "10"    },
                { "", "防御",    "10"    },
                { "", "速度",    "140"   },
            },
            "\"冻结与爆炸，皆在调配之间。\"",
            ImVec4(0.12f, 0.55f, 0.28f, 1.0f),
        }
    };

    // 两个卡片（Child 并排）
    float innerW = popW - 40 * s;
    float cardW  = (innerW - 16 * s) * 0.5f;
    float cardH  = popH - 130 * s;

    for (int i = 0; i < 2; i++) {
        if (i > 0) ImGui::SameLine(0, 16 * s);

        bool selected = (m_selectedCharIdx == i);

        // 选中：朱砂红粗边框
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.85f, 0.22f, 0.25f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 3.0f);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(chars[i].color.x, chars[i].color.y, chars[i].color.z, 0.55f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
        }

        ImGui::BeginChild(("##cc" + std::to_string(i)).c_str(), { cardW, cardH }, true,
            ImGuiWindowFlags_NoScrollbar);

        float blkW = cardW - 24 * s;

        // ── 头像区 ──
        float blkH = 100 * s;
        if (i == 0 && m_csAvatarLoaded) {
            ImGui::BeginChild(("##av" + std::to_string(i)).c_str(), { blkW, blkH }, false);
            ImVec2 screenPos = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            float texW = (float)m_csAvatarTexture.getSize().x;
            float texH = (float)m_csAvatarTexture.getSize().y;
            float ascale = std::min(blkW / texW, blkH / texH) * 1.15f;
            float drawW = texW * ascale;
            float drawH = texH * ascale;
            float drawX = screenPos.x + (blkW - drawW) * 0.5f;
            float drawY = screenPos.y + (blkH - drawH) * 0.5f;
            dl->AddImage((ImTextureID)(intptr_t)m_csAvatarTexture.getNativeHandle(),
                { drawX, drawY }, { drawX + drawW, drawY + drawH });
            ImGui::EndChild();
        } else if (i == 1 && m_chemAvatarLoaded) {
            ImGui::BeginChild(("##av" + std::to_string(i)).c_str(), { blkW, blkH }, false);
            ImVec2 screenPos = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            float texW = (float)m_chemAvatarTexture.getSize().x;
            float texH = (float)m_chemAvatarTexture.getSize().y;
            float ascale = std::min(blkW / texW, blkH / texH) * 1.15f;
            float drawW = texW * ascale;
            float drawH = texH * ascale;
            float drawX = screenPos.x + (blkW - drawW) * 0.5f;
            float drawY = screenPos.y + (blkH - drawH) * 0.5f;
            dl->AddImage((ImTextureID)(intptr_t)m_chemAvatarTexture.getNativeHandle(),
                { drawX, drawY }, { drawX + drawW, drawY + drawH });
            ImGui::EndChild();
        } else {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, chars[i].color);
            ImGui::BeginChild(("##av" + std::to_string(i)).c_str(), { blkW, blkH }, false);
            ImGui::SetWindowFontScale(2.6f * s);
            const char* bigChar = (i == 0) ? "CS" : "化";
            float tw2 = ImGui::CalcTextSize(bigChar).x;
            float th2 = ImGui::CalcTextSize(bigChar).y;
            ImGui::SetCursorPos({ (blkW - tw2) * 0.5f, (blkH - th2) * 0.5f });
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 0.9f));
            ImGui::Text("%s", bigChar);
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(1.0f);
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        ImGui::Dummy({ 0, 6 * s });

        // ── 学院名（小字灰色） ──
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.48f, 0.45f, 1.0f));
        ImGui::SetWindowFontScale(0.72f * s);
        float facW = ImGui::CalcTextSize(chars[i].faculty).x;
        ImGui::SetCursorPosX((cardW - facW) * 0.5f);
        ImGui::Text("%s", chars[i].faculty);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        // ── 角色名（大字，学院色） ──
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(
            chars[i].color.x * 0.5f, chars[i].color.y * 0.5f, chars[i].color.z * 0.5f, 1.0f));
        ImGui::SetWindowFontScale(1.25f * s);
        float nameW = ImGui::CalcTextSize(chars[i].name).x;
        ImGui::SetCursorPosX((cardW - nameW) * 0.5f);
        ImGui::Text("%s", chars[i].name);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::Dummy({ 0, 4 * s });

        // ── 类型标签（药丸样式，两个并排） ──
        ImGui::SetWindowFontScale(0.78f * s);
        float tagTextW1 = ImGui::CalcTextSize(chars[i].typeTag).x;
        float tagTextW2 = ImGui::CalcTextSize(chars[i].subTag).x;
        float tagPadH = 6 * s, tagPadV = 3 * s;
        float tagW1 = tagTextW1 + tagPadH * 2;
        float tagW2 = tagTextW2 + tagPadH * 2;
        float tagH  = ImGui::CalcTextSize(chars[i].typeTag).y + tagPadV * 2;
        float tagsTotalW = tagW1 + tagW2 + 8 * s;
        float tagsStartX = (cardW - tagsTotalW) * 0.5f;

        ImGui::SetCursorPosX(tagsStartX);
        // 类型标签（第一个）
        ImVec4 tc = chars[i].tagColor;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(tc.x, tc.y, tc.z, 0.18f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(tc.x, tc.y, tc.z, 0.28f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(tc.x, tc.y, tc.z, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(tc.x, tc.y, tc.z, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, tagH * 0.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(tagPadH, tagPadV));
        ImGui::Button(chars[i].typeTag, { tagW1, tagH });
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);

        ImGui::SameLine(0, 8 * s);

        // 子标签（第二个）
        ImVec4 tc2 = chars[i].color;
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(tc2.x, tc2.y, tc2.z, 0.12f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(tc2.x, tc2.y, tc2.z, 0.22f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(tc2.x, tc2.y, tc2.z, 0.30f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(tc2.x, tc2.y, tc2.z, 0.85f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, tagH * 0.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(tagPadH, tagPadV));
        ImGui::Button(chars[i].subTag, { tagW2, tagH });
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);

        ImGui::SetWindowFontScale(1.0f);

        ImGui::Dummy({ 0, 6 * s });

        // ── 分隔线 ──
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(chars[i].color.x, chars[i].color.y, chars[i].color.z, 0.25f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Dummy({ 0, 4 * s });

        // ── 属性列表（左对齐标签，右对齐数值，分两组） ──
        ImGui::SetWindowFontScale(0.82f * s);
        ImVec4 labelColor = ImVec4(0.50f, 0.48f, 0.45f, 1.0f);   // 灰色标签
        ImVec4 valueColor = ImVec4(0.20f, 0.20f, 0.26f, 1.0f);   // 墨黛色数值
        ImVec4 accentColor = chars[i].tagColor;                      // 关键数值强调色

        // 战斗核心组：HP、攻击、暴击
        for (int j = 0; j < 3; j++) {
            const char* lbl = chars[i].stats[j].label;
            const char* val = chars[i].stats[j].value;

            // 用标签颜色画小文字
            ImGui::PushStyleColor(ImGuiCol_Text, labelColor);
            ImGui::Text("  %s", lbl);
            ImGui::PopStyleColor();

            // 在同一行右侧画数值
            float valWidth = ImGui::CalcTextSize(val).x;
            ImGui::SameLine(blkW - valWidth + 2 * s);
            // HP 用强调色
            if (j == 0) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.25f, 0.20f, 1.0f));
                ImGui::SetWindowFontScale(0.95f * s);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, valueColor);
            }
            ImGui::Text("%s", val);
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(0.82f * s);
        }

        ImGui::Dummy({ 0, 3 * s });

        // 行动相关组：速度（第4个属性，索引3）
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(chars[i].color.x, chars[i].color.y, chars[i].color.z, 0.15f));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Dummy({ 0, 3 * s });

        for (int j = 3; j < 4; j++) {
            const char* lbl = chars[i].stats[j].label;
            const char* val = chars[i].stats[j].value;

            ImGui::PushStyleColor(ImGuiCol_Text, labelColor);
            ImGui::Text("  %s", lbl);
            ImGui::PopStyleColor();

            float valWidth = ImGui::CalcTextSize(val).x;
            ImGui::SameLine(blkW - valWidth + 2 * s);
            ImGui::PushStyleColor(ImGuiCol_Text, valueColor);
            ImGui::Text("%s", val);
            ImGui::PopStyleColor();
        }

        ImGui::SetWindowFontScale(1.0f);

        ImGui::Dummy({ 0, 6 * s });

        // ── 角色简介 ──
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(chars[i].color.x * 0.7f, chars[i].color.y * 0.7f, chars[i].color.z * 0.7f, 0.75f));
        ImGui::SetWindowFontScale(0.75f * s);
        float quoteW = ImGui::CalcTextSize(chars[i].quote).x;
        ImGui::SetCursorPosX((cardW - quoteW) * 0.5f);
        ImGui::Text("%s", chars[i].quote);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleColor();

        ImGui::Dummy({ 0, 8 * s });

        // ── 选择按钮 ──
        float sbW = blkW;
        float sbH = 36 * s;
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.72f, 0.18f, 0.21f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.82f, 0.22f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.60f, 0.14f, 0.18f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, chars[i].color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(
                std::min(chars[i].color.x + 0.15f, 1.0f),
                std::min(chars[i].color.y + 0.15f, 1.0f),
                std::min(chars[i].color.z + 0.15f, 1.0f), 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.60f, 0.14f, 0.18f, 1.0f));
        }
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));

        const char* label = selected ? "> 已选择 <" : "选择此角色";
        if (ImGui::Button(label, { sbW, sbH })) {
            m_selectedCharIdx = i;
        }
        ImGui::PopStyleColor(4);

        ImGui::EndChild(); // ##cc
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

    // ── 底部操作栏 ──
    ImGui::Dummy({ 0, 10 * s });
    float barBtnW = 160 * s;
    float barBtnH = 42 * s;
    float barTotalW = barBtnW * 2 + 16 * s;
    float barStartX = (popW - barTotalW) * 0.5f;
    ImGui::SetCursorPosX(barStartX);

    // 返回
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.75f, 0.72f, 0.68f, 0.55f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.60f, 0.56f, 0.75f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.55f, 0.50f, 0.48f, 0.90f));
    if (ImGui::Button("返回", { barBtnW, barBtnH })) {
        m_showCharSelect = false;
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine(0, 16 * s);

    // 开始
    bool canStart = (m_selectedCharIdx >= 0);
    if (!canStart) {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.60f, 0.58f, 0.55f, 0.40f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.60f, 0.58f, 0.55f, 0.40f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.60f, 0.58f, 0.55f, 0.40f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.6f, 0.6f, 0.6f, 0.6f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.72f, 0.18f, 0.21f, 0.88f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.82f, 0.22f, 0.25f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.60f, 0.14f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1, 1, 1, 1));
    }
    const char* goLabel = canStart ? "开始冒险" : "请先选择角色";
    if (ImGui::Button(goLabel, { barBtnW, barBtnH }) && canStart) {
        m_showCharSelect = false;
        enterGameplay();
    }
    ImGui::PopStyleColor(4);

    ImGui::End(); // ##charSelectPop
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

void Game::updateVideo(float dt) {
    if (!m_videoLoaded || !m_videoSprite.has_value())
        return;

    m_videoTimer += dt;
    if (m_videoTimer >= 1.0f / 15.0f) {
        m_videoTimer = 0.0f;
        m_currentFrame = (m_currentFrame + 1) % m_videoFrames.size();
        m_videoSprite->setTexture(m_videoFrames[m_currentFrame]);
    }
}

// ======================
// 左上角常驻动画（点击打开设置）
// ======================
void Game::renderTopLeftAnimation() {
    float s = m_uiScale;
    // 左上角固定位置（小巧不遮挡背景）
    ImGui::SetNextWindowPos({ 25 * s, 25 * s });
    ImGui::SetNextWindowSize({ 120 * s, 120 * s });
    ImGui::Begin("AnimationWidget", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBackground
    );

    // ✅ 核心修复：移除边框、圆角、背景、内边距
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));    // 清除内边距
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);            // 清除边框
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);              // 清除圆角（关键！）
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));           // 透明背景
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));    // 悬浮透明
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));     // 点击透明

    // 纯动画按钮，无任何样式
        if (m_videoLoaded && !m_videoFrames.empty()) {
        // 120框 - 110图 = 10px间隙，左右各5px
        ImGui::SetCursorPos({ 5 * s, 5 * s });

        if (ImGui::ImageButton("##SettingsAnim", m_videoFrames[m_currentFrame], { 110 * s, 110 * s })) {
            m_showSettingsPanel = !m_showSettingsPanel;
        }
    }
    else {
        const char* txt = "SET";
        float x = (ImGui::GetWindowSize().x - ImGui::CalcTextSize(txt).x) * 0.5f;
        ImGui::SetCursorPosX(x);
        ImGui::Text("%s", txt);
    }

    // 还原样式，不影响其他UI
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);

    ImGui::End();
}

// ======================
// 主菜单UI（根据背景图布局：放左侧空白区域，避开右下角牡丹花）
// ======================
void Game::renderMainUI() {
    float s = m_uiScale;

    // 背景图分析：
    // - 顶部有标题，避开 y<100
    // - 左下角有古建筑，避开 x<350, y>450
    // - 右下角有牡丹花，避开 x>600, y>350
    // - 最佳区域：左侧中间偏上 x=50~350, y=150~450

    const float uiX = 80 * s;      // 左对齐，留边距
    const float uiY = 180 * s;     // 标题下方
    const float uiW = 320 * s;     // 收窄避免碰到牡丹
    const float uiH = 420 * s;     // 高度适中

    ImGui::SetNextWindowPos({ uiX, uiY });
    ImGui::SetNextWindowSize({ uiW, uiH });
    ImGui::Begin("MainMenu", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBackground  // 透明背景，融入古风图
    );

    // 按钮尺寸更大，间距更宽松
    const float btnW = 280 * s;
    const float btnH = 64 * s;
    const float btnX = (uiW - btnW) / 2;
    const float btnSpacing = 16 * s;  // 按钮间距增大到16px

    ImGui::SetCursorPosX(btnX);
    ImGui::SetCursorPosY(20 * s);
    if (ImGui::Button("开始游戏", { btnW, btnH })) {
        // 默认 CS 学生，直接进入游戏
        if (m_selectedCharIdx < 0) m_selectedCharIdx = 0;
        enterGameplay();
    }
    ImGui::Dummy({ 0, btnSpacing });

    ImGui::SetCursorPosX(btnX);
    if (ImGui::Button("角色", { btnW, btnH })) {
        m_showCharSelect = true;
    }
    ImGui::Dummy({ 0, btnSpacing });

    ImGui::SetCursorPosX(btnX);
    if (ImGui::Button("地图", { btnW, btnH })) {
        m_showComingSoon = true;
        m_comingFrame    = 0;
        m_comingTimer    = 0.0f;
    }
    ImGui::Dummy({ 0, btnSpacing });

    ImGui::SetCursorPosX(btnX);
    if (ImGui::Button("退出游戏", { btnW, btnH })) {
        m_running = false;
    }

    ImGui::End();
}

// ======================
// 设置面板（音量调节）
// ======================
void Game::renderSettingsPanel() {
    float s = m_uiScale;
    const float winW = (float)m_window.getSize().x;
    const float winH = (float)m_window.getSize().y;

    // 设置面板：放在右上角，动态计算位置
    const float panelW = 320 * s;
    const float panelH = std::min(400 * s, winH * 0.7f);
    const float panelX = winW - panelW - 40 * s;  // 右侧40px边距
    const float panelY = 25 * s;

    ImGui::SetNextWindowPos({ panelX, panelY });
    ImGui::SetNextWindowSize({ panelW, panelH });
    ImGui::Begin("SettingsPanel", &m_showSettingsPanel,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar
    );


    // 音量调节（0%~100%）
    const float centerX = 160.0f * s; // 面板中心 x

    // Master Volume
    ImGui::SetCursorPosX(centerX - ImGui::CalcTextSize("Master Volume").x / 2.0f);
    ImGui::Text("主音量");
    ImGui::SetNextItemWidth(260 * s);
    ImGui::SetCursorPosX(30 * s);
    if (ImGui::SliderInt("##master", &m_masterPct, 0, 100, "%d%%")) {
        m_masterVolume = m_masterPct / 100.0f;
        updateBGMVolume();
    }

    ImGui::Dummy({ 0, 6 * s });

    // BGM Volume
    ImGui::SetCursorPosX(centerX - ImGui::CalcTextSize("BGM Volume").x / 2.0f);
    ImGui::Text("背景音乐");
    ImGui::SetNextItemWidth(260 * s);
    ImGui::SetCursorPosX(30 * s);
    if (ImGui::SliderInt("##bgm", &m_bgmPct, 0, 100, "%d%%")) {
        m_bgmVolume = m_bgmPct / 100.0f;
        updateBGMVolume();
    }

    ImGui::Dummy({ 0, 6 * s });

    // SFX Volume
    ImGui::SetCursorPosX(centerX - ImGui::CalcTextSize("SFX Volume").x / 2.0f);
    ImGui::Text("游戏音效");
    ImGui::SetNextItemWidth(260 * s);
    ImGui::SetCursorPosX(30 * s);
    if (ImGui::SliderInt("##sfx", &m_sfxPct, 0, 100, "%d%%"))
        m_sfxVolume = m_sfxPct / 100.0f;

    ImGui::Dummy({ 0, 10 * s });

    // ── 分隔线（朱砂色淡线） ──
    {
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.72f, 0.18f, 0.21f, 0.30f));
        ImGui::Separator();
        ImGui::PopStyleColor();
    }

    ImGui::Dummy({ 0, 12 * s });

    // ── Fullscreen 切换按钮（与面板同宽，风格统一） ──
    {
        const float fsBtnW = 260 * s;
        const float fsBtnH = 50 * s;
        ImGui::SetCursorPosX(centerX - fsBtnW / 2.0f);

        // 根据当前状态切换颜色
        if (m_isFullscreen) {
            // 已全屏 → 激活态：朱砂红底
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.80f, 0.24f, 0.38f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.20f, 0.32f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.60f, 0.16f, 0.28f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        } else {
            // 未全屏 → 默认态：象牙白底
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.98f, 0.96f, 0.92f, 0.65f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.82f, 0.55f, 0.68f, 0.80f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.80f, 0.24f, 0.38f, 0.90f));
            ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.20f, 0.20f, 0.26f, 1.0f));
        }

        const char* fsLabel = m_isFullscreen ? "[ ON ]  全屏" : "全屏";
        if (ImGui::Button(fsLabel, { fsBtnW, fsBtnH })) {
            m_switchFullscreenPending = true;
        }

        ImGui::PopStyleColor(4);
    }

    ImGui::Dummy({ 0, 14 * s });

    // Close 按钮
    {
        const float closeW = 250 * s;
        const float closeH = 55 * s;
        ImGui::SetCursorPosX(centerX - closeW / 2.0f);

        // Close 用更沉稳的灰色调，区别于全屏按钮
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.75f, 0.72f, 0.68f, 0.50f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.60f, 0.56f, 0.70f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.55f, 0.50f, 0.48f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.20f, 0.20f, 0.26f, 1.0f));

        if (ImGui::Button("关闭", { closeW, closeH })) {
            m_showSettingsPanel = false;
        }

        ImGui::PopStyleColor(4);
    }

    ImGui::End();
}

void Game::switchFullscreen() {
    ImGui::SFML::Shutdown();

    m_isFullscreen = !m_isFullscreen;
    if (m_isFullscreen) {
        auto desktop = sf::VideoMode::getDesktopMode();
        m_window.create(
            desktop,
            "SCUT WALK TALL",
            sf::Style::None,
            sf::State::Fullscreen
        );
    }
    else {
        m_window.create(
            sf::VideoMode({ DesignWidth, DesignHeight }),
            "SCUT WALK TALL",
            sf::Style::Titlebar | sf::Style::Close
        );
    }

    updateUIScale();

    // 重启时钟，防止窗口重建期间的累积时间导致第一帧 dt 过大
    m_clock.restart();

    // 如果在 Gameplay 中切换全屏，同步更新摄像机尺寸
    if (m_state == GameState::Gameplay) {
        m_camera.setSize(static_cast<sf::Vector2f>(m_window.getSize()));
    }
}

// ======================
// Gameplay 场景
// ======================
void Game::enterGameplay() {
    m_state = GameState::Gameplay;
    m_isDead    = false;
    m_killCount = 0;
    playBGM(BGMType::Gameplay);
    m_gameTime  = 0.0f;

    // ── 实例化选择的角色 ──
    if (m_selectedCharIdx == 0) {
        m_activePlayer = std::make_unique<CSPlayer>();
    } else if (m_selectedCharIdx == 1) {
        m_activePlayer = std::make_unique<ChemPlayer>();
    } else {
        // 未选择则默认 CS 学生
        m_activePlayer = std::make_unique<CSPlayer>();
    }

    // 从角色获取属性
    m_playerSpeed    = m_activePlayer->getSpeed();

    // 加载地图（固定 map_01.tmx）
    m_gameMap = std::make_unique<Map>();
    if (!m_gameMap->loadFromTiled("assets/maps/map_01.tmx")) {
        // 加载失败，退回角色选择
        m_state = GameState::CharSelect;
        return;
    }

    // 角色初始化：放在地图出生点
    m_playerPos = m_gameMap->getSpawnPoint();
    float renderScale = static_cast<float>(TILE_RENDER_SIZE) / m_gameMap->getTileWidth();
    m_playerPos.x *= renderScale;
    m_playerPos.y *= renderScale;

    // 角色色块：用角色代表色
    sf::Color charColor = m_activePlayer->getColor();
    sf::Color outlineColor(
        (int)(charColor.r * 0.4f),
        (int)(charColor.g * 0.4f),
        (int)(charColor.b * 0.4f)
    );
    m_playerShape.setSize({ static_cast<float>(TILE_RENDER_SIZE), static_cast<float>(TILE_RENDER_SIZE) });
    m_playerShape.setFillColor(charColor);
    m_playerShape.setOutlineColor(outlineColor);
    m_playerShape.setOutlineThickness(2.0f);

    // ── 角色精灵图加载 ──
    std::string charPrefix = (m_selectedCharIdx == 0) ? "cs" : "chem";
    m_charDown.loadFromFile("assets/sprites/characters/" + charPrefix + "_walk_down.png");
    m_charDown.setSmooth(true);
    m_charUp.loadFromFile("assets/sprites/characters/" + charPrefix + "_walk_up.png");
    m_charUp.setSmooth(true);
    m_charLeft.loadFromFile("assets/sprites/characters/" + charPrefix + "_walk_left.png");
    m_charLeft.setSmooth(true);
    // 侧面素材常有大量半透明边缘像素导致可见线条，裁剪掉低透明度像素
    {
        sf::Image img = m_charLeft.copyToImage();
        for (unsigned int y = 0; y < img.getSize().y; ++y)
            for (unsigned int x = 0; x < img.getSize().x; ++x) {
                sf::Color c = img.getPixel({x, y});
                if (c.a > 0 && c.a < 100) c.a = 0;   // 半透明→完全透明
                img.setPixel({x, y}, c);
            }
        m_charLeft.loadFromImage(img);
    }
    // 右向图可选，没有就用左向图镜像
    m_playerSpriteLoaded = m_charDown.getSize().x > 0;
    // 尝试加载右向图，失败则标记
    bool hasRight = m_charRight.loadFromFile("assets/sprites/characters/" + charPrefix + "_walk_right.png");
    if (hasRight) {
        m_charRight.setSmooth(true);
        // 同样裁剪侧面素材的半透明边缘
        sf::Image img = m_charRight.copyToImage();
        for (unsigned int y = 0; y < img.getSize().y; ++y)
            for (unsigned int x = 0; x < img.getSize().x; ++x) {
                sf::Color c = img.getPixel({x, y});
                if (c.a > 0 && c.a < 100) c.a = 0;
                img.setPixel({x, y}, c);
            }
        m_charRight.loadFromImage(img);
    }
    if (!hasRight) m_charRight = m_charLeft; // fallback 用左向图

    if (m_playerSpriteLoaded) {
        m_playerSprite.emplace(m_charDown);

        // ── 碰撞箱：取角色全部非透明像素的精确包围盒 ──
        sf::Image img = m_charDown.copyToImage();
        unsigned int w = img.getSize().x, h = img.getSize().y;

        int minX = (int)w, minY = (int)h, maxX = -1, maxY = -1;
        for (unsigned int y = 0; y < h; ++y) {
            for (unsigned int x = 0; x < w; ++x) {
                if (img.getPixel({x, y}).a > 30) {
                    if ((int)x < minX) minX = (int)x;
                    if ((int)y < minY) minY = (int)y;
                    if ((int)x > maxX) maxX = (int)x;
                    if ((int)y > maxY) maxY = (int)y;
                }
            }
        }

        if (maxX >= 0 && maxY >= 0) {
            float charPixelHeight = (float)(maxY - minY + 1);

            // 目标渲染高度固定为 202px（与分辨率无关，可调）
            const float targetRenderHeight = 202.0f;
            float scale = targetRenderHeight / charPixelHeight;

            m_charSpriteScale = scale;
            m_playerSprite->setScale({ scale, scale });

            // 精灵渲染尺寸
            float spriteW = w * scale;
            float spriteH = h * scale;

            // 碰撞箱 = 精灵图的非透明区域包围盒（相对于精灵左上角）
            m_charCollisionBox = sf::FloatRect(
                sf::Vector2f(minX * scale, minY * scale),
                sf::Vector2f((maxX - minX + 1) * scale, (maxY - minY + 1) * scale)
            );

            m_spriteOffsetY = -(spriteH - (float)TILE_RENDER_SIZE) + 75.f;
            m_spriteOffsetX = -(spriteW - (float)TILE_RENDER_SIZE) * 0.5f;
        }
    }

    // 初始化摄像机
    m_camera.setSize(static_cast<sf::Vector2f>(m_window.getSize()));
    m_camera.setCenter({ m_playerPos.x + TILE_RENDER_SIZE * 0.5f,
                         m_playerPos.y + TILE_RENDER_SIZE * 0.5f });

    m_facingDir = { 0.0f, 1.0f };
    m_pickups.clear();

    // ── 道具系统初始化 ──
    m_healItemLoaded  = m_healItemTexture.loadFromFile("assets/sprites/items/item_heal.png");
    m_healItemTexture.setSmooth(true);
    m_speedItemLoaded = m_speedItemTexture.loadFromFile("assets/sprites/items/item_speed.png");
    m_speedItemTexture.setSmooth(true);
    m_expItemLoaded   = m_expItemTexture.loadFromFile("assets/sprites/items/item_exp.png");
    m_expItemTexture.setSmooth(true);
    m_pickupSpawnTimer = 0.0f;
    m_nextSpawnInterval = 7.5f + std::rand() % 750 / 100.0f; // 7.5 ~ 15.0
    m_speedBuffTimer = 0.0f;
    m_expBuffTimer = 0.0f;
    m_enemySlowTimer = 0.0f;
    m_gameTime        = 0.0f;
    m_enemySpawnTimer = 0.0f;
    m_enemySpawnInterval = 8.0f; // 初始8s刷一批

    // ── 初始化敌人 ──
    initEnemies();

    // ── 初始化技能 ──
    initSkills();

    // HUD 数值平滑初始同步
    if (m_activePlayer && m_activePlayer->getMaxHp() > 0) {
        m_hpBarSmooth = static_cast<float>(m_activePlayer->getHp()) /
            static_cast<float>(m_activePlayer->getMaxHp());
    } else {
        m_hpBarSmooth = 1.0f;
    }
    m_expBarSmooth = (m_activePlayer && m_activePlayer->getExpToNext() > 0)
        ? static_cast<float>(m_activePlayer->getExp()) / static_cast<float>(m_activePlayer->getExpToNext())
        : 0.0f;
    m_gameplayElapsed = 0.0f;
}

void Game::updateGameplay(float dt) {
    if (!m_gameMap) return;

    // ── 升级选择 UI 暂停游戏 ──
    if (m_showLevelUp) return;

    m_gameplayElapsed += dt;

    if (m_isDead) return;

    // 防止 dt 过大（窗口拖动/失焦后回来）导致角色瞬移
    dt = std::min(dt, 0.05f);

    // ── 蓝屏僵直计时器 ──
    if (m_blueStunTimer > 0.f) {
        m_blueStunTimer -= dt;
        if (m_blueStunTimer < 0.f) m_blueStunTimer = 0.f;
    }

    // WASD 移动（僵直时跳过输入）
    sf::Vector2f movement(0, 0);
    if (m_blueStunTimer <= 0.f) {
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))
        movement.y -= 1;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))
        movement.y += 1;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
        movement.x -= 1;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D) ||
        sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
        movement.x += 1;
    }  // end stun check

    // 记录面朝方向（最后一次按的方向键）
    if (movement.x != 0 || movement.y != 0) {
        m_facingDir = movement; // movement 在归一化前记录方向，后面归一化只用于移动
    }

    // 对角线归一化
    if (movement.x != 0 && movement.y != 0) {
        movement.x *= 0.7071f;
        movement.y *= 0.7071f;
    }

    sf::Vector2f newPos = m_playerPos + movement * m_playerSpeed * dt;

    // 碰撞检测：用精灵完整包围盒（非透明像素区域）
    float renderScale = static_cast<float>(TILE_RENDER_SIZE) / m_gameMap->getTileWidth();

    auto checkCollision = [&](sf::Vector2f pos) -> bool {
        float rs = static_cast<float>(TILE_RENDER_SIZE) / m_gameMap->getTileWidth();
        // 碰撞箱在精灵坐标中的位置，转换为世界坐标
        float cbX = (pos.x + m_spriteOffsetX + m_charCollisionBox.position.x) / rs;
        float cbY = (pos.y + m_spriteOffsetY + m_charCollisionBox.position.y) / rs;
        float cbW = m_charCollisionBox.size.x / rs;
        float cbH = m_charCollisionBox.size.y / rs;
        if (cbW <= 0 || cbH <= 0) {
            float ms = static_cast<float>(TILE_RENDER_SIZE) / rs;
            return m_gameMap->isRectColliding(pos.x / rs, pos.y / rs, ms, ms);
        }
        return m_gameMap->isRectColliding(cbX, cbY, cbW, cbH);
    };

    bool canMoveX = true, canMoveY = true;

    // X 方向碰撞
    if (checkCollision({ newPos.x, m_playerPos.y })) {
        canMoveX = false;
    }

    // Y 方向碰撞
    if (checkCollision({ m_playerPos.x, newPos.y })) {
        canMoveY = false;
    }

    if (canMoveX) m_playerPos.x = newPos.x;
    if (canMoveY) m_playerPos.y = newPos.y;

    // 限制角色不超出地图边界
    float mapPixelW = m_gameMap->getCols() * TILE_RENDER_SIZE;
    float mapPixelH = m_gameMap->getRows() * TILE_RENDER_SIZE;
    m_playerPos.x = std::clamp(m_playerPos.x, 0.0f, mapPixelW - TILE_RENDER_SIZE);
    m_playerPos.y = std::clamp(m_playerPos.y, 0.0f, mapPixelH - TILE_RENDER_SIZE);

    // ── 游戏时间累积 + 难度系数 ──
    m_gameTime += dt;
    m_difficultyFactor = 1.0f + m_gameTime / 120.0f;  // 每2分钟增强1倍

    // ── 道具系统更新 ──
    updatePickups(dt);

    // ── 敌人动态刷新（随时间加快）──
    // 刷新间隔公式：max(3s, 8s - 游戏时间/30)，即每过30秒缩短1s，最低3s
    m_enemySpawnTimer += dt;
    m_enemySpawnInterval = std::max(3.0f, 8.0f - m_gameTime / 30.0f);
    if (m_enemySpawnTimer >= m_enemySpawnInterval) {
        m_enemySpawnTimer = 0.0f;
        // 敌人像素大小（用于碰撞检测）
        const float DASH_SIZE   = 50.f;
        const float BSOD_SIZE_W = 200.f;
        const float BSOD_SIZE_H = 163.f;
        const float THIEF_SIZE  = 64.f;
        float tileW = static_cast<float>(m_gameMap->getTileWidth());
        auto toTileCheckSize = [&](float pixelSize) -> float {
            return std::ceil(pixelSize / tileW) + 1.f;
        };
        // 每次刷新1~2只（游戏时间超过60s后刷2只）
        int count = (m_gameTime > 60.f) ? 2 : 1;
        for (int i = 0; i < count; ++i) {
            // 在玩家 250~600px 范围内随机位置生成
            for (int attempt = 0; attempt < 50; ++attempt) {
                float angle = (std::rand() % 360) * 3.14159f / 180.f;
                float dist  = 250.f + std::rand() % 350;
                sf::Vector2f pos = m_playerPos + sf::Vector2f(std::cos(angle)*dist, std::sin(angle)*dist);
                pos.x = std::clamp(pos.x, 0.f, mapPixelW - 32.f);
                pos.y = std::clamp(pos.y, 0.f, mapPixelH - 32.f);
                float rs = static_cast<float>(TILE_RENDER_SIZE) / m_gameMap->getTileWidth();
                // 随机生成三种敌人，根据类型使用不同的碰撞检测范围
                int roll = std::rand() % 3;
                float checkW, checkH;
                if (roll == 0) {
                    checkW = checkH = toTileCheckSize(DASH_SIZE);
                } else if (roll == 1) {
                    checkW = toTileCheckSize(BSOD_SIZE_W);
                    checkH = toTileCheckSize(BSOD_SIZE_H);
                } else {
                    checkW = checkH = toTileCheckSize(THIEF_SIZE);
                }
                if (!m_gameMap->isRectColliding(pos.x/rs, pos.y/rs, checkW, checkH)) {
                    if (roll == 0) {
                        auto enemy = std::make_unique<DashEnemy>(
                            pos, &m_dashEnemyTexLeft, &m_dashEnemyTexRight);
                        enemy->applyDifficulty(m_difficultyFactor);
                        m_enemies.push_back(std::move(enemy));
                    } else if (roll == 1) {
                        auto enemy = std::make_unique<BlueScreenEnemy>(
                            pos, &m_bsodTexLeft, &m_bsodTexRight);
                        enemy->applyDifficulty(m_difficultyFactor);
                        m_enemies.push_back(std::move(enemy));
                    } else {
                        auto thief = std::make_unique<ThiefEnemy>(
                            pos, &m_thiefTexLeft, &m_thiefTexRight);
                        // 追踪速度 = 玩家速度 * 60%，逃跑速度 = 玩家速度 * 90%
                        float baseSpd = (m_activePlayer ? m_activePlayer->getSpeed() : 200.f);
                        float thiefChaseSpeed = baseSpd * 0.6f;
                        thief->setBaseSpeed(thiefChaseSpeed);
                        thief->setFleeSpeed(baseSpd * 0.9f);
                        thief->applyDifficulty(m_difficultyFactor);
                        m_enemies.push_back(std::move(thief));
                    }
                    break;
                }
            }
        }
    }

    // ── 敌人系统更新 ──
    updateEnemies(dt);

    // ── 技能系统更新 ──
    updateSkills(dt);

    // ── 升级检测（两个角色都有技能升级） ──
    if (m_activePlayer && m_skills.empty()) {
        m_prevPlayerLevel = m_activePlayer->getLevel();
    }
    if (m_activePlayer && !m_skills.empty()) {
        int curLevel = m_activePlayer->getLevel();
        if (curLevel > m_prevPlayerLevel && !m_showLevelUp) {
            m_prevPlayerLevel = curLevel;
            m_showLevelUp = true;
        }
    }

    // ── buff / debuff 计时器 ──
    if (m_speedBuffTimer > 0.0f) {
        m_speedBuffTimer -= dt;
        if (m_speedBuffTimer <= 0.0f) {
            m_speedBuffTimer = 0.0f;
            m_playerSpeed = m_activePlayer ? m_activePlayer->getSpeed() : 160.0f;
        }
    }
    if (m_expBuffTimer > 0.0f) {
        m_expBuffTimer -= dt;
        if (m_expBuffTimer <= 0.0f) {
            m_expBuffTimer = 0.0f;
        }
    }
    // 被敌人冲撞后减速恢复
    if (m_enemySlowTimer > 0.0f) {
        m_enemySlowTimer -= dt;
        if (m_enemySlowTimer <= 0.0f) {
            m_enemySlowTimer = 0.0f;
            // 只在没有加速 buff 时恢复正常速度
            if (m_speedBuffTimer <= 0.0f)
                m_playerSpeed = m_activePlayer ? m_activePlayer->getSpeed() : 160.0f;
        }
    }

    // ── HUD 平滑动画 ──
    if (m_activePlayer) {
        float targetHp = (m_activePlayer->getMaxHp() > 0)
            ? static_cast<float>(m_activePlayer->getHp()) / static_cast<float>(m_activePlayer->getMaxHp())
            : 0.0f;
        float targetExp = (m_activePlayer->getExpToNext() > 0)
            ? static_cast<float>(m_activePlayer->getExp()) / static_cast<float>(m_activePlayer->getExpToNext())
            : 0.0f;
        // 平滑插值，血条下降快、恢复慢
        float hpSpeed = (targetHp < m_hpBarSmooth) ? 3.5f : 1.8f;
        m_hpBarSmooth += (targetHp - m_hpBarSmooth) * std::min(1.0f, hpSpeed * dt);
        m_expBarSmooth += (targetExp - m_expBarSmooth) * std::min(1.0f, 2.5f * dt);
    }

    // ESC 返回主菜单
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape)) {
        m_state = GameState::MainMenu;
        m_gameMap.reset();
        m_pickups.clear();
        playBGM(BGMType::Menu);
        m_enemies.clear();
        m_speedBuffTimer = 0.0f;
        m_expBuffTimer = 0.0f;
        m_isDead    = false;
        m_killCount = 0;
        m_gameTime  = 0.0f;
    }
}



// ======================
// 死亡蒙版 + 统计信息
// ======================
void Game::renderDeathScreen() {
    float winW = static_cast<float>(m_window.getSize().x);
    float winH = static_cast<float>(m_window.getSize().y);

    // ── 透明黑色蒙版 ──
    ImGui::SetNextWindowPos({ 0, 0 });
    ImGui::SetNextWindowSize({ winW, winH });
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.75f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::Begin("##deathScreen", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoInputs
    );

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 screenCenter = { winW / 2.0f, winH / 2.0f };

    // ── 标题：你已阵亡 ──
    ImGui::SetWindowFontScale(2.4f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.25f, 0.25f, 1.0f));
    const char* title = "你已挂科";
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    ImGui::SetCursorPos({ (winW - titleSize.x) / 2.0f, winH * 0.28f });
    ImGui::Text("%s", title);
    ImGui::PopStyleColor();
    ImGui::SetWindowFontScale(1.0f);

    // ── 统计信息 ──
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
    ImGui::SetWindowFontScale(1.1f);

    char info[256];
    // 击杀数
    std::snprintf(info, sizeof(info), "击杀敌人：%d 只", m_killCount);
    ImVec2 infoSize = ImGui::CalcTextSize(info);
    ImGui::SetCursorPos({ (winW - infoSize.x) / 2.0f, winH * 0.42f });
    ImGui::Text("%s", info);

    // 等级
    int level = m_activePlayer ? m_activePlayer->getLevel() : 0;
    std::snprintf(info, sizeof(info), "角色等级：%d 级", level);
    infoSize = ImGui::CalcTextSize(info);
    ImGui::SetCursorPos({ (winW - infoSize.x) / 2.0f, winH * 0.48f });
    ImGui::Text("%s", info);

    // 坚持时间
    int totalSec = static_cast<int>(m_gameTime);
    int minutes = totalSec / 60;
    int seconds = totalSec % 60;
    if (minutes > 0) {
        std::snprintf(info, sizeof(info), "坚持时间：%d 分 %d 秒", minutes, seconds);
    } else {
        std::snprintf(info, sizeof(info), "坚持时间：%d 秒", seconds);
    }
    infoSize = ImGui::CalcTextSize(info);
    ImGui::SetCursorPos({ (winW - infoSize.x) / 2.0f, winH * 0.54f });
    ImGui::Text("%s", info);

    ImGui::PopStyleColor();
    ImGui::SetWindowFontScale(1.0f);

    // ── 提示 ──
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 0.8f));
    ImGui::SetWindowFontScale(0.85f);
    const char* hint = "按 ESC 返回主菜单";
    ImVec2 hintSize = ImGui::CalcTextSize(hint);
    ImGui::SetCursorPos({ (winW - hintSize.x) / 2.0f, winH * 0.65f });
    ImGui::Text("%s", hint);
    ImGui::PopStyleColor();
    ImGui::SetWindowFontScale(1.0f);

    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

// ======================
// HUD：血条 + 经验条（古风风格）
// ======================
void Game::renderHUD() {
    if (!m_activePlayer) return;

    // ── HUD 面板尺寸（宽度按屏幕比例微调，小屏不变，大屏适当加宽） ──
    float winW = static_cast<float>(m_window.getSize().x);
    // 基准 1280，超过 1280 时宽度按比例增长
    float wideScale = std::max(1.0f, winW / 1280.0f);
    const float panelW = 360.0f * wideScale;
    const float panelH = 220.0f;
    const float panelX = 20.0f;
    const float panelY = 20.0f;

    // ── 外层窗口（古风纸卷） ──
    ImGui::SetNextWindowPos({ panelX, panelY });
    ImGui::SetNextWindowSize({ panelW, panelH });

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.95f, 0.92f, 0.86f, 0.82f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.46f, 0.28f, 0.15f, 0.55f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.4f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 10.0f));

    ImGui::Begin("##hudPanel", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoInputs
    );

    ImDrawList* dl = ImGui::GetWindowDrawList();
    float innerW = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;

    // ── 标题 + 角色信息 ──
    int level = m_activePlayer->getLevel();
    const char* titleName = "SCUTER";
    const char* mottoText = "博学 慎思 明辨 笃行";
    if (m_activePlayer->getType() == PlayerType::CS) {
        titleName = "玄码执律";
        mottoText = "代码如律，迅如闪电";
    } else if (m_activePlayer->getType() == PlayerType::Chem) {
        titleName = "丹烬凝霜";
        mottoText = "冻结与爆炸，皆在调配之间";
    }
    char roleText[48];
    std::snprintf(roleText, sizeof(roleText), "%s · %d级", titleName, level);

    // 座右铭（小字，淡色）
    ImGui::SetWindowFontScale(0.68f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.31f, 0.22f, 0.85f));
    ImGui::Text("%s", mottoText);
    ImGui::PopStyleColor();

    // 角色名+等级
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.28f, 0.24f, 0.22f, 0.95f));
    ImGui::SetWindowFontScale(0.64f);
    ImGui::Text("%s", roleText);
    ImGui::PopStyleColor();
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Dummy({ 0, 6.0f });

    // ── HP 条（标签在条内左侧） ──
    float hpRatio = std::clamp(m_hpBarSmooth, 0.0f, 1.0f);
    float barH = 22.0f;
    float barR = barH * 0.4f;
    ImVec2 hpBarPos = ImGui::GetCursorScreenPos();

    // 背景槽
    dl->AddRectFilled(hpBarPos, { hpBarPos.x + innerW, hpBarPos.y + barH },
        IM_COL32(170, 153, 130, 230), barR);

    // 伤害残影
    float hpActual = (m_activePlayer->getMaxHp() > 0)
        ? static_cast<float>(m_activePlayer->getHp()) / static_cast<float>(m_activePlayer->getMaxHp())
        : 0.0f;
    if (hpActual < m_hpBarSmooth - 0.01f) {
        float ghostW = hpActual * innerW;
        dl->AddRectFilled(hpBarPos, { hpBarPos.x + ghostW, hpBarPos.y + barH },
            IM_COL32(188, 102, 95, 180), barR);
    }

    // 当前气血填充
    if (hpRatio > 0.001f) {
        float fillW = hpRatio * innerW;
        float r = 0.70f + (1.0f - hpRatio) * 0.18f;
        float g = 0.19f * hpRatio + 0.07f;
        float b = 0.18f * hpRatio + 0.08f;
        dl->AddRectFilled(hpBarPos, { hpBarPos.x + fillW, hpBarPos.y + barH },
            IM_COL32(
                static_cast<int>(r * 255),
                static_cast<int>(g * 255),
                static_cast<int>(b * 255), 255
            ), barR);
    }

    // 外框
    dl->AddRect(hpBarPos, { hpBarPos.x + innerW, hpBarPos.y + barH },
        IM_COL32(92, 76, 62, 220), barR, 0, 1.2f);

    // HP 标签（左侧）+ 数值（居中）
    int hp = m_activePlayer->getHp();
    int maxHp = m_activePlayer->getMaxHp();
    char hpText[32];
    std::snprintf(hpText, sizeof(hpText), "%d / %d", hp, maxHp);

    ImGui::SetWindowFontScale(0.62f);
    ImVec2 hpLabelSize = ImGui::CalcTextSize("HP");
    ImVec2 hpTextSize = ImGui::CalcTextSize(hpText);

    // HP 标签（左，深色）
    dl->AddText({ hpBarPos.x + 8.0f,
                   hpBarPos.y + (barH - hpLabelSize.y) * 0.5f },
        IM_COL32(60, 45, 38, 220), "HP");

    // 数值（居中）
    dl->AddText({ hpBarPos.x + (innerW - hpTextSize.x) * 0.5f,
                   hpBarPos.y + (barH - hpTextSize.y) * 0.5f },
        IM_COL32(252, 248, 242, 235), hpText);
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Dummy({ 0, barH + 8.0f });

    // ── EXP 条（标签在条内左侧，更细） ──
    float expRatio = std::clamp(m_expBarSmooth, 0.0f, 1.0f);
    float expBarH = 16.0f;
    float expBarR = expBarH * 0.4f;
    ImVec2 expBarPos = ImGui::GetCursorScreenPos();

    // 背景槽
    dl->AddRectFilled(expBarPos, { expBarPos.x + innerW, expBarPos.y + expBarH },
        IM_COL32(156, 151, 138, 210), expBarR);

    // 经验填充
    if (expRatio > 0.001f) {
        float fillW = expRatio * innerW;
        dl->AddRectFilledMultiColor(
            expBarPos,
            { expBarPos.x + fillW, expBarPos.y + expBarH },
            IM_COL32(48, 96, 146, 238),
            IM_COL32(68, 126, 174, 238),
            IM_COL32(48, 122, 116, 230),
            IM_COL32(38, 108, 104, 230)
        );
    }

    // 外框
    dl->AddRect(expBarPos, { expBarPos.x + innerW, expBarPos.y + expBarH },
        IM_COL32(86, 79, 72, 210), expBarR, 0, 1.2f);

    // EXP 标签（左侧）+ 数值（居中）
    int exp = m_activePlayer->getExp();
    int expToNext = m_activePlayer->getExpToNext();
    char expText[32];
    std::snprintf(expText, sizeof(expText), "%d / %d", exp, expToNext);

    ImGui::SetWindowFontScale(0.58f);
    ImVec2 expLabelSize = ImGui::CalcTextSize("EXP");
    ImVec2 expTextSize = ImGui::CalcTextSize(expText);

    // EXP 标签（左，靛蓝色）
    dl->AddText({ expBarPos.x + 8.0f,
                   expBarPos.y + (expBarH - expLabelSize.y) * 0.5f },
        IM_COL32(38, 78, 108, 220), "EXP");

    // 数值（居中）
    dl->AddText({ expBarPos.x + (innerW - expTextSize.x) * 0.5f,
                   expBarPos.y + (expBarH - expTextSize.y) * 0.5f },
        IM_COL32(232, 236, 241, 212), expText);
    ImGui::SetWindowFontScale(1.0f);

    // ── 计时器（EXP 条下方，右对齐，独占一行） ──
    int totalSec = static_cast<int>(m_gameplayElapsed);
    int mm = totalSec / 60;
    int ss = totalSec % 60;
    char timerText[24];
    std::snprintf(timerText, sizeof(timerText), "TIME  %02d:%02d", mm, ss);

    ImGui::SetWindowFontScale(0.55f);
    ImVec2 timerSize = ImGui::CalcTextSize(timerText);
    float timerY = expBarPos.y + expBarH + 4.0f;
    dl->AddText({ expBarPos.x + innerW - timerSize.x,
                   timerY },
        IM_COL32(180, 174, 162, 200), timerText);
    ImGui::SetWindowFontScale(1.0f);

    // ── 蓝屏叠层指示器（始终显示在计时器下方） ──
    {
        ImGui::SetWindowFontScale(0.55f);
        char stackText[32];
        std::snprintf(stackText, sizeof(stackText), "BSOD  %d / 5", m_blueStackCount);
        float stackRatio = static_cast<float>(m_blueStackCount) / 5.0f;
        // 计时器下方 + 间隔4px
        ImVec2 stPos = { expBarPos.x, timerY + timerSize.y + 4.0f };
        float stH = 14.0f;
        float stR = stH * 0.35f;
        // 背景
        dl->AddRectFilled(stPos, { stPos.x + innerW, stPos.y + stH },
            IM_COL32(44, 44, 55, 200), stR);
        // 填充
        if (stackRatio > 0.f) {
            int r = static_cast<int>(30  + stackRatio * 200);
            int g = 30;
            int b = static_cast<int>(215 - stackRatio * 100);
            dl->AddRectFilled(stPos, { stPos.x + innerW * stackRatio, stPos.y + stH },
                IM_COL32(r, g, b, 220), stR);
        }
        // 边框
        dl->AddRect(stPos, { stPos.x + innerW, stPos.y + stH },
            IM_COL32(80, 80, 140, 180), stR, 0, 1.0f);
        // 文字居中
        ImVec2 stTextSize = ImGui::CalcTextSize(stackText);
        dl->AddText({ stPos.x + (innerW - stTextSize.x) * 0.5f,
                      stPos.y + (stH - stTextSize.y) * 0.5f },
            m_blueStackCount > 0 ? IM_COL32(200, 210, 255, 240)
                                 : IM_COL32(130, 130, 150, 160),
            stackText);
        ImGui::SetWindowFontScale(1.0f);
    }

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

void Game::renderGameplay() {
    if (!m_gameMap) return;

    float mapPixelW = static_cast<float>(m_gameMap->getCols() * TILE_RENDER_SIZE);
    float mapPixelH = static_cast<float>(m_gameMap->getRows() * TILE_RENDER_SIZE);

    m_camera.setSize({ mapPixelW, mapPixelH });

    float halfW = mapPixelW * 0.5f;
    float halfH = mapPixelH * 0.5f;
    float targetCX = m_playerPos.x + TILE_RENDER_SIZE * 0.5f;
    float targetCY = m_playerPos.y + TILE_RENDER_SIZE * 0.5f;
    float clampedCX = targetCX;
    float clampedCY = targetCY;
    if (clampedCX < halfW) clampedCX = halfW;
    if (clampedCX > mapPixelW - halfW) clampedCX = mapPixelW - halfW;
    if (clampedCY < halfH) clampedCY = halfH;
    if (clampedCY > mapPixelH - halfH) clampedCY = mapPixelH - halfH;

    m_camera.setCenter({ clampedCX, clampedCY });
    sf::FloatRect vpFull(sf::Vector2f(0.f, 0.f), sf::Vector2f(1.f, 1.f));
    m_camera.setViewport(vpFull);
    m_window.setView(m_camera);

    // ── 渲染地图 ──
    m_gameMap->render(m_window, TILE_RENDER_SIZE);

    // ── 渲染技能（法阵等，在角色和敌人之下） ──
    renderSkills();

    // ── 渲染角色 ──
    if (m_playerSpriteLoaded && m_playerSprite.has_value()) {
        sf::Vector2f dir = m_facingDir;
        sf::Texture* tex = &m_charDown;
        if (dir.y > 0)       tex = &m_charDown;
        else if (dir.y < 0)  tex = &m_charUp;
        else if (dir.x < 0)  tex = &m_charLeft;
        else if (dir.x > 0)  tex = &m_charRight;

        m_playerSprite->setTexture(*tex);

        // 根据当前纹理尺寸动态计算 scale 和 offset（不同朝向素材尺寸可能不同）
        unsigned int tw = tex->getSize().x, th = tex->getSize().y;
        float targetH = 202.0f;
        float scale = targetH / (float)th;
        m_playerSprite->setScale({ scale, scale });
        float spriteW = (float)tw * scale;
        float spriteH = (float)th * scale;
        float offX = -(spriteW - (float)TILE_RENDER_SIZE) * 0.5f;
        float offY = -(spriteH - (float)TILE_RENDER_SIZE) + 75.f;
        m_playerSprite->setPosition(sf::Vector2f(
            m_playerPos.x + offX,
            m_playerPos.y + offY
        ));
        m_window.draw(*m_playerSprite);
    } else {
        m_playerShape.setPosition(m_playerPos);
        m_window.draw(m_playerShape);
    }

    // ── 渲染覆盖层 ──
    m_gameMap->renderOverlays(m_window, TILE_RENDER_SIZE);

    // ── 渲染道具 ──
    renderPickups();
    renderEnemies();

    // ── 渲染技能命中特效（在敌人之上） ──
    if (m_skills.empty()) {} else {
        for (auto& skill : m_skills)
            skill->renderHitEffects(m_window);
    }

    m_window.setView(m_window.getDefaultView());

    renderHUD();
    renderSkillLevelUpUI();
    if (m_isDead) renderDeathScreen();

    float s = m_uiScale;
    float escHintW = static_cast<float>(m_window.getSize().x);
    float escHintH = static_cast<float>(m_window.getSize().y);

    ImGui::SetNextWindowPos({ escHintW - 180 * s, escHintH - 45 * s });
    ImGui::SetNextWindowSize({ 160 * s, 35 * s });
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.4f));
    ImGui::Begin("##escHint", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBackground
    );
    ImGui::TextColored(ImVec4(1, 1, 1, 0.76f), "ESC 返回主页面");
    ImGui::End();
    ImGui::PopStyleColor();

    ImGui::SFML::Render(m_window);
}

void Game::render() {
    m_window.clear();

    if (m_state == GameState::MainMenu) {
        // ── 主菜单渲染 ──
        if (m_textureLoaded) {
            auto& sprite = m_menuBgSprite.value();
            auto winSize = m_window.getSize();
            auto texSize = m_menuBgTexture.getSize();

            float scaleX = (float)winSize.x / texSize.x;
            float scaleY = (float)winSize.y / texSize.y;

            sprite.setScale({ scaleX, scaleY });
            sprite.setPosition({ 0, 0 });
            m_window.draw(sprite);
        }
        else {
            m_window.draw(m_fallbackBg);
        }

        ImGui::SFML::Render(m_window);
    }
    else if (m_state == GameState::CharSelect) {
        // ── 角色选择界面背景（复用主菜单背景） ──
        if (m_textureLoaded) {
            auto& sprite = m_menuBgSprite.value();
            auto winSize = m_window.getSize();
            auto texSize = m_menuBgTexture.getSize();
            float scaleX = (float)winSize.x / texSize.x;
            float scaleY = (float)winSize.y / texSize.y;
            sprite.setScale({ scaleX, scaleY });
            sprite.setPosition({ 0, 0 });
            m_window.draw(sprite);
        } else {
            m_window.draw(m_fallbackBg);
        }
        ImGui::SFML::Render(m_window);
    }
    else if (m_state == GameState::Gameplay) {
        renderGameplay();
    }

    m_window.display();
}

// ======================
// 道具系统
// ======================
void Game::updatePickups(float dt) {
    // ── 随机刷新计时（回血 + 加速道具，15~30 秒一个） ──
    m_pickupSpawnTimer += dt;
    if (m_pickupSpawnTimer >= m_nextSpawnInterval) {
        m_pickupSpawnTimer = 0.0f;
        m_nextSpawnInterval = 7.5f + std::rand() % 750 / 100.0f; // 7.5 ~ 15.0
        spawnRandomPickup();
    }

    // ── 更新所有道具 ──
    for (auto& pickup : m_pickups) {
        pickup.update(dt);
    }

    // ── 拾取检测：用精灵实际渲染的包围盒 ──
    sf::FloatRect playerBounds(
        sf::Vector2f(m_playerPos.x + m_spriteOffsetX + m_charCollisionBox.position.x,
                     m_playerPos.y + m_spriteOffsetY + m_charCollisionBox.position.y),
        sf::Vector2f(m_charCollisionBox.size.x, m_charCollisionBox.size.y)
    );
    // fallback：如果碰撞箱无效就用 tile 大小
    if (playerBounds.size.x <= 0 || playerBounds.size.y <= 0) {
        playerBounds = sf::FloatRect(m_playerPos, { (float)TILE_RENDER_SIZE, (float)TILE_RENDER_SIZE });
    }
    for (auto& pickup : m_pickups) {
        if (!pickup.isAlive()) continue;
        if (playerBounds.findIntersection(pickup.getBounds()).has_value()) {
            pickupItem(pickup);
            pickup.kill();
        }
    }

    // ── 移除已消失的道具 ──
    m_pickups.erase(
        std::remove_if(m_pickups.begin(), m_pickups.end(),
                       [](const ItemPickup& p) { return !p.isAlive(); }),
        m_pickups.end()
    );
}

void Game::renderPickups() {
    for (auto& pickup : m_pickups) {
        pickup.render(m_window);
    }
}

void Game::spawnRandomPickup() {
    if (!m_gameMap) return;

    // 随机选一个类型：回血 或 加速
    PickupType type = (std::rand() % 2 == 0) ? PickupType::Heal : PickupType::Speed;

    // 选择纹理
    const sf::Texture* tex = nullptr;
    if (type == PickupType::Heal && m_healItemLoaded) tex = &m_healItemTexture;
    if (type == PickupType::Speed && m_speedItemLoaded) tex = &m_speedItemTexture;
    // 贴图未加载：经验直接给玩家；治疗/加速道具则不生成
    if (!tex) {
        if (type == PickupType::Exp && m_activePlayer) {
            int expAmount = 50;
            if (m_expBuffTimer > 0.0f) expAmount *= 2;
            m_activePlayer->addExp(expAmount);
        }
        return;
    }

    // 随机位置（避开碰撞区域，最多尝试 200 次）
    int cols = m_gameMap->getCols();
    int rows = m_gameMap->getRows();
    float rs = static_cast<float>(TILE_RENDER_SIZE) / m_gameMap->getTileWidth();
    float itemSizeTile = (TILE_RENDER_SIZE * 4.0f) / rs;  // 道具在 Tiled 世界坐标下的大小

    for (int attempts = 0; attempts < 200; ++attempts) {
        int tx = 2 + std::rand() % (cols - 4);
        int ty = 2 + std::rand() % (rows - 4);
        sf::Vector2f pos = {
            static_cast<float>(tx) * TILE_RENDER_SIZE,
            static_cast<float>(ty) * TILE_RENDER_SIZE
        };
        // pos 是像素坐标，除以 rs 转换为 Tiled 世界坐标传给 isRectColliding
        if (!m_gameMap->isRectColliding(
                pos.x / rs, pos.y / rs,
                itemSizeTile, itemSizeTile)) {
            m_pickups.emplace_back(type, *tex, pos, TILE_RENDER_SIZE * 6.0f);
            return;
        }
    }
}

void Game::spawnPickup(PickupType type, sf::Vector2f pos, int expValue) {
    const sf::Texture* tex = nullptr;
    if (type == PickupType::Heal && m_healItemLoaded) tex = &m_healItemTexture;
    if (type == PickupType::Speed && m_speedItemLoaded) tex = &m_speedItemTexture;
    if (type == PickupType::Exp   && m_expItemLoaded)   tex = &m_expItemTexture;

    // 贴图未加载：经验直接给玩家，其他类型直接返回
    if (!tex) {
        if (type == PickupType::Exp && m_activePlayer && expValue > 0) {
            m_activePlayer->addExp(expValue);
        }
        return;
    }

    // 碰撞检测：将像素坐标转为瓦片坐标，检测 3x3 瓦片区域（覆盖放大后的道具尺寸）
    if (m_gameMap) {
        float tileW = static_cast<float>(m_gameMap->getTileWidth());
        int tx = static_cast<int>(pos.x / tileW);
        int ty = static_cast<int>(pos.y / tileW);

        if (m_gameMap->isRectColliding(static_cast<float>(tx) - 1.f,
                                        static_cast<float>(ty) - 1.f, 3, 3)) {
            // 在 3x3 像素偏移里找安全瓦片
            const int tryOrder[][2] = {{1,0},{-1,0},{0,1},{0,-1},
                                        {1,1},{-1,1},{1,-1},{-1,-1},
                                        {2,0},{-2,0},{0,2},{0,-2}};
            bool found = false;
            for (auto& off : tryOrder) {
                sf::Vector2f tryPos = { pos.x + off[0] * tileW,
                                         pos.y + off[1] * tileW };
                int tx2 = static_cast<int>(tryPos.x / tileW);
                int ty2 = static_cast<int>(tryPos.y / tileW);
                if (tx2 < 0 || ty2 < 0) continue;
                if (!m_gameMap->isRectColliding(static_cast<float>(tx2) - 1.f,
                                                static_cast<float>(ty2) - 1.f, 3, 3)) {
                    pos = tryPos;
                    found = true;
                    break;
                }
            }
            // 找不到安全位置：经验直接给玩家，不丢失
            if (!found) {
                if (type == PickupType::Exp && m_activePlayer && expValue > 0) {
                    int amount = expValue;
                    if (m_expBuffTimer > 0.0f) amount *= 2;
                    m_activePlayer->addExp(amount);
                }
                return;
            }
        }
    }

    m_pickups.emplace_back(type, *tex, pos, TILE_RENDER_SIZE * 6.0f, expValue);
}

void Game::pickupItem(const ItemPickup& item) {
    if (!m_activePlayer) return;

    switch (item.getType()) {
    case PickupType::Heal: {
        int healAmount = static_cast<int>(m_activePlayer->getMaxHp() * 0.2f);
        m_activePlayer->heal(healAmount);
        break;
    }
    case PickupType::Speed: {
        m_speedBuffTimer = 10.0f;
        m_playerSpeed = m_activePlayer->getSpeed() * 1.5f;
        break;
    }
    case PickupType::Exp: {
        int expAmount = item.getExpValue();
        if (m_expBuffTimer > 0.0f) expAmount *= 2;
        m_activePlayer->addExp(expAmount);
        break;
    }
    }
}

// ============================================================
// 敌人系统
// ============================================================
void Game::initEnemies() {
    m_enemies.clear();

    // 加载服务器机柜素材
    m_dashEnemyTexLoaded  = m_dashEnemyTexLeft.loadFromFile("assets/sprites/enemies/server_left.png");
    m_dashEnemyTexLoaded &= m_dashEnemyTexRight.loadFromFile("assets/sprites/enemies/server_right.png");
    if (m_dashEnemyTexLoaded) {
        m_dashEnemyTexLeft.setSmooth(true);
        m_dashEnemyTexRight.setSmooth(true);
    }

    if (!m_dashEnemyTexLoaded) {
        // fallback：生成简单色块纹理
        auto makeFallback = [](sf::Texture& tex, sf::Color col) {
            sf::Image img(sf::Vector2u(40u, 64u), col);
            tex.loadFromImage(img);
            tex.setSmooth(true);
        };
        makeFallback(m_dashEnemyTexLeft,  sf::Color(60, 60, 80, 255));
        makeFallback(m_dashEnemyTexRight, sf::Color(60, 60, 80, 255));
        m_dashEnemyTexLoaded = true;
    }

    // 加载蓝屏敌人素材
    m_bsodTexLoaded  = m_bsodTexLeft.loadFromFile("assets/sprites/enemies/bluescreen_left.png");
    m_bsodTexLoaded &= m_bsodTexRight.loadFromFile("assets/sprites/enemies/bluescreen_right.png");
    if (m_bsodTexLoaded) {
        m_bsodTexLeft.setSmooth(true);
        m_bsodTexRight.setSmooth(true);
    }

    if (!m_bsodTexLoaded) {
        auto makeFallback = [](sf::Texture& tex, sf::Color col) {
            sf::Image img(sf::Vector2u(64u, 52u), col);
            tex.loadFromImage(img);
            tex.setSmooth(true);
        };
        makeFallback(m_bsodTexLeft,  sf::Color(0, 120, 215, 255));
        makeFallback(m_bsodTexRight, sf::Color(0, 120, 215, 255));
        m_bsodTexLoaded = true;
    }

    // 加载偷外卖贼素材
    m_thiefTexLoaded  = m_thiefTexLeft.loadFromFile("assets/sprites/enemies/steal_thief_left.png");
    m_thiefTexLoaded &= m_thiefTexRight.loadFromFile("assets/sprites/enemies/steal_thief_right.png");
    if (m_thiefTexLoaded) {
        m_thiefTexLeft.setSmooth(true);
        m_thiefTexRight.setSmooth(true);
    }

    if (!m_thiefTexLoaded) {
        // fallback：深灰色方块
        auto makeFallback = [](sf::Texture& tex, sf::Color col) {
            sf::Image img(sf::Vector2u(64u, 64u), col);
            tex.loadFromImage(img);
            tex.setSmooth(true);
        };
        makeFallback(m_thiefTexLeft,  sf::Color(50, 50, 60, 255));
        makeFallback(m_thiefTexRight, sf::Color(50, 50, 60, 255));
        m_thiefTexLoaded = true;
    }

    if (!m_gameMap) return;

    // 在地图上随机散布若干个敌人（服务器机柜 + 蓝屏）
    float mapW = m_gameMap->getCols() * TILE_RENDER_SIZE;
    float mapH = m_gameMap->getRows() * TILE_RENDER_SIZE;
    float rs    = static_cast<float>(TILE_RENDER_SIZE) / m_gameMap->getTileWidth();
    float tileW = static_cast<float>(m_gameMap->getTileWidth());

    // 敌人像素大小（用于碰撞检测）
    const float DASH_SIZE  = 50.f;   // DashEnemy 约 50x80
    const float BSOD_SIZE_W = 200.f; // BlueScreenEnemy 约 200x163
    const float BSOD_SIZE_H = 163.f;
    const float THIEF_SIZE = 64.f;   // ThiefEnemy 约 64x64

    // 将敌人像素大小转换为 tile 单位（向上取整确保覆盖）
    auto toTileCheckSize = [&](float pixelSize) -> float {
        return std::ceil(pixelSize / tileW) + 1.f; // 加1 tile 边缘
    };

    const float dashCheck  = toTileCheckSize(DASH_SIZE);
    const float bsodCheckW = toTileCheckSize(BSOD_SIZE_W);
    const float bsodCheckH = toTileCheckSize(BSOD_SIZE_H);
    const float thiefCheck = toTileCheckSize(THIEF_SIZE);

    // 辅助函数：检测指定位置的敌人是否与碰撞层重叠
    auto isSpawnPosColliding = [&](float x, float y, float checkW, float checkH) -> bool {
        int tx = static_cast<int>(x / tileW);
        int ty = static_cast<int>(y / tileW);
        // 检测区域要比敌人稍大一点，确保完全覆盖
        return m_gameMap->isRectColliding(
            static_cast<float>(tx) - 0.5f,
            static_cast<float>(ty) - 0.5f,
            checkW + 1.f,
            checkH + 1.f
        );
    };

    const int DASH_COUNT = 3;
    const int BSOD_COUNT = 3;
    int attempts = 0;
    int spawnedDash = 0;
    int spawnedBsod = 0;

    // 服务器机柜
    while (spawnedDash < DASH_COUNT && attempts < 500) {
        ++attempts;
        float x = static_cast<float>(std::rand() % static_cast<int>(mapW - 64));
        float y = static_cast<float>(std::rand() % static_cast<int>(mapH - 64));
        if (isSpawnPosColliding(x, y, dashCheck, dashCheck)) continue;
        sf::Vector2f spawnPos(x, y);
        sf::Vector2f diff = spawnPos - m_playerPos;
        if (diff.x * diff.x + diff.y * diff.y < 200.f * 200.f) continue;

        auto enemy = std::make_unique<DashEnemy>(
            spawnPos,
            &m_dashEnemyTexLeft,
            &m_dashEnemyTexRight
        );
        enemy->applyDifficulty(m_difficultyFactor);
        m_enemies.push_back(std::move(enemy));
        ++spawnedDash;
    }

    // 蓝屏敌人
    attempts = 0;
    while (spawnedBsod < BSOD_COUNT && attempts < 500) {
        ++attempts;
        float x = static_cast<float>(std::rand() % static_cast<int>(mapW - 64));
        float y = static_cast<float>(std::rand() % static_cast<int>(mapH - 64));
        if (isSpawnPosColliding(x, y, bsodCheckW, bsodCheckH)) continue;
        sf::Vector2f spawnPos(x, y);
        sf::Vector2f diff = spawnPos - m_playerPos;
        if (diff.x * diff.x + diff.y * diff.y < 200.f * 200.f) continue;

        auto enemy = std::make_unique<BlueScreenEnemy>(
            spawnPos,
            &m_bsodTexLeft,
            &m_bsodTexRight
        );
        enemy->applyDifficulty(m_difficultyFactor);
        m_enemies.push_back(std::move(enemy));
        ++spawnedBsod;
    }

    // 偷外卖贼（初始2只）
    const int THIEF_COUNT = 2;
    int spawnedThief = 0;
    attempts = 0;
    while (spawnedThief < THIEF_COUNT && attempts < 500) {
        ++attempts;
        float x = static_cast<float>(std::rand() % static_cast<int>(mapW - 64));
        float y = static_cast<float>(std::rand() % static_cast<int>(mapH - 64));
        if (isSpawnPosColliding(x, y, thiefCheck, thiefCheck)) continue;
        sf::Vector2f spawnPos(x, y);
        sf::Vector2f diff = spawnPos - m_playerPos;
        if (diff.x * diff.x + diff.y * diff.y < 200.f * 200.f) continue;

        auto thief = std::make_unique<ThiefEnemy>(
            spawnPos,
            &m_thiefTexLeft,
            &m_thiefTexRight
        );
        float baseSpd2 = (m_activePlayer ? m_activePlayer->getSpeed() : 200.f);
        float thiefSpeed = baseSpd2 * 0.6f;
        thief->setBaseSpeed(thiefSpeed);
        thief->setFleeSpeed(baseSpd2 * 0.9f);
        thief->applyDifficulty(m_difficultyFactor);
        m_enemies.push_back(std::move(thief));
        ++spawnedThief;
    }
}

void Game::updateEnemies(float dt) {
    if (!m_activePlayer || !m_playerSprite.has_value()) return;

    // 玩家精灵实际渲染中心（无透明通道几何中心）
    sf::FloatRect bounds = m_playerSprite->getGlobalBounds();
    sf::Vector2f playerCenter = {
        bounds.position.x + bounds.size.x / 2.f,
        bounds.position.y + bounds.size.y / 2.f
    };

    // 玩家非透明通道精确碰撞框（世界坐标）
    sf::FloatRect playerHitbox(
        sf::Vector2f(m_playerPos.x + m_spriteOffsetX + m_charCollisionBox.position.x,
                     m_playerPos.y + m_spriteOffsetY + m_charCollisionBox.position.y),
        sf::Vector2f(m_charCollisionBox.size.x, m_charCollisionBox.size.y)
    );
    // fallback：碰撞箱无效则用 getGlobalBounds
    if (playerHitbox.size.x <= 0 || playerHitbox.size.y <= 0) {
        playerHitbox = bounds;
    }

    // 确保所有敌人知道地图边界（ThiefEnemy 需要用来 clamp 逃跑路线）
    if (m_gameMap) {
        float mw = static_cast<float>(m_gameMap->getCols() * TILE_RENDER_SIZE);
        float mh = static_cast<float>(m_gameMap->getRows() * TILE_RENDER_SIZE);
        for (auto& e : m_enemies) {
            e->setMapBounds(mw, mh);
        }
    }

    for (auto& e : m_enemies) {
        // 死亡敌人：掉落经验 + 击杀计数（在 continue 之前，否则永远不会执行）
        if (e->isDead()) {
            if (!e->hasDroppedExp()) {
                e->markExpDropped();
                if (auto* thief = dynamic_cast<ThiefEnemy*>(e.get())) {
                    int stolen = thief->getStolenExp();
                    if (stolen > 0) {
                        if (m_expItemLoaded) {
                            spawnPickup(PickupType::Exp, e->getPosition(), stolen);
                        } else if (m_activePlayer) {
                            m_activePlayer->addExp(stolen);
                        }
                    }
                } else {
                    int expAmount = e->getExpValue();
                    if (m_expBuffTimer > 0.0f) expAmount *= 2;
                    if (m_expItemLoaded) {
                        spawnPickup(PickupType::Exp, e->getPosition(), expAmount);
                    } else if (m_activePlayer && expAmount > 0) {
                        m_activePlayer->addExp(expAmount);
                    }
                }
                ++m_killCount;
            }
            continue;
        }

        sf::Vector2f prevPos = e->getPosition();

        // 先尝试偷经验（按距离触发，无需触碰）
        int playerExp = m_activePlayer ? m_activePlayer->getExp() : 0;
        e->tryStealExp(playerExp, playerCenter);

        e->update(dt, playerCenter);

        // 触碰伤害检测（ThiefEnemy 此处不再处理偷取）
        int stolenAmt = e->checkTouchDamage(playerHitbox, dt, playerExp);

        // ── 偷贼：检测偷取事件，扣玩家经验 + 生成飘字 ──
        if (e->popStolenFlag() && m_activePlayer) {
            int stolen = e->getLastStolenAmount();
            int curExp = m_activePlayer->getExp();
            int actual = std::min(stolen, curExp);
            m_activePlayer->addExp(-actual);

            // 生成飘字 "-EXP"
            sf::Vector2f thiefWorldPos = e->getPosition();
            sf::Vector2i screenPosI = m_window.mapCoordsToPixel(
                thiefWorldPos, m_window.getView());
            FloatText ft;
            ft.text    = "-EXP";
            ft.pos     = { static_cast<float>(screenPosI.x), static_cast<float>(screenPosI.y) - 20.f };
            ft.life    = 1.5f;
            ft.elapsed = 0.f;
            ft.color   = sf::Color(220, 50, 50, 255);
            m_floatTexts.push_back(ft);
        }

        // ── 敌人碰撞检测：无视碰撞层的敌人可穿墙 ──
        if (m_gameMap && !e->ignoreMapCollision()) {
            float rs = static_cast<float>(TILE_RENDER_SIZE) / m_gameMap->getTileWidth();
            sf::Vector2f eSize = e->getSize();
            float ew = eSize.x / rs;
            float eh = eSize.y / rs;
            sf::Vector2f ePos = e->getPosition();
            if (m_gameMap->isRectColliding(ePos.x / rs, ePos.y / rs, ew, eh)) {
                // 尝试仅 X 方向移动
                sf::Vector2f tryX(prevPos.x, e->getPosition().y);
                bool canX = !m_gameMap->isRectColliding(tryX.x / rs, tryX.y / rs, ew, eh);
                // 尝试仅 Y 方向移动
                sf::Vector2f tryY(e->getPosition().x, prevPos.y);
                bool canY = !m_gameMap->isRectColliding(tryY.x / rs, tryY.y / rs, ew, eh);
                if (canX && !canY) {
                    e->setPosition(tryX);
                } else if (canY && !canX) {
                    e->setPosition(tryY);
                } else {
                    e->setPosition(prevPos);  // 都不行就完全回退
                }
            }
        }

        // 统一伤害接口：每类敌人自己管理待结算伤害
        int dmg = e->getPendingDamage(dt);
        if (dmg > 0) {
            m_activePlayer->takeDamage(dmg);
            // 检测玩家死亡
            if (m_activePlayer->getHp() <= 0) {
                m_isDead = true;
            }
            // DashEnemy 附加减速45%
            if (auto* de = dynamic_cast<DashEnemy*>(e.get())) {
                (void)de;   // getSlowAmount() 暂未暴露，减速值硬编码
                if (m_speedBuffTimer <= 0.0f) {
                    float baseSpeed = m_activePlayer->getSpeed();
                    m_playerSpeed   = baseSpeed * 0.55f;   // 减速45%
                    m_enemySlowTimer = 2.0f;
                }
            }
        }

        // ── 蓝屏叠层：每0.5s叠一层，满5层→僵直1.5s+清零 ──
        if (auto* bs = dynamic_cast<BlueScreenEnemy*>(e.get())) {
            if (bs->popStackEvent() && m_blueStunTimer <= 0.f) {
                m_blueStackCount = bs->getStackCount();
                if (m_blueStackCount >= 5) {
                    // 触发僵直
                    m_blueStunTimer = 1.5f;
                    m_blueStackCount = 0;
                    // 重置所有蓝屏敌人的叠层计数
                    for (auto& en : m_enemies) {
                        if (auto* bsReset = dynamic_cast<BlueScreenEnemy*>(en.get())) {
                            bsReset->resetStack();
                        }
                    }
                    // 生成提示飘字
                    sf::Vector2i spi = m_window.mapCoordsToPixel(
                        m_activePlayer->getPosition(), m_window.getView());
                    FloatText ft;
                    ft.text    = "BSOD!";
                    ft.pos     = { static_cast<float>(spi.x), static_cast<float>(spi.y) - 40.f };
                    ft.life    = 1.5f;
                    ft.elapsed = 0.f;
                    ft.color   = sf::Color(0, 120, 215, 255);
                    m_floatTexts.push_back(ft);
                }
            }
        }
    }

    // 更新飘字
    for (auto& ft : m_floatTexts) {
        ft.elapsed += dt;
        ft.pos.y   -= 40.f * dt;  // 向上漂移
    }
    m_floatTexts.erase(
        std::remove_if(m_floatTexts.begin(), m_floatTexts.end(),
            [](const FloatText& ft) { return ft.isDone(); }),
        m_floatTexts.end()
    );

    // 移除死亡敌人
    m_enemies.erase(
        std::remove_if(m_enemies.begin(), m_enemies.end(),
            [](const std::unique_ptr<Enemy>& e) { return e->isDead(); }),
        m_enemies.end()
    );
}

void Game::renderEnemies() {
    for (auto& e : m_enemies) {
        e->render(m_window);
    }

    // ── 飘字特效渲染（用 ImGui DrawList，在屏幕坐标） ──
    if (!m_floatTexts.empty()) {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        for (const auto& ft : m_floatTexts) {
            float alpha = 1.0f - (ft.elapsed / ft.life);
            ImU32 col = IM_COL32(
                ft.color.r, ft.color.g, ft.color.b,
                static_cast<int>(alpha * 255));
            dl->AddText(nullptr, 18.f,
                ImVec2(ft.pos.x, ft.pos.y),
                col, ft.text.c_str());
        }
    }
}

// ============================================================
// 技能系统
// ============================================================
void Game::initSkills() {
    m_skills.clear();
    m_showLevelUp = false;
    m_prevPlayerLevel = m_activePlayer ? m_activePlayer->getLevel() : 0;

    if (m_selectedCharIdx == 0) {
        // CS 学生技能组
        m_skills.push_back(std::make_unique<PointerStormSkill>());   // 底层·指针风暴
        m_skills.push_back(std::make_unique<MagicCircleSkill>());    // 编译优化·校徽法阵
        m_skills.push_back(std::make_unique<KoiShieldSkill>());      // 华工锦鲤·守护环绕
    } else {
        // 化学学生技能组（锦鲤与CS完全共用，另外两技能逻辑相同仅素材/文案不同）
        m_skills.push_back(std::make_unique<ExothermicStormSkill>()); // 放热反应·试剂风暴
        m_skills.push_back(std::make_unique<ChemCircleSkill>());      // 催化循环·分子法阵
        m_skills.push_back(std::make_unique<KoiShieldSkill>());       // 华工锦鲤·守护环绕（共用）
    }
}

void Game::updateSkills(float dt) {
    if (m_skills.empty()) return;

    // 技能中心：用角色碰撞箱中心位置
    sf::Vector2f skillCenter = {
        m_playerPos.x + (float)TILE_RENDER_SIZE * 0.5f,
        m_playerPos.y + (float)TILE_RENDER_SIZE * 0.8f  // 略高于脚底，视觉居中
    };

    // ── 检测升级：弹出UI时随机一次属性 ──
    if (m_activePlayer) {
        int playerLevel = m_activePlayer->getLevel();
        if (!m_showLevelUp && m_prevPlayerLevel < playerLevel) {
            m_showLevelUp = true;
            m_prevPlayerLevel = playerLevel;
            m_levelUpRandomized = false; // 重置，触发随机
        }
    }

    for (auto& skill : m_skills) {
        skill->update(dt, skillCenter, this);
    }
}

void Game::renderSkills() {
    if (m_skills.empty()) return;

    // 技能中心：与 updateSkills 保持一致
    sf::Vector2f skillCenter = {
        m_playerPos.x + (float)TILE_RENDER_SIZE * 0.5f,
        m_playerPos.y + (float)TILE_RENDER_SIZE * 0.8f
    };

    for (auto& skill : m_skills) {
        skill->render(m_window, skillCenter);
    }
}

void Game::renderSkillLevelUpUI() {
    if (!m_showLevelUp) return;

    // ── 全屏蒙版 ──
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImVec2 vp = ImGui::GetMainViewport()->Size;
    dl->AddRectFilled({ 0, 0 }, vp, IM_COL32(180, 175, 160, 210));

    // ── 技能纹理缓存 ──
    static std::map<std::string, sf::Texture> s_texCache;

    size_t skillCount = m_skills.size();
    if (skillCount < 3) skillCount = 3;

    // 弹出时一次性随机属性
    if (!m_levelUpRandomized) {
        m_cachedRandomPicks.resize(skillCount);
        for (size_t i = 0; i < skillCount; ++i) {
            if (i < m_skills.size()) {
                auto ups = m_skills[i]->getUpgrades();
                m_cachedRandomPicks[i] = ups.empty() ? 0 : static_cast<size_t>(std::rand()) % ups.size();
            } else {
                m_cachedRandomPicks[i] = 0;
            }
        }
        m_levelUpRandomized = true;
    }

    const int cols = 3;
    float gap = 16.f;
    float totalGap = gap * (cols + 1);
    float cardW = (vp.x - totalGap) / static_cast<float>(cols);
    float cardH = vp.y * 0.62f;
    float topY  = (vp.y - cardH) / 2.f + 24.f;

    // ── 先用底层窗口处理所有点击 ──
    ImGui::SetNextWindowPos({ 0, 0 });
    ImGui::SetNextWindowSize(vp);
    ImGui::Begin("##LevelUpBG", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBackground);

    // 标题
    float titleW = 340.f;
    ImGui::SetCursorPos({ (vp.x - titleW) / 2.f, 14.f });
    ImGui::PushFont(m_customFont);
    ImGui::TextColored(ImVec4(0.55f, 0.42f, 0.22f, 1.f), "-- LEVEL UP !  选择一项技能强化 --");
    ImGui::PopFont();

    m_levelUpChoice = -1;

    // ── 三张卡片：纯 DrawList 绘制 + 透明按钮覆盖 ──
    for (size_t i = 0; i < skillCount; ++i) {
        float leftX = gap + static_cast<float>(i) * (cardW + gap);
        ImVec2 cardTL = { leftX, topY };
        ImVec2 cardBR = { leftX + cardW, topY + cardH };

        ImGui::PushID(static_cast<int>(i));

        // 先定义 hasSkill
        bool hasSkill = (i < m_skills.size());

        // ── 卡片背景 ──
        int tint = static_cast<int>(i) * 5;
        dl->AddRectFilled(cardTL, cardBR,
            IM_COL32(248 - tint, 243 - tint, 232 - tint, 248), 10.f);
        dl->AddRect(cardTL, cardBR,
            IM_COL32(160 - tint * 2, 148 - tint * 2, 120 - tint * 2, 200),
            10.f, 0, 2.f);

        // ── 透明按钮覆盖整张卡片（用 InvisibleButton 绝对定位）──
        ImGui::SetCursorPos({ leftX, topY });
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0,0,0,0));
        std::string cardBtnId = "##cardBtn" + std::to_string(i);
        ImGui::InvisibleButton(cardBtnId.c_str(), ImVec2(cardW, cardH));
        ImGui::PopStyleColor(3);

        // 悬停高亮
        if (hasSkill && ImGui::IsItemHovered()) {
            dl->AddRect(cardTL, cardBR,
                IM_COL32(200, 160, 60, 200), 10.f, 0, 2.5f);
        }

        // 点击处理
        if (hasSkill && ImGui::IsItemClicked()) {
            auto ups = m_skills[i]->getUpgrades();
            size_t pick = m_cachedRandomPicks[i];
            if (pick < ups.size()) {
                m_skills[i]->applyUpgrade(ups[pick].statName, ups[pick].delta);
                m_levelUpChoice = static_cast<int>(i);
                m_showLevelUp = false;
            }
        }

        // ── 1. 技能名称 ──
        const char* skillName = hasSkill ? m_skills[i]->getName().c_str() : "??? (占位)";
        int skillLv = hasSkill ? m_skills[i]->getLevel() : 0;

        float cx = leftX + 10.f;
        float contentW = cardW - 20.f;

        // 名称
        dl->AddText({ cx, topY + 8.f },
            IM_COL32(107, 82, 46, 255), skillName);

        // 等级（右对齐）
        char lvBuf[32];
        snprintf(lvBuf, sizeof(lvBuf), "Lv.%d", skillLv);
        ImVec2 lvSize = ImGui::CalcTextSize(lvBuf);
        dl->AddText({ leftX + cardW - 10.f - lvSize.x, topY + 12.f },
            IM_COL32(140, 128, 107, 255), lvBuf);

        // ── 2. 技能图片 ──
        float imgTopY = topY + 50.f;  // 增加间距，避免与标题重合
        float imgMaxW = contentW;
        float imgMaxH = cardH * 0.50f;

        sf::Texture* tex = nullptr;
        if (hasSkill) {
            std::string imgPath = m_skills[i]->getLevelupImagePath();
            auto it = s_texCache.find(imgPath);
            if (it != s_texCache.end()) {
                tex = &it->second;
            } else {
                sf::Texture t;
                if (t.loadFromFile(imgPath)) {
                    s_texCache[imgPath] = std::move(t);
                    tex = &s_texCache[imgPath];
                }
            }
        }

        if (tex) {
            auto ts = tex->getSize();
            float scale = std::min(imgMaxW / static_cast<float>(ts.x),
                                  imgMaxH / static_cast<float>(ts.y));
            float drawW = static_cast<float>(ts.x) * scale;
            float drawH = static_cast<float>(ts.y) * scale;
            ImGui::SetCursorPos({ leftX + (cardW - drawW) / 2.f, imgTopY });
            ImGui::Image((ImTextureID)(intptr_t)tex->getNativeHandle(),
                        ImVec2(drawW, drawH));
        } else {
            // 占位色块
            dl->AddRectFilled(
                { cx, imgTopY },
                { cx + imgMaxW, imgTopY + imgMaxH },
                IM_COL32(228, 224, 215, 220), 8.f);
            const char* placeholder = "技能图片";
            ImVec2 textSize = ImGui::CalcTextSize(placeholder);
            dl->AddText({
                cx + (imgMaxW - textSize.x) / 2.f,
                imgTopY + (imgMaxH - textSize.y) / 2.f },
                IM_COL32(165, 155, 140, 220), placeholder);
        }

        // ── 3. 升级属性区域 ──
        float attrTopY = imgTopY + imgMaxH + 10.f;
        float attrH = 80.f;
        float attrW = contentW;

        if (hasSkill) {
            auto upgrades = m_skills[i]->getUpgrades();
            if (!upgrades.empty()) {
                size_t pick = m_cachedRandomPicks[i];
                auto& chosen = upgrades[pick];

                ImVec2 attrTL = { cx + 2.f, attrTopY };
                ImVec2 attrBR = { attrTL.x + attrW, attrTL.y + attrH };

                // 属性背景
                dl->AddRectFilled(attrTL, attrBR,
                    IM_COL32(218, 208, 190, 230), 6.f);
                dl->AddRect(attrTL, attrBR,
                    IM_COL32(155, 140, 110, 180), 6.f, 0, 1.2f);

                // 左侧装饰竖条
                dl->AddRectFilled(
                    { attrTL.x + 4.f, attrTL.y + 8.f },
                    { attrTL.x + 7.f, attrBR.y - 8.f },
                    IM_COL32(180, 145, 80, 220));

                // 属性描述（在框内居中显示，支持自动换行）
                const char* statDesc = "";
                if (chosen.statName == "range") {
                    statDesc = "攻击范围扩大，覆盖更远的目标区域";
                } else if (chosen.statName == "maxTargets") {
                    statDesc = "同时命中更多目标，清场效率大幅提升";
                } else if (chosen.statName == "cooldown") {
                    statDesc = "技能冷却时间缩短，攻击频率显著提升";
                } else if (chosen.statName == "damage") {
                    statDesc = "技能伤害提升，每次攻击更加致命";
                } else if (chosen.statName == "tick") {
                    statDesc = "伤害触发间隔缩短，持续压制更凶猛";
                } else if (chosen.statName == "radius") {
                    statDesc = "作用范围扩大，更多敌人被卷入其中";
                } else if (chosen.statName == "count") {
                    statDesc = "增加环绕数量，攻防一体更强大";
                } else {
                    statDesc = "提升技能属性";
                }

                // 计算文本区域宽度（减去左侧装饰条和边距）
                float textW = attrW - 24.f;
                ImVec2 textSize = ImGui::CalcTextSize(statDesc, nullptr, false, textW);
                float textX = attrTL.x + 18.f;  // 左侧装饰条宽度 + 边距
                float textY = attrTL.y + (attrH - textSize.y) / 2.f;  // 垂直居中
                dl->AddText(nullptr, ImGui::GetFontSize(), { textX, textY },
                    IM_COL32(133, 97, 46, 255), statDesc, nullptr, textW);
            }
        } else {
            // 占位：待解锁（高度与属性区域一致）
            dl->AddRectFilled(
                { cx, attrTopY }, { cx + attrW, attrTopY + attrH },
                IM_COL32(210, 205, 195, 200), 6.f);
            ImVec2 lockSize = ImGui::CalcTextSize("待解锁");
            dl->AddText({
                cx + (attrW - lockSize.x) / 2.f,
                attrTopY + (attrH - lockSize.y) / 2.f },
                IM_COL32(160, 150, 135, 220), "待解锁");
        }

        // ── 4. 介绍词（卡片底部上方，小字，水平居中）──
        const char* descText = hasSkill ? m_skills[i]->getDesc().c_str()
                                        : "该技能尚未解锁，敬请期待。";
        float descW = contentW - 4.f;
        float descFontSz = ImGui::GetFontSize() * 0.50f * m_uiScale;
        ImVec2 descSize = ImGui::CalcTextSize(descText, nullptr, false, descW);
        float descDrawX = cx + (descW - descSize.x) / 2.f;  // 水平居中
        float descDrawY = cardBR.y - 48.f - descSize.y;
        dl->AddText(nullptr, descFontSz,
            { descDrawX, descDrawY },
            IM_COL32(128, 122, 107, 255), descText, nullptr, descW);

        ImGui::PopID();
    }

    ImGui::End();
}

// ======================
// 音频系统
// ======================

void Game::playBGM(BGMType type) {
    // 同一首 BGM 不重复播放
    if (type == m_currentBGM && m_bgmMusic && m_bgmMusic->getStatus() == sf::Music::Status::Playing)
        return;

    m_currentBGM = type;
    const std::string& path = (type == BGMType::Menu) ? m_bgmMenuPath : m_bgmGamePath;

    // 停止当前 BGM
    if (m_bgmMusic) {
        m_bgmMusic->stop();
    }

    // 创建新 Music 并播放
    auto music = std::make_unique<sf::Music>();
    if (!music->openFromFile(path)) {
        // 文件不存在则静默跳过（不阻塞游戏）
        return;
    }
    music->setLooping(true);
    updateBGMVolume();   // 设置音量后播放
    music->play();
    m_bgmMusic = std::move(music);
}

void Game::stopBGM() {
    if (m_bgmMusic) {
        m_bgmMusic->stop();
    }
}

void Game::updateBGMVolume() {
    if (m_bgmMusic) {
        // 最终音量 = 主音量 × BGM 音量，范围 [0, 100]
        float vol = std::clamp(m_masterVolume * m_bgmVolume, 0.f, 1.f) * 100.f;
        m_bgmMusic->setVolume(vol);
    }
}


