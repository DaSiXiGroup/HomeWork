# PathLab Prototype v0.8.5

本版本继续修复 v0.8.3 的交互和可读性问题：

- 保存 / 读取地图改为弹出系统文件管理器：Windows 使用系统 Save/Open FileDialog，macOS 使用 Finder 选择文件，Linux 尝试使用 zenity。
- 取消 `Slot 1/2/3` 的固定保存槽位，用户可以自由选择保存位置和文件名。
- 左右侧栏的滑条改成一行布局：`标签 + 滑条 + 数值`，数值统一放到滑条右侧，不再压在按钮和文字上。
- UI 字体间距缩小，按钮文字、分组标题、滑条文字进一步放大并加粗。
- `Surface: soft` 模式重新增强：更低的默认相机角度、更近的相机距离、更强的高度夸张和明暗层次，避免看起来像一张平面色块。
- 在 3D 模式下切换 `Surface: solid / soft` 时，会自动重置到更适合当前模式的相机角度。

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

## 保存 / 读取地图

点击右侧 `OUTPUT` 区域的：

- `Save as...`：弹出文件管理器，选择保存位置和文件名；没有扩展名时会自动补 `.pathmap`。
- `Open...`：弹出文件管理器，选择已有 `.pathmap` 地图文件。

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


## v0.8.5 update

- Rebuilt `Path: curve` mode.
- Replaced the previous Catmull-Rom spline with a conservative rounded-corner route.
- The curve now stays near the original grid-cell centerline and avoids large overshoot at corners.
- 2D paths now draw with an outline plus a solid core for better visibility.
- 3D paths now render as a thicker route tube instead of a one-pixel line.
- `Path: polyline` is still available when exact cell-by-cell movement must be shown.
