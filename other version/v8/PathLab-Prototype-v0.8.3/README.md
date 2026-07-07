# PathLab Prototype v0.8.3

本版本针对 v0.8.2 的可读性问题做了集中修复：

- 优化 UI 排版：左右侧栏分组标题加粗、按钮和文字放大、整体比例随窗口尺寸自适应。
- 左右侧栏支持滚轮滚动，避免低分辨率或放大后发生内容重叠。
- 2D 编辑模式支持缩放和平移：鼠标滚轮缩放，中键拖动画布。
- 2D 放大后，格子里的高度数字会随格子尺寸同步放大，方便编辑。
- `Surface: soft` 重新设计为更清楚的 soft heightfield，不再是看不清的淡色平面。
- soft 模式加入低噪声高度着色、坡度明暗、稀疏等高线、可读的障碍和道路覆盖层。
- 地图保存/读取支持 3 个槽位：`Slot 1`、`Slot 2`、`Slot 3`，对应 `maps/slot1.pathmap` 等文件。
- 字体加载分辨率提升到 192px，并优先尝试系统粗体/半粗体字体；仍可通过 `assets/fonts/ui.ttf` 自定义字体。

## 快捷键

| 操作 | 功能 |
|---|---|
| F5 | 切换 2D / 3D |
| F6 | 切换亮色 / 暗色主题 |
| F10 | 最大化窗口 |
| F11 | 全屏 / 退出全屏 |
| R | 3D 模式下重置相机 |
| 鼠标滚轮 | 2D 模式缩放地图；3D 模式缩放相机；侧栏上滚动侧栏 |
| 鼠标中键拖动 | 2D 模式平移地图；3D 模式旋转相机 |

## 构建

Windows / Visual Studio 2022：

```bash
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

macOS：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/PathLabSample
```

第一次构建需要联网下载 raylib。若 CMake 4.x 因第三方库兼容性报错，本项目已经在 `CMakeLists.txt` 中设置了兼容策略。

## 字体替换

将字体文件放到：

```text
assets/fonts/ui.ttf
```

或：

```text
assets/fonts/ui.otf
```

程序启动时会优先加载该字体。请不要随项目分发没有授权的商业字体。

## 地图文件

三个保存槽位分别是：

```text
maps/slot1.pathmap
maps/slot2.pathmap
maps/slot3.pathmap
```

在右侧 `OUTPUT` 区域选择槽位后，再点击 `Save` 或 `Load`。
