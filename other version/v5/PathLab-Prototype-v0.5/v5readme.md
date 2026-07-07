# PathLab Prototype v0.5

这是一个用于工程实训的 **C++ 路径规划算法可视化原型工程**。

当前版本已经从最小样例升级为一个小型算法实验平台骨架，并加入了大屏/全屏布局优化与 3D 地形渲染：

- C++17 + CMake + raylib
- Windows / macOS 跨平台
- 随机地图、地形地图、迷宫地图生成
- 地图编辑器：障碍、擦除、起点、终点、道路、森林、山地、水域、高度增减
- 2.5D 地形代价：每个格子有 `terrain` 和 `height`
- 算法：BFS、Dijkstra、A*、Weighted A*、Greedy Best-First
- 支持四方向 / 八方向移动切换
- 搜索过程逐帧动画、暂停、单步、时间轴拖动
- 统计面板：路径代价、扩展节点、生成节点、耗时
- Benchmark：一键比较多个算法
- 地图保存 / 读取：`maps/current.pathmap`
- Benchmark CSV 导出：`exports/benchmark.csv`
- 字体替换机制：把字体放入 `assets/fonts/ui.ttf` 或 `assets/fonts/ui.otf`
- 大屏模式：默认最大化窗口，侧边栏和底部时间轴重新加宽
- 字体缩放：根据窗口尺寸自动放大 UI 字体
- 快捷键：`F11` 切换全屏，`F10` 最大化窗口，`F5` 切换 2D / 3D

- 3D 地形渲染视图：把现有 `height` 和 `terrain` 渲染为三维地形柱
- 2D / 3D 一键切换：顶部按钮或 `F5`
- 3D 摄像机控制：右键/中键拖动旋转，滚轮缩放，`WASD` 平移，`Q/E` 升降，`R` 重置
- 3D 中同步显示算法搜索过程、当前节点、最终路径、起点和终点

---

## 1. 构建方式

### Windows + Visual Studio

先确认 Visual Studio Installer 里安装了：

- 使用 C++ 的桌面开发
- C++ CMake tools for Windows

然后：

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

---

## 2. 如果 CMake 4.x 报 raylib 的 `cmake_minimum_required` 错误

项目已经在 `CMakeLists.txt` 中加入了兼容设置：

```cmake
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Minimum CMake policy version for third-party dependencies" FORCE)
```

如果你之前配置失败过，先删除旧缓存：

```powershell
Remove-Item -Recurse -Force out\build
Remove-Item -Recurse -Force build
```

然后重新配置。

---

## 3. 字体怎么换

raylib 默认字体比较像像素字体，确实不好看。

本项目没有内置字体文件。你可以自己放一个字体文件到：

```text
assets/fonts/ui.ttf
```

或者：

```text
assets/fonts/ui.otf
```

程序启动时会自动加载它。CMake 构建后会把 `assets` 文件夹复制到可执行文件旁边。

推荐使用你自己有权使用的字体，例如：

- Noto Sans
- MiSans
- HarmonyOS Sans
- Inter
- 你电脑里已有的系统 UI 字体

不要把有版权限制的字体文件提交到公开仓库里。

---

## 4. 大屏 / 全屏模式

这个版本针对字体偏小的问题重新调整了窗口与排版：

- 默认窗口从 `1440 x 900` 提升到 `1600 x 1000`。
- 程序启动后会自动最大化。
- 左侧控制面板、右侧编辑面板、底部时间轴都加宽加高。
- 字体会根据屏幕尺寸自动缩放，最大约放大到原来的 `1.22` 倍。
- 按 `F11` 可以切换全屏 / 退出全屏。
- 按 `F10` 可以重新最大化窗口。

如果你们教室投影或者演示屏是 1080p / 2K，这个版本会比 v0.3 清楚很多。

---

## 5. 3D 地形渲染

这个版本新增了一个 **3D Terrain View**。它不是独立的一套地图数据，而是直接复用二维地图中的：

