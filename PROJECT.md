# SCUT WALK TALL — 项目文件说明

> 华工·顶天立地 | C++17 + SFML 3 | 高级语言设计大作业

---

## 项目架构总览

```
SCUT_WALK_TALL/
├── src/
│   ├── main.cpp                  ← 程序入口
│   ├── game/                     ← 核心游戏层
│   │   ├── Game.h / Game.cpp     ← 主控制器（最大模块）
│   │   ├── Enemy.h / Enemy.cpp   ← 敌人体系
│   │   ├── Skill.h / Skill.cpp   ← 技能系统
│   │   └── ItemPickup.h / ItemPickup.cpp  ← 掉落道具
│   ├── player/                   ← 角色层
│   │   ├── Player.h / Player.cpp ← 角色抽象基类
│   │   ├── CSPlayer.h / CSPlayer.cpp     ← 计算机学院角色
│   │   └── ChemPlayer.h / ChemPlayer.cpp ← 化学学院角色
│   └── map/                      ← 地图层
│       ├── Map.h / Map.cpp       ← Tiled 地图加载与渲染
│       ├── MapManager.h / MapManager.cpp ← 地图管理器
│       └── Tile.h / Tile.cpp     ← 单格子数据
├── third_party/                  ← 第三方依赖（见下文）
├── assets/                       ← 游戏素材（贴图/地图/音频）
└── CMakeLists.txt                ← 构建配置
```

---

## 源文件说明

### `src/main.cpp`

程序唯一入口，代码只有 8 行：

```cpp
int main() {
    Game game;
    game.run();
}
```

实例化 `Game` 对象并启动游戏主循环，其他一切由 `Game` 接管。

---

### `src/game/Game.h` / `Game.cpp`（核心，106KB）

**游戏主控制器**，持有并协调所有子系统。主要职责：

| 职责 | 说明 |
|------|------|
| 窗口管理 | 创建 SFML 窗口（1280×720），支持全屏切换 |
| 场景状态机 | 主菜单 → 角色选择 → 游戏中 → 暂停 → 结算，各场景独立渲染/更新逻辑 |
| 角色控制 | 处理 WASD 移动、朝向判定、精灵切换、碰撞推回 |
| 敌人管理 | 定时刷怪（初始 8s，最低 3s），管理 `m_enemies` 列表，处理死亡/经验掉落 |
| 拾取物系统 | `spawnPickup()` 统一生成道具，玩家碰撞触发拾取效果（回血/加速/经验） |
| 技能系统 | `updateSkills()` / `renderSkills()` 驱动技能循环，传入玩家位置 |
| BGM 系统 | `sf::Music` 流式播放，主菜单/游戏两套 BGM，支持主音量×BGM 音量双重调节 |
| UI 系统 | ImGui 绘制设置面板（音量三通道、分辨率、全屏）、HUD（血条/经验条/计时/击杀数）、人物选择界面 |
| 过场动画 | 逐帧播放序列图（开场 + 彩蛋） |

**关键常量**（定义于 `Game.h`）：

| 常量 | 值 | 说明 |
|------|----|------|
| `DesignWidth` | 1280 | 设计宽度（px） |
| `DesignHeight` | 720 | 设计高度（px） |
| `TILE_RENDER_SIZE` | 64 | 每格渲染尺寸（px） |

---

### `src/game/Enemy.h` / `Enemy.cpp`（10KB / 20KB）

**敌人体系**，采用继承多态设计：

#### 基类 `Enemy`

提供所有敌人共有的接口：

| 方法 | 说明 |
|------|------|
| `update(dt, playerPos)` | 纯虚，子类实现 AI 行为 |
| `render(window)` | 纯虚，子类实现渲染 |
| `isDead()` | `m_hp <= 0` |
| `getPendingDamage(dt)` | 返回本帧对玩家造成的伤害（接触伤害类用） |
| `hasDroppedExp()` / `markExpDropped()` | 防止重复掉落经验 |
| `getExpValue()` | 返回该敌人死亡应给的经验值 |

#### 三种敌人

| 类名 | 游戏内名称 | 特点 | HP |
|------|-----------|------|----|
| `DashEnemy` | 教务系统服务器机柜 | 蓄力冲刺，穿墙撞击，有左右朝向贴图 | 60 |
| `BlueScreenEnemy` | 蓝屏怪 | 接触叠层，10 层触发 1.5s 眩晕，DPS=8，无视碰撞穿墙 | 自定义 |
| `ThiefEnemy` | 外卖小偷 | 靠近偷走经验，死亡时归还；需击杀才能回收经验 | 自定义 |

