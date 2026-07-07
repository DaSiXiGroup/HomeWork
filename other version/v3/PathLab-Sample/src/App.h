#pragma once

#include "GridMap.h"
#include "Pathfinder.h"

#include "raylib.h"

#include <string>
#include <vector>

namespace pathlab {

enum class EditMode {
    Wall,
    Erase,
    Start,
    Goal,
    Raise,
    Lower,
    Plain,
    Road,
    Forest,
    Mountain,
    Water
};

struct BenchmarkRow {
    std::string algorithmName;
    bool found = false;
    double pathLength = 0.0;
    int expandedNodes = 0;
    int generatedNodes = 0;
    double elapsedMs = 0.0;
};

class App {
public:
    App();
    void Run();

private:
    static constexpr int kWindowWidth = 1440;
    static constexpr int kWindowHeight = 900;

    GridMap map_;
    SearchResult result_;
    AlgorithmOptions options_{};
    std::vector<BenchmarkRow> benchmarkRows_;

    bool hasResult_ = false;
    bool playing_ = false;
    int frameIndex_ = 0;
    float animationSpeed_ = 35.0f;
    float obstacleProbability_ = 0.28f;
    float mapRowsValue_ = 35.0f;
    float mapColsValue_ = 55.0f;
    float heuristicWeightValue_ = 1.4f;
    float terrainWeightValue_ = 1.0f;
    float heightWeightValue_ = 0.25f;
    float accumulator_ = 0.0f;

    EditMode editMode_ = EditMode::Wall;
    std::string statusMessage_ = "Ready";

    Font uiFont_{};
    bool customFontLoaded_ = false;
    std::string fontStatus_ = "default raylib font";

    Rectangle leftPanel_{};
    Rectangle rightPanel_{};
    Rectangle canvas_{};
    Rectangle bottomPanel_{};

    void Layout();
    void Update();
    void Draw();

    void DrawTopBar();
    void DrawLeftPanel();
    void DrawRightPanel();
    void DrawBottomPanel();
    void DrawMapCanvas();
    void HandleMapEditing();

    void GenerateRandomMap();
    void GenerateTerrainMap();
    void GenerateMazeMap();
    void ApplyMapSize();
    void RunAlgorithm(AlgorithmKind kind);
    void RunBenchmark();
    void SaveCurrentMap();
    void LoadCurrentMap();
    void ExportBenchmarkCsv();
    void ResetVisualization();
    void SyncOptionsFromSliders();

    void LoadUIFont();
    void UnloadUIFont();

    bool Button(Rectangle bounds, const char* text);
    bool ToggleButton(Rectangle bounds, const char* text, bool active);
    bool Slider(Rectangle bounds, const char* label, float* value, float minValue, float maxValue, bool integerDisplay = false);

    void DrawTextUI(const std::string& text, float x, float y, float fontSize, Color color) const;
    float MeasureTextUI(const std::string& text, float fontSize) const;

    [[nodiscard]] int FrameCount() const;
    [[nodiscard]] const SearchFrame* CurrentFrame() const;
    [[nodiscard]] int MouseToCell(Vector2 mouse) const;
    [[nodiscard]] Color CellColor(int cell, const SearchFrame* frame) const;
    [[nodiscard]] bool ContainsCell(const std::vector<int>& values, int cell) const;
    [[nodiscard]] std::string EditModeName() const;
};

} // namespace pathlab
