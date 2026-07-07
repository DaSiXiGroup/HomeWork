# PathLab Prototype v0.8.7

This version fixes the soft terrain view and its 3D picking logic.

## Main changes

- Fixed `Surface: soft` picking: the ray-cast no longer tests raw cell boxes while the surface is rendered from smoothed heights.
- `Surface: soft` now uses one shared height function for terrain rendering, path placement, selection marker, start/goal marker, and graph overlay.
- Soft-mode picking projects the mouse ray onto the map's XZ plane, then converts the hit point to a grid cell. This is more stable for a continuous heightfield.
- Removed the extra solid-mode feature pass from soft mode, so the soft view is no longer covered by duplicate cubes, trees, and mountain markers.
- Made soft-mode contour lines much sparser to avoid the broken-tile look.
- Updated title/version to v0.8.7.

## Controls

- `F5`: switch 2D / 3D
- `F6`: switch light / dark theme
- `F10`: maximize
- `F11`: fullscreen
- `R`: reset 3D camera
- Mouse wheel in 2D: zoom
- Middle mouse in 2D: pan
- Left click in 3D: select / edit cell
- Right or middle drag in 3D: rotate camera
- Mouse wheel in 3D: zoom

## Build

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