AI 随机性由静态 `std::mt19937` 引擎驱动，保证随机 seed 不重复。

---

### `src/game/Skill.h` / `Skill.cpp`（11KB / 36KB）

**技能系统**，抽象基类 + 多派生实现：

#### 基类 `Skill`

```cpp
virtual void update(float dt, const sf::Vector2f& playerPos, Game* gameCtx) = 0;
virtual void render(sf::RenderWindow& window, const sf::Vector2f& playerPos) = 0;
```

持有 `Game*` 上下文指针，可访问敌人列表、玩家状态等游戏数据。

#### 已实现技能

| 技能类 | 内部名 | 游戏名 | CD | 范围 | 伤害 |
|--------|--------|--------|----|------|------|
| `PointerStormSkill` | `pointer_storm` | 底层·指针风暴（CS专属） | 2s | 250px | 30 |
| `MagicCircleSkill` | `magic_circle` | 编译优化·校徽法阵（CS专属） | — | — | — |
| `KoiShieldSkill` | `koi_shield` | 华工锦鲤·守护环绕（CS专属） | — | — | — |
| `ExothermicStormSkill` | `exothermic_storm` | 放热反应·试剂风暴（化学专属） | — | — | — |
| `ChemCircleSkill` | `chem_circle` | 催化循环·分子法阵（化学专属） | — | — | — |
| `ChemOrbitSkill` | `chem_orbit` | 炼金护盾·烬焰绕行（化学专属） | — | — | — |

技能由 `Game::initSkills()` 按选中角色分支加载，存入 `m_skills`（`vector<unique_ptr<Skill>>`）。

---

### `src/game/ItemPickup.h` / `ItemPickup.cpp`（1.5KB / 1.3KB）

**掉落道具**，管理地图上的可拾取物品：

| 类型 (`PickupType`) | 游戏内名称 | 效果 |
|--------------------|-----------|------|
| `Heal` | 华农牛奶 | 回复血量 |
| `Speed` | 加速道具 | 临时提升移动速度 |
| `Exp` | 经验道具 | 拾取后给玩家经验值 |

特性：
- **浮动动画**：`sin(age * 3.0f) * 3px` 上下 bob 效果
- **自动消失**：存在超过 30 秒自动销毁（`m_lifetime = 30.0f`）
- 经验道具携带 `m_expValue`，由 `Game::spawnPickup(expValue)` 传入

---

### `src/player/Player.h` / `Player.cpp`（1.9KB / 2KB）

**角色抽象基类**，定义所有角色共有的数据与逻辑：

**属性**：HP、MaxHP、攻击、防御、速度、暴击率、等级、经验、代表色、位置

**经验升级公式**（`addExp()`）：

```
每次升级：
  m_level++
  m_maxHp  += 10，并回满血
  m_attack += 2
  m_defense += 1
  m_expToNext *= 1.35f（指数增长）
```

**子类必须实现**：`init()`、`update(dt)`、`attack()`

---

### `src/player/CSPlayer.h` / `CSPlayer.cpp`（408B / 608B）

**计算机学院学生**，快攻型：

| 属性 | 初始值 |
|------|--------|
| HP | 80 |
| 攻击 | 15 |
| 防御 | 4 |
| 速度 | **200**（最快） |
| 暴击率 | 20% |
| 代表色 | 科技蓝 `(60, 140, 255)` |

---

### `src/player/ChemPlayer.h` / `ChemPlayer.cpp`（402B / 604B）

**化学学院学生**，坦克型：

| 属性 | 初始值 |
|------|--------|
| HP | **140**（最厚） |
| 攻击 | 10 |
| 防御 | **10**（最高） |
| 速度 | 140（稍慢） |
| 暴击率 | 5% |
| 代表色 | 化学绿 `(50, 200, 100)` |

---

### `src/map/Map.h` / `Map.cpp`（2.9KB / 15.6KB）

**Tiled 地图加载与渲染**：

- 使用 **pugixml** 解析 Tiled 导出的 `.tmx` XML 格式
- 支持**多图层**渲染（普通图层 + `overlay` 覆盖图层，overlay 画在角色之上）
- 支持**多 Tileset**（通过 `firstGid` 映射到正确纹理）
- **碰撞检测**：矩形（`CollisionRect`）和多边形（`CollisionPolygon`）两种碰撞体
- 从 Tiled 对象层解析**出生点**（`m_spawnPoint`）和**出口**（`m_exitPoint`）

