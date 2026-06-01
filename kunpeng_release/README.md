# SCUT WALK TALL - 鲲鹏 ARM64 版本

<div align="center">

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-ARM64%20%7C%20Linux-green.svg)
![SFML](https://img.shields.io/badge/SFML-3.0.0-orange)
![C++](https://img.shields.io/badge/C%2B%2B-17-yellow.svg)

**一款基于 SFML 开发的华南理工大学校园主题弹幕射击游戏**

*专为华为鲲鹏 ARM64 架构优化*

**[鲲鹏版本详细文档](./kunpeng_release/README.md)**

</div>

---

## 📖 项目简介

本仓库包含 SCUT WALK TALL 游戏的**鲲鹏 ARM64 版本**。

如需查看详细的构建指南和项目文档，请访问：[鲲鹏版本文档](./kunpeng_release/README.md)

---

## 🚀 快速开始

```bash
# 克隆仓库
git clone https://github.com/SUNMOONHD/SCUT_WALK_TALL_-.git
cd SCUT_WALK_TALL_-

# 进入鲲鹏版本目录
cd kunpeng_release

# 编译构建
chmod +x build.sh
./build.sh Release

# 运行游戏
cd build
./SCUT_WALK_TALL_KUNPENG
```

---

## 📁 项目结构

```
SCUT_WALK_TALL_-/
└── kunpeng_release/           # 鲲鹏 ARM64 版本
    ├── assets/                 # 游戏资源
    ├── src/                    # 源代码
    ├── third_party/            # 第三方库
    ├── CMakeLists.txt          # CMake 配置
    ├── build.sh                # 构建脚本
    ├── README.md               # 项目文档
    └── BUILD_GUIDE.md          # 构建指南
```

---

## 🙏 致谢

- [SFML](https://www.sfml-dev.org/) - C++ 多媒体库
- [ImGui](https://github.com/ocornut/imgui) - 即时模式 GUI
- [华为鲲鹏开发者社区](https://www.hikunpeng.com/)

---

<div align="center">

**Made with ❤️ for Huawei Kunpeng**

</div>