# PathLab Prototype v1.1.1

这一版修复 v1.1 中 3D Diorama/Editor 自定义立方体的渲染顺序与面片缺失问题。

## 主要修复

- 修复自定义五面立方体的面片 winding 不一致导致的侧面三角形缺失。
- `DrawQuad3D` 改为双 winding 绘制，避免 raylib/OpenGL back-face culling 导致一个四边形只剩半个三角面。
- `DrawFlatShadedCubeV` 不再混用 raylib 原生 `DrawCubeV`，统一使用自定义五面实心绘制。
- 屋顶板高度略微抬高，避免和建筑顶面发生 z-fighting。
- 保留 v1.1 的 Demo Mode、Undo/Redo、Legend、Agent Path Compare、Screenshot、Map Presets 等功能。

## 构建

Windows:

```bash
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

macOS:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/PathLabSample
```
