# SCUT WALK TALL 鲲鹏 ARM64 版本构建指南

## 1. 项目概述

### 1.1 项目简介

本项目是 SCUT WALK TALL 游戏的鲲鹏 ARM64 版本，专为华为鲲鹏处理器平台优化，可直接在 ARM64 架构的 Linux 服务器上编译和运行。

### 1.2 技术栈

| 组件 | 版本 | 说明 |
|------|------|------|
| SFML | 3.0.0 | 多媒体库（从源码编译） |
| ImGui | latest | 即时模式 GUI 库 |
| pugixml | latest | XML 解析库 |
| CMake | 3.20+ | 构建工具 |
| GCC | 9.4.0+ | 编译器 |

### 1.3 目录结构

```
kunpeng_release/
├── assets/              # 游戏资源文件
│   ├── sprites/         # 精灵图
│   ├── maps/            # 地图文件
│   ├── audio/           # 音频文件
│   └── frames/          # 动画帧
├── src/                 # 源代码
│   ├── game/            # 游戏逻辑
│   ├── player/          # 玩家系统
│   └── map/             # 地图系统
├── third_party/         # 第三方依赖
│   ├── imgui/           # ImGui 库
│   ├── imgui-sfml/      # ImGui-SFML 绑定
│   └── pugixml/         # XML 解析库
├── CMakeLists.txt       # CMake 构建配置
└── build.sh             # 自动化构建脚本
```

---

## 2. 环境准备

### 2.1 系统要求

- **架构**: ARM64 (aarch64)
- **操作系统**: Ubuntu 22.04 LTS 或更高版本
- **内存**: 至少 4GB
- **存储**: 至少 10GB 可用空间

### 2.2 依赖安装

```bash
# 更新系统
sudo apt-get update
sudo apt-get upgrade -y

# 安装基础依赖
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

---

## 3. 编译构建

### 3.1 自动构建（推荐）

```bash
# 进入项目目录
cd ~/kunpeng_release

# 添加执行权限
chmod +x build.sh

# 执行构建（Release 模式）
./build.sh Release
```

### 3.2 手动构建步骤

#### 3.2.1 编译 SFML 3.0.0

```bash
# 克隆 SFML 源码
cd /tmp
git clone https://github.com/SFML/SFML.git
cd SFML
git checkout 3.0.0

# 编译安装
mkdir -p build && cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=TRUE \
    -DSFML_BUILD_EXAMPLES=FALSE \
    -DSFML_BUILD_TEST_SUITE=FALSE
make -j$(nproc)
sudo make install
sudo ldconfig
```

#### 3.2.2 编译游戏

```bash
# 创建构建目录
cd ~/kunpeng_release
mkdir -p build && cd build

# 配置 CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# 编译
make -j$(nproc)
```

---

## 4. 运行游戏

### 4.1 基本运行

```bash
cd ~/kunpeng_release/build
./SCUT_WALK_TALL_KUNPENG
```

### 4.2 软件渲染模式（无 GPU 环境）

```bash
cd ~/kunpeng_release/build

# 设置软件渲染环境变量
export LIBGL_ALWAYS_SOFTWARE=1
export GALLIUM_DRIVER=llvmpipe

# 运行游戏
./SCUT_WALK_TALL_KUNPENG
```

---

## 5. 性能优化建议

### 5.1 编译优化

```bash
cd ~/kunpeng_release/build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-O3 -march=native -flto"
make -j$(nproc)
```

### 5.2 运行时优化

```bash
# 禁用垂直同步
export __GL_SYNC_TO_VBLANK=0

# 运行游戏
./SCUT_WALK_TALL_KUNPENG
```

### 5.3 硬件优化建议

| 场景 | 建议 |
|------|------|
| 纯 CPU 环境 | 使用软件渲染，降低游戏分辨率 |
| 远程连接 | 使用 X2Go 或 Moonlight 进行图形串流 |
| 追求最佳体验 | 使用带有 GPU 的云服务器实例 |

---

## 6. SFML 版本管理

### 6.1 版本查询

```bash
# 查询系统 SFML 版本
pkg-config --modversion sfml-system

# 查询编译安装的 SFML 版本
cat /usr/local/include/SFML/Config.hpp | grep SFML_VERSION
```

### 6.2 多版本共存处理

当系统同时安装了多个版本的 SFML 时，CMakeLists.txt 配置如下：

```cmake
# 强制使用 /usr/local 下的 SFML 3.0
set(CMAKE_PREFIX_PATH "/usr/local" ${CMAKE_PREFIX_PATH})
find_package(SFML 3 COMPONENTS Graphics Window System Audio REQUIRED)
```

---

## 7. 附录

### 7.1 动态库依赖检查

```bash
ldd SCUT_WALK_TALL_KUNPENG | grep sfml
```

预期输出：
```
libsfml-graphics.so.3.0 => /usr/local/lib/libsfml-graphics.so.3.0
libsfml-window.so.3.0 => /usr/local/lib/libsfml-window.so.3.0
libsfml-system.so.3.0 => /usr/local/lib/libsfml-system.so.3.0
libsfml-audio.so.3.0 => /usr/local/lib/libsfml-audio.so.3.0
```

### 7.2 SFML 3.x API 说明

| 功能 | SFML 2.x | SFML 3.x |
|------|----------|----------|
| 事件处理 | `pollEvent(event)` | `pollEvent()` 返回 optional |
| 窗口图标 | `setIcon(width, height, pixels)` | `setIcon(image)` |
| 全屏模式 | `Style::Fullscreen` | `State::Fullscreen` |
| 像素访问 | `getPixel(x, y)` | `getPixel({x, y})` |

### 7.3 参考链接

- [SFML 官方文档](https://www.sfml-dev.org/documentation/3.0/)
- [ImGui-SFML 绑定](https://github.com/SFML/imgui-sfml)
- [pugixml](https://pugixml.org/)
- [华为鲲鹏开发者社区](https://www.hikunpeng.com/)

---

**文档版本**: v1.0
**创建日期**: 2026年5月
**适用平台**: 鲲鹏 ARM64 Ubuntu 22.04 LTS