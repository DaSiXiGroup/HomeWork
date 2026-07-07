# PathLab Prototype v0.6

这是一个用于工程实训的 **C++ 路径规划算法可视化原型工程**。当前版本已经具备 2D/3D 地图显示、地图编辑、随机地图生成、路径规划动画、统计面板、Benchmark 与地图保存读取。

## 主要功能

- C++17 + CMake + raylib
- Windows / macOS 跨平台
- 随机地图、地形地图、迷宫地图生成
- 地图编辑器：选择、障碍、擦除、起点、终点、道路、森林、山地、水域、高度增减
- 算法：BFS、Dijkstra、A*、Weighted A*、Greedy Best-First
- 四方向 / 八方向移动切换
- 搜索过程逐帧动画、暂停、单步、时间轴拖动
- 统计面板：路径代价、扩展节点、生成节点、耗时
- Benchmark：一键比较多个算法
- 地图保存 / 读取：`maps/current.pathmap`
- Benchmark CSV 导出：`exports/benchmark.csv`
- 字体替换：把字体放入 `assets/fonts/ui.ttf` 或 `assets/fonts/ui.otf`
- 大屏模式：默认最大化窗口，`F11` 全屏，`F10` 最大化
- 2D / 3D 一键切换：顶部按钮或 `F5`
- 3D 摄像机控制：右键/中键拖动旋转，滚轮缩放，`WASD` 平移，`Q/E` 升降，`R` 重置
- 3D 鼠标拾取：通过 `GetMouseRay + GetRayCollisionBox` 射线检测选中地块
- 3D 视图中可直接编辑地块：左键选择或应用当前工具
- 3D 中同步显示算法搜索过程、当前节点、最终路径、起点和终点

## 构建方式

### Windows + Visual Studio

先确认 Visual Studio Installer 里安装了：

- 使用 C++ 的桌面开发
- C++ CMake tools for Windows

然后在项目根目录运行：

```bash
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

运行：

```bash
./build/Release/PathLabSample.exe
```

也可以在 Visual Studio 中：

```text
文件 → 打开 → 文件夹 → 选择 PathLab-Sample 文件夹
```

注意：不要只打开 `main.cpp`，也不要打开 `src` 文件夹。

### macOS

```bash
cmake -S . -B build
cmake --build build
./build/PathLabSample
```

如果使用 Homebrew 的 raylib：

```bash
brew install raylib
cmake -S . -B build -DUSE_SYSTEM_RAYLIB=ON
cmake --build build
./build/PathLabSample
```

## 操作说明

### 全局快捷键

| 操作 | 功能 |
|---|---|
| `F5` | 切换 2D / 3D 视图 |
| `F10` | 最大化窗口 |
| `F11` | 全屏 / 退出全屏 |

### 2D 编辑

| 操作 | 功能 |
|---|---|
| 左键拖动 | 使用当前工具绘制 |
| 右键拖动 | 擦除障碍 / 水域 |
| `Shift + 左键` | 设置起点 |
| `Ctrl + 左键` | 设置终点 |

### 3D 编辑与相机

| 操作 | 功能 |
|---|---|
| 左键点击 / 拖动 | 射线拾取地块，并使用当前工具编辑 |
| 右键拖动 / 中键拖动 | 旋转视角 |
| 鼠标滚轮 | 缩放 |
| `WASD` | 平移视角中心 |
| `Q / E` | 降低 / 抬高视角中心 |
| `R` | 重置摄像机 |
| `Shift + 左键` | 在 3D 中设置起点 |
| `Ctrl + 左键` | 在 3D 中设置终点 |

## v0.6 地形设计说明

本版本把“海拔”和“地形种类”明确分开：

- `height`：只表示几何高度，用于 3D 地形柱高度和坡度代价；
- `terrain`：表示通行语义，不自动决定海拔。

因此，低海拔也可以是山地，高海拔也可以是道路。算法中的单步代价大致为：

```text
step cost = base distance × terrain multiplier
          + terrain penalty × terrain weight
          + |height difference| × height weight × slope multiplier
```

当前地形规则：

| 地形 | 算法含义 | 视觉特征 |
|---|---|---|
| Plain | 普通地面，基准代价 | 普通地形块 |
| Road | 移动更快，坡度惩罚更小 | 地块表面有道路铺装 |
| Forest | 可通行但阻力较高 | 地块上有树 |
| Mountain | 粗糙地表，移动和坡度惩罚更高 | 地块上有岩石/山峰标记 |
| Water | 当前模型中不可通行 | 地块表面有水面覆盖 |

这个设计比“山地 = 高海拔”更合理，因为地形是材料/环境属性，海拔是几何属性，二者不应该绑定。

## 字体替换

项目会自动尝试加载：

```text
assets/fonts/ui.ttf
assets/fonts/ui.otf
```

你可以把自己有权使用的字体放到这个位置。项目不会直接附带商业字体文件。

## 项目结构

```text
PathLab-Sample/
├── CMakeLists.txt
├── README.md
├── assets/
│   └── fonts/
│       └── README.md
├── maps/
├── exports/
└── src/
    ├── main.cpp
    ├── App.h
    ├── App.cpp
    ├── GridMap.h
    ├── GridMap.cpp
    ├── Pathfinder.h
    └── Pathfinder.cpp
```

## 后续建议

- 加入蚁群算法 ACO
- 把模拟退火用于 TSP / 多目标路径，而不是普通单源最短路
- 搜索热力图
- 搜索树面板
- 更正式的项目文档：需求分析、总体设计、详细设计、测试报告、用户手册
