# SCUT WALK TALL

> 华南理工大学主题的 2D 动作角色扮演游戏

## 🎮 游戏简介

SCUT WALK TALL 是一款以华南理工大学为背景的 2D ARPG 游戏。玩家将扮演华工学子，在充满挑战的校园世界中战斗、成长，战胜各种"校园敌人"。

## ✨ 功能特性

### 游戏玩法
- **双角色选择**：选择 CS（计算机科学）或 Chem（化学）学生角色，各有独特技能树
- **技能系统**：每个角色拥有 3 个独特技能，支持升级强化
- **敌人类型**：
  - 🖥️ DashEnemy - 教务系统服务器（蓄力冲撞攻击）
  - 💙 BlueScreenEnemy - 蓝屏错误窗口（叠层眩晕）
  - 🦹 ThiefEnemy - 外卖小偷（偷取经验后逃跑）
- **等级系统**：击杀敌人获取经验，升级提升属性
- **道具系统**：经验道具、回血道具、加速道具

### 技术特性
- **SFML 3.0**：高性能 2D 图形渲染
- **ImGui**：现代化 UI 界面
- **Tiled Map**：支持 TMX 地图格式
- **状态机 AI**：智能敌人寻敌逻辑
- **碰撞检测**：AABB 和多边形碰撞

## 🛠️ 技术栈

| 组件 | 技术 | 版本 |
|------|------|------|
| 游戏引擎 | SFML | 3.0.2 |
| UI 库 | ImGui | 1.xx |
| 地图解析 | pugixml | - |
| 构建工具 | CMake | 3.20+ |

## 📦 编译和运行

### 环境要求

- **Windows**：Visual Studio 2022+
- **CMake**：3.20 或更高版本

### 编译步骤

```powershell
# 克隆仓库
git clone https://github.com/SUNMOONHD/SCUT_WALK_TALL-.git
cd SCUT_WALK_TALL-

# 配置项目
cmake -S . -B build

# 编译
cmake --build build --config Release
```

### 运行游戏

编译完成后，在 `build/Release` 目录下找到 `SCUT_WALK_TALL.exe` 运行即可。

## 🎯 游戏玩法

### 操作说明

| 按键 | 功能 |
|------|------|
| `W/A/S/D` | 移动 |
| `J` | 普通攻击 |
| `K` | 技能 1 |
| `L` | 技能 2 |
| `;` | 技能 3 |
| `Space` | 闪避 |
| `Esc` | 暂停菜单 |

### 角色介绍

#### CS 学生（计算机科学）
- **技能 1**：指针风暴 - 周期性释放指针弹幕，攻击范围内最近的多个敌人
- **技能 2**：校徽法阵 - 双层法阵围绕玩家，内圈校徽固定，外圈法环旋转，范围内敌人持续受到伤害
- **技能 3**：锦鲤护盾 - 锦鲤环绕玩家游动，碰撞敌人时造成伤害后消失，随后重生继续环绕

#### Chem 学生（化学）
- **技能 1**：放热反应 - 释放火焰风暴，持续伤害范围内敌人
- **技能 2**：分子法阵 - 化学分子结构法阵，范围内敌人持续受到伤害
- **技能 3**：炼金环绕 - 炼金粒子环绕玩家，碰撞敌人造成伤害后重生

## 📁 项目结构

```
SCUT_WALK_TALL/
├── src/                    # 源代码目录
│   ├── game/              # 游戏核心逻辑
│   │   ├── Game.cpp/h     # 游戏主控制器
│   │   ├── Enemy.cpp/h    # 敌人系统
│   │   ├── Skill.cpp/h    # 技能系统
│   │   └── ItemPickup.cpp/h # 道具系统
│   ├── map/               # 地图系统
│   │   ├── Map.cpp/h      # 地图管理
│   │   └── MapManager.cpp/h # 地图加载
│   ├── player/            # 角色系统
│   │   ├── Player.cpp/h   # 角色基类
│   │   ├── CSPlayer.cpp/h # CS 角色
│   │   └── ChemPlayer.cpp/h # Chem 角色
│   └── main.cpp           # 程序入口
├── assets/                # 资源文件
│   ├── audio/             # 音频文件
│   ├── fonts/             # 字体文件
│   ├── images/            # 图片资源
│   ├── maps/              # 地图文件
│   ├── skills/            # 技能特效
│   └── sprites/           # 精灵图
├── third_party/           # 第三方库
│   ├── SFML/              # SFML 库
│   ├── imgui/             # ImGui 库
│   └── pugixml/           # XML 解析库
└── CMakeLists.txt         # CMake 配置
```

## 🤝 贡献指南

欢迎提交 Issue 和 Pull Request！

### 开发规范

1. 代码风格：遵循 Google C++ 编码规范
2. 注释：重要函数和类需要添加 Doxygen 风格注释
3. 提交信息：使用清晰的提交信息，格式如下：
   - `feat: 添加新功能`
   - `fix: 修复 bug`
   - `docs: 更新文档`
   - `refactor: 代码重构`

## 📄 许可证

MIT License - 详见 LICENSE 文件

## 📧 联系方式

如有问题或建议，请通过以下方式联系：
- GitHub Issues: D893028193@outlook.com

---

**Go SCUT! 🏃♂️💨**
