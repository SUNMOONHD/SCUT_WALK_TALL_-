# SCUT WALK TALL - 鲲鹏 ARM64 版本

<div align="center">

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-ARM64%20%7C%20Linux-green.svg)
![SFML](https://img.shields.io/badge/SFML-3.0.0-orange)
![C++](https://img.shields.io/badge/C%2B%2B-17-yellow.svg)

**一款基于 SFML 开发的华南理工大学校园主题弹幕射击游戏**

*专为华为鲲鹏 ARM64 架构优化*

</div>

---

## 📖 项目简介

SCUT WALK TALL 是一款以华南理工大学校园为主题的弹幕射击游戏，玩家操控角色在校园场景中对抗各种敌人。游戏采用 C++ 和 SFML 3.0 开发，本版本针对华为鲲鹏 ARM64 架构进行了专门优化。

### 🎮 游戏特色

- **校园主题**：以华南理工大学校园为背景
- **多种角色**：可选计算机系或化学系学生角色
- **丰富敌人**：教务系统服务器、蓝屏怪、外卖小偷等多种敌人类型
- **技能系统**：多种技能特效，包括化学圈、吸热风暴、锦鲤护盾等
- **像素风格**：复古像素艺术风格

---

## 🖥️ 支持平台

| 平台 | 架构 | 状态 | 说明 |
|------|------|------|------|
| Linux (Ubuntu 22.04+) | ARM64 (鲲鹏) | ✅ 已测试 | 本版本专门优化 |
| Linux | x86_64 | 🔄 兼容 | 需使用相同 SFML 3.0 版本 |
| Windows | x86_64 | 🔄 兼容 | 参考原项目 |

---

## 🚀 快速开始

### 环境要求

- **操作系统**: Ubuntu 22.04 LTS 或更高版本 (ARM64)
- **处理器**: ARM64 架构 (鲲鹏处理器)
- **内存**: 至少 4GB
- **存储**: 至少 2GB 可用空间

### 安装依赖

```bash
# 更新系统
sudo apt-get update
sudo apt-get upgrade -y

# 安装编译依赖
sudo apt-get install -y \
    cmake \
    g++ \
    pkg-config \
    libudev1 \
    libudev-dev \
    libgl1-mesa-dev \
    libopenal-dev \
    libflac-dev \
    libvorbis-dev \
    libogg-dev \
    git
```

### 编译构建

```bash
# 克隆本仓库
git clone https://github.com/SUNMOONHD/SCUT_WALK_TALL_-.git
cd SCUT_WALK_TALL_-

# 执行自动构建脚本
chmod +x build.sh
./build.sh Release
```

### 运行游戏

```bash
cd build
./SCUT_WALK_TALL_KUNPENG
```

---

## 📁 项目结构

```
kunpeng_release/
├── assets/                    # 游戏资源
│   ├── audio/                # 音频文件
│   │   └── BGM/            # 背景音乐
│   ├── fonts/               # 字体文件
│   ├── frames/              # 动画帧
│   ├── images/              # 角色立绘
│   ├── maps/                # 地图文件 (.tmx)
│   ├── skills/              # 技能特效
│   └── sprites/             # 精灵图
│       ├── characters/      # 角色动画
│       ├── enemies/         # 敌人精灵
│       ├── items/           # 道具图标
│       └── ui/              # UI 素材
├── src/                      # 源代码
│   ├── game/               # 游戏核心逻辑
│   ├── map/                # 地图系统
│   └── player/             # 玩家系统
├── third_party/             # 第三方库
│   ├── imgui/             # ImGui
│   ├── imgui-sfml/        # ImGui-SFML 绑定
│   └── pugixml/           # pugixml
├── CMakeLists.txt           # CMake 构建配置
├── build.sh                 # 自动构建脚本
├── BUILD_GUIDE.md          # 详细构建指南
└── README.md               # 项目说明文档
```

---

## 🔧 技术栈

| 组件 | 版本 | 用途 | 许可证 |
|------|------|------|--------|
| SFML | 3.0.0 | 多媒体库 | zlib/png |
| ImGui | latest | 即时模式 GUI | MIT |
| pugixml | latest | XML 解析 | MIT |
| CMake | 3.20+ | 构建系统 | BSD-3 |

---

## 📖 文档

- [构建指南 (BUILD_GUIDE.md)](BUILD_GUIDE.md) - 详细的编译构建说明

---

## ⚠️ 注意事项

### 性能说明

- **无 GPU 环境**：服务器如果没有 GPU，游戏会使用软件渲染，性能可能受限
- **音频设备**：服务器环境通常没有音频设备，游戏会静音运行

### 性能优化建议

```bash
# 使用软件渲染模式
export LIBGL_ALWAYS_SOFTWARE=1
export GALLIUM_DRIVER=llvmpipe
./SCUT_WALK_TALL_KUNPENG
```

---

## 📝 许可证

本项目继承原项目的许可证协议。请参考 [LICENSE](LICENSE) 文件。

第三方库许可证：
- **SFML**: zlib/png license
- **ImGui**: MIT License
- **pugixml**: MIT License

---

## 🙏 致谢

- [SFML](https://www.sfml-dev.org/) - 优秀的 C++ 多媒体库
- [ImGui](https://github.com/ocornut/imgui) - 即时模式 GUI 库
- [ImGui-SFML](https://github.com/SFML/imgui-sfml) - ImGui 与 SFML 的绑定
- [pugixml](https://pugixml.org/) - 轻量级 XML 解析库
- [华为鲲鹏开发者社区](https://www.hikunpeng.com/) - ARM64 平台支持

---

## 📧 联系方式

- **GitHub Issues**: [提交问题](https://github.com/SUNMOONHD/SCUT_WALK_TALL_-/issues)
- **Pull Requests**: 欢迎提交 Pull Request

---

<div align="center">

**Made with ❤️ for Huawei Kunpeng**

</div>