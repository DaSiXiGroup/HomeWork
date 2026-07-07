# PathLab Prototype v0.9

A C++17 + raylib path-planning visualization prototype.

v0.9 removes the failed soft-mesh design and replaces it with a **terrace relief terrain** mode:

- `Surface: solid`: precise block-based editor view.
- `Surface: relief`: clean 2.5D terraced terrain board, no triangle patchwork.
- 3D picking now uses the same bounding boxes as the visible terraced blocks.
- Roads/water are drawn as clear top-layer plates.
- Forest/mountain markers are sparse accents rather than clutter.
- Path curve mode uses rounded-safe curves rather than drifting splines.
- Save/Open uses native file dialogs on Windows/macOS/Linux where available.

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

- `F5`: 2D/3D view
- `F6`: light/dark theme
- `F10`: maximize
- `F11`: fullscreen
- `R`: reset 3D camera when mouse is over canvas
- 2D: mouse wheel zoom, middle mouse drag pan
- 3D: left click edit/select, right/middle drag rotate, wheel zoom, WASD pan
