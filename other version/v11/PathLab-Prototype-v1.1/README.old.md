# PathLab Prototype v0.8.2

一个使用 **C++17 + CMake + raylib** 编写的路径规划算法可视化原型。v0.8.2 在 v0.8.1 的基础上重点修复了 3D soft/mesh 视图信息过载、2D 编辑时格子太小、整体 UI 排版拥挤的问题。

## v0.8.2 新增 / 修复内容

- 重新设计 UI 布局：顶部栏更简洁，左右面板按 `MAP / ALGORITHMS / AGENT & COST / VIEW` 与 `TOOLS / PLAYBACK / OUTPUT / STATS` 分组。
- 2D 模式加入缩放和平移：鼠标滚轮缩放格子，中键拖动画布，顶部 `Reset 2D` 可复位。
- 左侧加入 `2D zoom` 滑条，适合精细编辑大地图。
- `Surface: mesh` 改名为 `Surface: soft`，并重写为低噪声连续高度场。
- soft 模式不再使用大量不同颜色三角面拼贴，改为以海拔为主的统一渐变表面。
- blocked cells 在 soft 模式下改为低矮半透明覆盖，不再铺满刺眼灰色方块。
- 森林、山峰、道路 3D 小部件进一步降密度，减少视觉噪声。
- 字体加载分辨率提升到 128px，按钮与滑条文字字号和位置重新调整。

## 已有功能

- 2D / 3D 视图切换。
- solid 地形 / soft 连续高度场切换。
- 3D 鼠标拾取、射线检测、选中地块。
- 2D 和 3D 地图编辑。
- 随机地图、连续地形地图、迷宫地图生成。
- BFS、Dijkstra、A*、Weighted A*、Greedy Best-First。
- 行人、汽车、越野车、无人机四种移动角色。
- 四方向 / 八方向移动。
- 地形代价、海拔代价、启发式权重调节。
- 路径平滑显示。
- 算法图层显示。
- 搜索动画播放、暂停、单步、回放、时间轴。
- Benchmark 对比和 CSV 导出。
- 地图保存 / 读取。
- 字体替换机制。

## 快捷键

| 快捷键 | 功能 |
|---|---|
| `F5` | 切换 2D / 3D 视图 |
| `F6` | 切换亮色 / 暗色主题 |
| `F10` | 最大化窗口 |
| `F11` | 全屏 / 退出全屏 |
| 鼠标滚轮 | 2D 缩放 / 3D 缩放 |
| 鼠标中键拖动 | 2D 平移 / 3D 旋转 |
| `R` | 3D 视图下重置相机 |
| `WASD` | 3D 视图平移相机目标 |
| `Q / E` | 3D 视图降低 / 抬高相机目标 |
| 鼠标滚轮 | 3D 视图缩放 |
| 右键 / 中键拖动 | 3D 视图旋转相机 |
| `Shift + 左键` | 设置起点 |
| `Ctrl + 左键` | 设置终点 |

## 构建方式

### Windows + Visual Studio 2022

```bash
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

运行：

```bash
./build/Release/PathLabSample.exe
```

### macOS

不需要完整 Xcode，但需要 Xcode Command Line Tools：

```bash
xcode-select --install
brew install cmake
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/PathLabSample
```

如果可执行文件位置不同，可用：

```bash
find build -name "PathLabSample"
```

## 字体替换

把字体文件放到：

```text
assets/fonts/ui.ttf
```

或者：

```text
assets/fonts/ui.otf
```

程序启动时会自动加载。仓库不附带字体文件，避免字体授权问题。

## 答辩可讲的设计点

本系统采用“视觉连续、算法离散”的设计：

- 视觉层使用连续三角地形、渐变背景和平滑路径，让地图看起来更接近连续空间。
- 算法层仍然保留网格图结构，使 Dijkstra / A* / BFS 等经典图搜索算法可解释、可测试、可比较。
- 代价函数把距离、地形、海拔和移动角色分离。同一张地图、同一起点终点，在行人、汽车、越野车、无人机模式下会得到不同路径。
- 亮色/暗色主题不是简单反色，而是分别定义了地形、UI、路径和搜索状态的颜色，保证 3D 演示中的可读性。


## v0.8.1 视觉修正

这一版修复了 v0.8 的两个主要观感问题：

- 默认 3D 表面改为 `Surface: solid`，减少三角网格造成的碎片感。
- `Surface: mesh` 仍然保留，但同一网格单元两组三角面使用统一颜色，并对顶点高度做局部平均，降低刺眼的三角拼贴效果。
- 地形小部件密度降低，森林、山峰、道路不再每个格子都堆叠模型，画面更干净。
- UI 字体加载分辨率从 32 提升到 96，并启用双线性过滤，减轻字体锯齿。
- 默认 3D 高度缩放降低，山体不再过分尖锐。

快捷键不变：`F5` 切换 2D/3D，`F6` 切换亮色/暗色，`F11` 全屏。