```text
terrain：Plain / Road / Forest / Mountain / Water
height：0 到 9 的高度值
obstacle：障碍状态
start / goal：起点与终点
path frame：算法动画帧
```

因此算法仍然跑在网格图上，但展示时会把每个格子渲染成一个三维地形块。高度越大，柱体越高；水域、山地、森林、道路会使用不同颜色；算法运行时，open/closed/current/path 也会同步映射到 3D 视图中。

### 切换方式

| 操作 | 功能 |
|---|---|
| 顶部 `View: 2D / View: 3D` 按钮 | 切换 2D / 3D 视图 |
| `F5` | 快速切换 2D / 3D 视图 |
| 顶部 `Reset cam` | 重置 3D 摄像机 |

### 3D 摄像机控制

| 操作 | 功能 |
|---|---|
| 右键拖动 / 中键拖动 | 旋转视角 |
| 鼠标滚轮 | 缩放距离 |
| `WASD` | 平移视角中心 |
| `Q / E` | 降低 / 抬高视角中心 |
| `R` | 重置摄像机 |

注意：当前版本的 3D 视图主要用于展示和演示。地图编辑仍然建议在 2D 视图完成，然后切换到 3D 观察效果。

---

## 6. 界面功能

### 左侧：地图与算法

| 区域 | 功能 |
|---|---|
| Map creator | 随机地图、2.5D 地形地图、迷宫地图、清空地图 |
| Rows / Columns | 调整地图尺寸 |
| Obstacle | 调整随机障碍概率 |
| Algorithms | BFS、Dijkstra、A*、Weighted A*、Greedy、Benchmark |
| Options | 八方向移动、启发式权重、地形代价权重、高度代价权重 |

### 右侧：编辑器与统计

| 区域 | 功能 |
|---|---|
| Editor | 选择当前编辑工具 |
| Animation | 播放、暂停、上一帧、下一帧、回放 |
| File / output | 保存地图、读取地图、导出 CSV |
| Statistics | 显示当前算法运行结果 |
| Benchmark | 显示多算法比较结果 |

### 底部：时间轴

可以拖动时间轴查看任意搜索步骤。

---

## 7. 地图编辑操作

| 操作 | 功能 |
|---|---|
| 左键 | 使用当前选中的编辑工具 |
| 右键 | 擦除障碍 / 水域 |
| Shift + 左键 | 设置起点 |
| Ctrl + 左键 | 设置终点 |

工具说明：

| 工具 | 作用 |
|---|---|
| Wall | 设置障碍 |
| Erase | 删除障碍 |
| Start | 设置起点 |
| Goal | 设置终点 |
| Road | 道路，代价较低 |
| Forest | 森林，代价较高 |
| Mountain | 山地，代价更高 |
| Water | 水域，不可通行 |
| Plain | 普通地形 |
| Height+ / Height- | 调整高度值，范围 0 到 9 |

---

## 8. 地形代价模型

每一步移动代价大致为：

```text
step_cost = base_distance + terrain_weight * terrain_cost + height_weight * abs(height_a - height_b)
```

其中：

| 地形 | 代价含义 |
|---|---|
| Road | 更低代价 |
| Plain | 普通代价 |
| Forest | 较高代价 |
| Mountain | 更高代价 |
| Water | 不可通行 |

因此，在地形图上运行 Dijkstra / A* 时，算法不一定选择几何距离最短的路，而是选择总代价最小的路。

---

## 8. 项目结构

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

---

## 9. 后续建议

这个版本已经足够作为工程实训的主干。后续可以继续加：

- 蚁群算法 ACO
- 模拟退火用于 TSP / 多目标路径，而不是普通单源最短路
- 搜索热力图
- 搜索树面板
- Dear ImGui 控件层
- 更正式的项目文档：需求分析、总体设计、详细设计、测试报告、用户手册

## v0.4.1 修复说明

- 修复 Visual Studio 下 `const int` 到 `float` 的列表初始化收缩转换报错。
- 将 UI 布局坐标、按钮宽高等局部变量统一改为 `float`，避免 raylib `Rectangle` 初始化时报 C2397。
