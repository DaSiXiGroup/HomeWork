#pragma once

#include "GridMap.h"
#include "Pathfinder.h"

#include "raylib.h"

#include <string>
#include <vector>

using DIV = Rectangle;
// shorthand for drawing rectangles, caz We just wanna serve rectangles as DIVs
// and we don't need to type Rectangle all the time.

const Color PANEL_COLOR         = Color{ 34, 39, 46, 255 };
const Color DARK_PANEL_COLOR    = Color{ 25, 29, 34, 255 };
const Color ACCENT_COLOR        = Color{ 66, 135, 245, 255 };
const Color TEXT_COLOR          = Color{ 235, 240, 247, 255 };
const Color MUTED_TEXT_COLOR    = Color{ 160, 171, 186, 255 };
const Color GRID_LINE_COLOR     = Color{ 50, 56, 64, 255 };
const Color DANGER_COLOR        = Color{ 255, 100, 110, 255 };
const Color SUCCESS_COLOR       = Color{ 94, 222, 128, 255 };
const Color ROAD_COLOR          = Color{ 213, 199, 143, 255 };
const Color FOREST_COLOR        = Color{ 113, 166, 114, 255 };
const Color WATER_COLOR         = Color{ 59, 132, 204, 255 };
const Color MOUNTAIN_COLOR      = Color{ 158, 142, 130, 255 };
const Color PLAIN_COLOR         = Color{ 224, 229, 236, 255 };
const Color TEMP_COLOR          = Color{ 224, 229, 236, 255 };
const Color CLOSE_COLOR         = Color{ 116, 129, 150, 255 };
const Color OPEN_COLOR          = Color{ 88,  180, 255, 255 };
const Color PATH_COLOR          = Color{ 255, 224, 102, 255 };
const Color CELL_COLOR          = Color{ 255, 177, 66, 255 };
const Color START_COLOR         = Color{ 80, 220, 120, 255 };
const Color GOAL_COLOR          = Color{ 255, 89, 94, 255 };

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
    bool    found           = false;
    double  pathLength      = 0.0;
    int     expandedNodes   = 0;
    int     generatedNodes  = 0;
    double  elapsedMs       = 0.0;
};

class App {
public:
    App();
    void Run();

private:
    static constexpr int kWindowWidth   = 1600;
    static constexpr int kWindowHeight  = 1000;

    GridMap                     __MAP;
    SearchResult                __RESULT;
    AlgorithmOptions            __OPTIONS{};
    std::vector<BenchmarkRow>   __BENCHMARK_ROWS;

    bool    __HAS_RESULT                = false;
    bool    __PLAYING                   = false;
    int     __FRAME_INDEX               = 0;
    float   __ANIMATION_SPEED           = 35.0f;
    float   __OBSTACLE_PROBABILITY      = 0.28f;
    float   __MAP_ROWS_VALUE            = 35.0f;
    float   __MAP_COLS_VALUE            = 55.0f;
    float   __HEURISTTIC_WEIGHT_VALUE   = 1.4f;
    float   __TERRAIN_WEIGHT_VALUE      = 1.0f;
    float   __HEIGHT_WEIGHT_VALUE       = 0.25f;
    float   __ACCUMULATOR               = 0.0f;
    float   __UI_SCALE                  = 1.0f;
    bool    __FULL_SCREEN               = false;
    int     __WINDOW_ED_WIDTH           = kWindowWidth;
    int     __WINDOW_ED_HEIGHT          = kWindowHeight;

	// Edit mode
    EditMode    __EDIT_MODE     = EditMode::Wall;
    std::string __STATUS_MSG    = "Ready";

    // Set Font
    Font        __UI_FONT{};
    bool        __CUSTOM_FONT_LOADED    = false;
    std::string __FONT_STATUS           = "default raylib font";

    Rectangle __LEFT_PANEL{};
    Rectangle __RIGHT_PANEL{};
    Rectangle __CANVAS{};
    Rectangle __BOTTOM_PANEL{};

    // Layout Functions
    void Layout();
    void Update();
    void Draw();
    void UpdateUIScale();
    void ToggleFullscreenMode();

	// Draw Functions
    void DrawTopBar();
    void DrawLeftPanel();
    void DrawRightPanel();
    void DrawBottomPanel();
    void DrawMapCanvas();
    void HandleMapEditing();

	// Utility Functions : Map and Algorithm
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

    // Font Functions
    void LoadUIFont();
    void UnloadUIFont();

	// Button Function
    bool Button         (Rectangle bounds, const char* text);
    bool ToggleButton   (Rectangle bounds, const char* text, bool active);
    bool Slider         (Rectangle bounds, const char* label, float* value, float minValue, float maxValue, bool integerDisplay = false);

    void    DrawTextUI   (const std::string& text, float x, float y, float fontSize, Color color) const;
    float   MeasureTextUI(const std::string& text, float fontSize) const;

    [[nodiscard]] float                 S(float value) const;
    [[nodiscard]] int                   FrameCount() const;
    [[nodiscard]] const SearchFrame*    CurrentFrame() const;
    [[nodiscard]] int                   MouseToCell(Vector2 mouse) const;
    [[nodiscard]] Color                 CellColor(int cell, const SearchFrame* frame) const;
    [[nodiscard]] bool                  ContainsCell(const std::vector<int>& values, int cell) const;
    [[nodiscard]] std::string           EditModeName() const;
};

}