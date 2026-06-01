# MetroSim - 地铁站客流仿真系统

基于 C++/Qt6 的地铁站内行人微观仿真引擎，支持站厅拓扑建模、乘客智能路径规划、实时可视化监控与多维数据导出。

## 功能特性

### 站厅拓扑建模
- 支持 11 种节点类型：入口、出口、安检区、售票区、闸机、走廊、大厅、楼梯、扶梯、站台、候车区
- 基于图结构定义节点属性（容量、宽度、楼层）与边属性（长度、通行能力、换乘时间）
- JSON 格式的站点配置文件，可灵活扩展

### 乘客智能仿真
- **状态机模型**：Enter → Security → Ticket → Wait → Board → Exit，完整覆盖乘客进站全流程
- **独立 Agent**：每位乘客拥有独立的行走速度、耐心值、熟悉度等属性
- **高峰时段**：支持早晚高峰的双峰客流模型，可配置到达率与突发客流

### 智能路径规划
- **A\* 寻路算法**：支持五种路径目标策略
  - 最短时间优先
  - 最短距离优先
  - 最小拥堵优先
  - 最少区域切换
  - 加权综合
- **Pareto 前沿计算**：多目标路径方案对比
- **路径缓存**：加速重复查询，提升仿真性能

### 实时可视化
- **拓扑视图**：实时展示站厅节点/边的拥堵热度
- **热力图**：基于节点密度的颜色映射
- **3D 站厅视图**：基于 OpenGL 的三维站厅渲染，支持分层浏览
- **实时仪表盘**：活跃乘客数、完成/超时人数、拥堵事件数、平均通行时间

### 数据分析与导出
- **统计面板**：历史趋势曲线、节点密度排行、事件日志
- **结果导出**：JSON 摘要报告 + CSV 事件明细
- **HTML 报告**：生成可视化步骤报告

## 技术栈

| 类别 | 技术 |
|------|------|
| 语言 | C++17 |
| GUI 框架 | Qt 6 (Widgets, OpenGL, SVG) |
| 构建工具 | CMake 3.20+ |
| 编译器 | MSVC 2022 (Visual Studio) |
| 第三方库 | nlohmann/json, QCustomPlot, QtWaitingSpinner, QtNodes |

## 项目结构

```
metro-simulation/
├── data/
│   ├── params/          # 仿真参数配置 (JSON)
│   └── stations/        # 站点拓扑定义 (JSON)
├── external/
│   └── nodeeditor/      # 第三方 QtNodes 节点编辑器库
├── include/
│   ├── core/            # 核心模块头文件
│   └── thirdparty/      # 第三方库头文件
├── resources/
│   ├── diagrams/        # 流程图资源
│   ├── icons/           # 图标资源 (SVG)
│   ├── schematics/      # 示意图资源
│   └── ui/              # UI 资源 (图标、启动画面)
├── src/
│   ├── core/            # 核心模块源码
│   └── thirdparty/      # 第三方库源码
├── output/              # 构建输出 (已忽略)
├── CMakeLists.txt       # 主构建配置
└── .gitignore
```

## 核心模块

| 模块 | 文件 | 说明 |
|------|------|------|
| 站厅图模型 | `metro_graph.h/cpp` | 地铁站拓扑的节点-边图数据结构 |
| 乘客模型 | `passenger.h/cpp` | 乘客 Agent 的状态与属性定义 |
| 仿真引擎 | `simulation.h/cpp` | 离散时间步仿真核心逻辑 |
| 路径规划 | `path_planner.h/cpp` | A\* 寻路 & Pareto 前沿计算 |
| 统计模块 | `statistics.h/cpp` | 仿真指标实时统计 |
| 可视化 | `visualization.h/cpp` | 拓扑图/热力图/数据面板渲染 |
| 3D 视图 | `station_3d_view.h/cpp` | OpenGL 三维站厅渲染 |
| 站点编辑器 | `station_editor.h/cpp` | 基于 QtNodes 的可视化站厅编辑 |
| 结果导出 | `result_export.h/cpp` | JSON/CSV 格式结果输出 |
| 报告生成 | `report_writer.h/cpp` | HTML 仿真报告生成 |

## 构建指南

### 环境要求

- **Windows** 10/11 x64
- **Visual Studio 2022** (Community 或更高版本)
- **Qt 6.5+** (MSVC 2022 64-bit)
- **CMake 3.20+**

### 一键构建

编辑 `build_and_deploy.bat` 中的路径配置：

```batch
set "QT_DIR=D:\yingyong\qt\6.10.3\msvc2022_64"
set "CMAKE_DIR=D:\yingyong\qt\Tools\CMake_64\bin"
set "VS_DIR=D:\Program Files\Microsoft Visual Studio\2022\Community"
```

双击运行 `build_and_deploy.bat` 即可自动完成 CMake 配置、编译与部署。

### 手动构建

```powershell
# 配置
cmake -G "Visual Studio 17 2022" -A x64 ^
  -S metro-simulation -B metro-simulation/build_release ^
  -DCMAKE_PREFIX_PATH="D:/Qt/6.x.x/msvc2022_64"

# 编译
cmake --build metro-simulation/build_release --config Release

# 部署
windeployqt metro-simulation/build_release/bin/metro_sim.exe
```

## 使用说明

### 启动程序

```powershell
# 使用默认站点和参数
metro_sim.exe

# 指定自定义站点和参数
metro_sim.exe data/stations/my_station.json data/params/my_params.json
```

### 站点配置

站点使用 JSON 格式定义，包含节点和边两类元素：

**节点 (node)** 字段：
- `id` - 唯一标识符
- `name` - 显示名称
- `type` - 节点类型（`entrance`, `security`, `ticket`, `gate`, `corridor`, `hall`, `stairs`, `escalator`, `platform`, `exit`, `waiting`）
- `floor` - 所在楼层
- `x`, `y` - 平面坐标
- `capacity` - 容量上限
- `width` - 通道宽度

**边 (edge)** 字段：
- `from`, `to` - 起止节点 ID
- `length` - 路径长度
- `width` - 通道宽度
- `capacity` - 通行能力
- `transfer_time` - 通过时间
- `line_index` - 线路编号 (用于换乘站)
- `bidirectional` - 是否双向

### 仿真参数

```json
{
  "time_step": 1,
  "peak_lambda": 200,
  "offpeak_lambda": 120,
  "peak_multiplier": 4.0,
  "peak_hours": [[7, 9], [17, 19]],
  "max_patience": 300,
  "base_speed": 1.2,
  "congestion_threshold": 0.7,
  "congestion_k": 0.6,
  "security_time": 4,
  "ticket_time_base": 8,
  "gate_time": 1,
  "boarding_time": 3,
  "train_headway": 40,
  "train_capacity": 200
}
```

### 可视化编辑器

程序内置基于 QtNodes 的可视化站点编辑器，支持：
- 拖拽式节点增删
- 节点属性面板编辑
- 连接线（边）的创建与编辑
- JSON 文件的导入/导出

## 许可

本项目基于 GPLv3 许可证开源。使用到的第三方库遵循各自许可证：
- **nlohmann/json** - MIT License
- **QCustomPlot** - GPLv3
- **QtWaitingSpinner** - MIT License
- **QtNodes** - MIT License

## 作者

华南理工大学 (SCUT) - SUNMOONHD

---

> *MetroSim - 让地铁站客流管理更智能*