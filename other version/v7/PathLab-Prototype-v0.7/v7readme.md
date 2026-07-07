# PathLab Prototype v0.7

这是一个用于工程实训的 **C++ 路径规划算法可视化原型工程**。当前版本已经从“3D 方块地形”升级为“连续地形视觉层 + 离散算法图”的结构：用户看到的是连续起伏的地形、弯曲道路、河流和森林，但算法底层仍然可以用 BFS、Dijkstra、A* 等图搜索方法求解。

## v0.7 新增内容

- 3D 连续地形渲染：用三角面片连接高度场，不再只能显示一个个方块柱
- 方块地形 / 连续地形一键切换
- 2D/3D 路径平滑显示：Catmull-Rom 曲线让最终路径更像真实路线
- 算法图层显示开关：可把底层离散图叠加显示在连续地形上，方便向老师解释“视觉连续，算法离散”
- 连续地形生成器重做：生成更自然的山丘、山脉、河流、道路、森林
- 多角色代价函数：行人 / 汽车 / 越野车 / 无人机
- 同一张地图、同一起终点，不同角色会得到不同路线
- 地形类型和海拔继续解耦：海拔负责几何高度，地形负责通行语义

## 主要功能

- C++17 + CMake + raylib
- Windows / macOS 跨平台
- 随机地图、连续地形地图、迷宫地图生成
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

## v0.7 核心设计思想

系统采用三层结构：

```text
视觉层：连续地形、道路、河流、森林、平滑路径
        ↓
算法层：网格图 / 离散节点 / 邻接边 / 权值
        ↓
解释层：搜索动画、Open/Closed 集合、路径代价、Benchmark
```

也就是说，程序不是直接做数学意义上的完全连续路径规划，而是先做一个更合理、更可演示的工程结构：

> **地图看起来连续，但算法仍然运行在可解释、可测试、可比较的离散图上。**

这个设计很适合工程实训，因为既能保留 Dijkstra / A* 的教学核心，又能让演示效果更接近真实地图系统。

## 多角色代价函数

同一张地图上，不同角色对“最短路径”的理解不同。

| 角色 | 特点 |
|---|---|
| Pedestrian | 行人，能走普通地面、森林、山地，但水域不可通行；道路更快，爬坡有惩罚 |
| Car | 汽车，极度偏好道路；普通地面代价高，森林和山地代价非常高，水域不可通行 |
| Offroad | 越野车，能较好通过森林和山地，坡度惩罚较低 |
| Drone | 无人机，近似忽略地面地形和障碍，只按空中距离规划 |

因此，点击左侧的角色按钮后，重新运行同一个算法，你会看到不同路径。这一点可以作为答辩亮点：

> 最短路径不是绝对概念，它取决于代价函数。地图数据相同，角色模型不同，最优路径也会不同。

## 地形与海拔的关系

本项目把“海拔”和“地形种类”明确分开：

- `height`：几何高度，用于 3D 地形起伏和坡度代价；
- `terrain`：通行语义，表示地表类型、移动阻力和可通行性。

所以：

- 高海拔可以是道路；
- 低海拔也可以是山地；
- 山地不等于高，山地表示粗糙地表；
- 水域不等于低，水域表示当前地面角色不可通行；
- 道路不等于平地，道路表示基础设施，移动代价更低。

算法中的单步代价大致为：

```text
step cost = base distance × terrain multiplier
          + terrain penalty × terrain weight
          + |height difference| × height weight × slope multiplier
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

### 左侧新增开关

| 开关 | 功能 |
|---|---|
| `Surface: smooth / blocks` | 切换连续地形和方块地形 |
| `Path: curve / polyline` | 切换平滑路径和折线路径 |
| `Algorithm graph overlay` | 在 3D 连续地形上显示底层离散图 |

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

不需要完整 Xcode，只需要 Xcode Command Line Tools、CMake 和编译器：

```bash
xcode-select --install
brew install cmake
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

- 加入 PRM / RRT / RRT*，真正进入采样路径规划
- 加入蚁群算法 ACO，用信息素动画展示路径收敛
- 把模拟退火用于 TSP / 多目标路径，而不是普通单源最短路
- 搜索热力图
- 搜索树面板
- 更正式的项目文档：需求分析、总体设计、详细设计、测试报告、用户手册
