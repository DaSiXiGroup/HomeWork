# PathLab Prototype v1.0.3

C++17 + raylib + CMake path-planning visualization prototype.

## v1.0 changes

- Replaced the old `relief/soft` surface with a true **Diorama** presentation mode.
- `Style: editor`: precise solid-cell 3D editor, best for editing and picking.
- `Style: diorama`: presentation sandbox; hides dense grid, draws roads as ribbons, water as plates, obstacles as buildings, forests/rocks as sparse landmarks.
- Keeps 2D zoom/pan editor, 3D mouse picking, native Save/Open file dialogs, Dijkstra/A*/BFS/Greedy/Weighted A*, benchmark export, light/dark themes.

## Build

Windows:

```bash
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

macOS/Linux:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/PathLabSample
```

## Controls

- `F5`: switch 2D/3D
- `F6`: light/dark theme
- `F10`: maximize
- `F11`: fullscreen
- 2D: wheel zoom, middle drag pan, left paint, right erase
- 3D: left select/edit, right/middle rotate, wheel zoom, WASD pan, R reset

## Fonts

Put a font file at `assets/fonts/ui.ttf` or `assets/fonts/ui.otf` to override the default UI font.


## v1.0.3 更新

- 3D 立方体改为按面着色：顶面更亮，两个侧面使用不同亮度，增强体积感。
- diorama 模式中的建筑、道路、水域、底板和 editor 模式中的地块均使用新的方向性面光照。
- 保留原有 2D/3D 编辑、Diorama 展示、文件选择保存/读取、曲线路径等功能。


## v1.0.3 update

- Reworked cuboid face lighting to a gentler ambient style.
- Removed the excessively dark underside/rear-face look from v1.0.1.
- Thin slabs such as road and water now render as clean solid plates.



## v1.0.3 说明

本版删除了立方体底面的绘制。普通建筑块、墙体、地形块贴在地形板上时只显示顶面和四个侧面，避免底面露出造成“空心盒子”或“悬浮块”的视觉问题。
