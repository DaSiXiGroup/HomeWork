# Refactor notes

This version keeps the user's App.h/App.cpp style, but abstracts two core concepts:

1. Terrain is no longer just a switch statement.
   - `Terrain` is an abstract base class.
   - `PlainTerrain`, `RoadTerrain`, `ForestTerrain`, `WaterTerrain`, and `MountainTerrain` define name, save id, movement blocking, and cost delta.
   - `TerrainCatalog` is the small registry used by `GridMap` and UI text.

2. Algorithms are no longer one giant `if/switch` inside `Pathfinder::Run`.
   - `SearchAlgorithm` is an abstract base class.
   - `BfsAlgorithm`, `DijkstraAlgorithm`, `AStarAlgorithm`, `WeightedAStarAlgorithm`, and `GreedyBestFirstAlgorithm` are concrete algorithm classes.
   - `Pathfinder` now only dispatches to the selected algorithm object.

Also fixed:
- `GridMap::Resize` / `LoadFromFile` member assignment shadowing.
- obvious naked constants in GridMap/Pathfinder are now named `constexpr` values.
- terrain costs and algorithm weights are named rather than buried inside expressions.
