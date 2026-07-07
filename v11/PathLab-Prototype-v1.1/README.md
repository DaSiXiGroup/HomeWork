# PathLab Prototype v1.1

C++17 + CMake + raylib 的地图最短路径可视化样例工程。

v1.1 在 v1.0.3 的基础上加入了几个更适合答辩展示的功能：

- **Demo Mode**：一键生成河流地图、切到 3D Diorama、运行 A* 并播放动画。
- **Map Presets**：City preset、River preset、Mountain pass。
- **Undo / Redo**：支持 Ctrl+Z / Ctrl+Y，也可以点右侧按钮。
- **Legend**：右侧显示地形图例和当前 agent 的通行规则。
- **Agent Path Compare**：同一张地图上同时比较 Pedestrian / Car / Offroad / Drone 的 A* 路径。
- **Screenshot Export**：弹出系统文件管理器，导出当前窗口截图为 PNG。
- 继续保留：2D/3D 编辑、Diorama 展示、A*/Dijkstra/BFS/Weighted A*/Greedy、Benchmark、CSV 导出、文件管理器保存/读取地图。

## Build

### Windows + Visual Studio 2022

```bash
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

运行：

```bash
.\build\Release\PathLabSample.exe
```

### macOS

不需要完整 Xcode，只需要 Command Line Tools：

```bash
xcode-select --install
brew install cmake
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/PathLabSample
```

第一次构建需要联网下载 raylib。

## Important controls

```text
F5        切换 2D / 3D
F6        切换 Light / Dark
F10       最大化
F11       全屏
R         3D 重置相机
Ctrl + Z  撤销
Ctrl + Y  重做
```

2D 编辑：

```text
鼠标滚轮       缩放地图
鼠标中键拖动   平移地图
左键拖动       使用当前工具绘制
右键拖动       擦除障碍/水域
Shift + 左键   设置起点
Ctrl + 左键    设置终点
```

3D 编辑：

```text
左键点击/拖动       拾取并编辑格子
右键/中键拖动       旋转相机
鼠标滚轮            缩放相机
WASD / Q / E        平移/升降相机目标
```

## Suggested demo flow

1. 点击左侧 **Demo A\***。
2. 程序会自动生成 River preset、进入 3D Diorama、运行 A*。
3. 点击右侧 **Screenshot** 可导出演示截图。
4. 点击左侧 **Agent paths: ON**，比较四类 agent 的路径差异。
5. 点击 **Benchmark**，再导出 CSV。

## Notes

- `Style: editor` 用于精确编辑。
- `Style: diorama` 用于答辩展示。
- `Path: curve` 使用圆角路径，不会像样条曲线那样严重外甩。
- 地图文件扩展名为 `.pathmap`。
