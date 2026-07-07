# PathLab Prototype v0.8

一个使用 **C++17 + CMake + raylib** 编写的路径规划算法可视化原型。v0.8 在 v0.7 的连续地形和多角色代价函数基础上，加入了更柔和的梦境渐变视觉风格，并提供亮色/暗色两个主题。

## v0.8 新增内容

- 梦境渐变背景，不再使用纯黑/纯灰背景。
- 亮色主题与暗色主题，可通过顶部按钮或 `F6` 切换。
- 3D 地形重新配色：地形颜色会随海拔产生柔和渐变。
- 修复 v0.7 暗色模式下 `F5` 切到 3D 后山峰颜色和背景重叠的问题：暗色主题中的山地改为高对比的淡紫/薰衣草色。
- UI 面板、按钮、滑条、时间轴同步适配主题。
- 起点、终点、当前节点、搜索集合、路径颜色统一调整。
- 道路、森林、山地、水域的 3D 小部件颜色也同步重做。

## 已有功能

- 2D / 3D 视图切换。
- 方块地形 / 连续地形切换。
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