**关键结构体**：

| 结构体 | 用途 |
|--------|------|
| `CollisionRect` | 矩形碰撞区（x, y, width, height） |
| `CollisionPolygon` | 多边形碰撞体（顶点列表，世界坐标） |
| `TilesetInfo` | Tileset 元信息（firstGid / 纹理 / 行列数） |

---

### `src/map/MapManager.h` / `MapManager.cpp`（448B / 1.7KB）

**地图管理器**，统一管理多张地图：

- 启动时用 `std::filesystem` 扫描 `assets/maps/` 下所有 `.tmx` 文件，**按文件名排序**后依次加载
- 提供 `getCurrentMap()` / `switchMap(name)` / `getMapCount()` 接口
- 内部用 `vector<unique_ptr<Map>>` + 当前索引维护地图列表

---

### `src/map/Tile.h` / `Tile.cpp`（725B / 704B）

**最小单元**，表示地图上的一个格子：

| `TileType` 枚举值 | 含义 | 可行走 |
|------------------|------|--------|
| `Floor` | 地板 | ✅ |
| `Wall` | 墙壁 | ❌ |
| `Obstacle` | 障碍物 | ❌ |
| `Spawn` | 出生点 | ✅ |
| `Exit` | 出口 | ❌ |

每个 Tile 记录其在 Tileset 中的纹理索引坐标（`m_texTileX`, `m_texTileY`），由 `Map` 在渲染时用于切分 Sprite。

---

## 第三方依赖

| 库 | 版本 | 位置 | 引入方式 | 作用 |
|----|------|------|----------|------|
| **SFML 3** | 3.x | `third_party/SFML/` | 预编译 + `find_package` | 窗口/渲染/音频/系统基础框架 |
| **ImGui** | 最新 | `third_party/imgui/` | 直接编译源文件（7个 .cpp） | 游戏内即时模式 GUI（设置面板/HUD/选角界面） |
| **imgui-sfml** | 最新 | `third_party/imgui-sfml/` | 编译 `imgui-SFML.cpp` | ImGui 的 SFML 渲染绑定层 |
| **pugixml** | 最新 | `third_party/pugixml/` | 纯头文件模式（`PUGIXML_HEADER_ONLY`） | 轻量 XML 解析器，读取 Tiled `.tmx` 地图文件 |

### SFML 使用的四个模块

| 模块 | 链接目标 | 主要用途 |
|------|---------|---------|
| `SFML::Graphics` | sfml-graphics.lib | 渲染窗口、精灵、纹理、形状、视图 |
| `SFML::Window` | sfml-window.lib | 窗口创建、事件处理、输入 |
| `SFML::System` | sfml-system.lib | 时钟（`sf::Clock`）、向量（`sf::Vector2f`） |
| `SFML::Audio` | sfml-audio.lib | BGM 流式播放（`sf::Music`） |

SFML 内部还依赖 **FLAC**、**Ogg**、**Vorbis**、**FreeType** 等库，均已预编译进 `third_party/SFML/lib/`，Post-Build 步骤自动将所有 DLL 复制到输出目录。

---

## 构建说明

```bash
# 配置（首次或 CMakeLists 改变后运行）
cmake -S . -B build

# 编译 Release
cmake --build build --config Release

# 可执行文件输出到
build/Release/SCUT_WALK_TALL.exe
```

**注意**：
- 需要 CMake ≥ 3.20、MSVC（Visual Studio 2022 推荐）
- C++ 标准：C++17（`std::filesystem`、`std::optional`、结构化绑定均有用到）
- Post-Build 自动将 `assets/` 和 SFML DLL 复制到 `build/Release/`，直接双击 `.exe` 即可运行

---

## BGM 素材规范

| 用途 | 路径 | 格式 |
|------|------|------|
| 主菜单背景音乐 | `assets/audio/BGM/bgm_menu.ogg` | OGG / WAV / FLAC |
| 游戏中背景音乐 | `assets/audio/BGM/bgm_gameplay.ogg` | OGG / WAV / FLAC |

音量由设置面板中「主音量」×「背景音乐」两个滑块共同控制（`0–100%`），实时生效。
