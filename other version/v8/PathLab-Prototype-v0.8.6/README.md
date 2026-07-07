# PathLab Prototype v0.8.6

This version redraws the 3D `Surface: soft` mode as a readable contour-relief terrain.

Key changes:

- `Surface: soft` is no longer a flat pastel mesh.
- Soft mode now uses stronger height exaggeration, banded contour coloring, directional shading, and a terrain skirt.
- Roads, water, obstacles, forests, and mountain peaks are rendered as explicit semantic overlays.
- The soft-mode camera preset is closer and lower to make relief visible.
- `Path: curve` keeps the v0.8.5 rounded-polyline implementation.
- Save/Open still use native system file dialogs.

Controls:

- `F5`: toggle 2D / 3D
- `F6`: toggle Light / Dark theme
- `R`: reset 3D camera when the mouse is over the canvas
- Mouse wheel: zoom camera in 3D; zoom map in 2D
- Right/middle drag in 3D: rotate camera

Build:

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
