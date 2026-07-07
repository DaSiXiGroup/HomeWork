#pragma once

#include "GridMap.h"
#include "Pathfinder.h"

#include "raylib.h"

#include <string>
#include <vector>

namespace pathlab {

enum class ThemeMode {
    Light,
    Dark
};

enum class EditMode {
    Select,
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
    static constexpr int kWindowWidth = 1600;
    static constexpr int kWindowHeight = 1000;

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
    float uiScale_ = 1.0f;
    bool fullscreen_ = false;
    bool view3D_ = false;
    ThemeMode themeMode_ = ThemeMode::Light;
    int selectedCell_ = -1;
    int hoveredCell3D_ = -1;
    float terrainHeightScale_ = 0.24f;
    float mapZoom2D_ = 1.0f;
    Vector2 mapPan2D_{ 0.0f, 0.0f };
    int selectedMapSlot_ = 1;
    float leftPanelScroll_ = 0.0f;
    float rightPanelScroll_ = 0.0f;
    bool smoothTerrain3D_ = false;
    bool smoothPath_ = true;
    bool showGraphOverlay_ = false;
    MovementProfile movementProfile_ = MovementProfile::Pedestrian;
    float cameraDistance_ = 58.0f;
    float cameraYaw_ = 0.78f;
    float cameraPitch_ = 0.58f;
    Vector3 cameraTarget_{ 0.0f, 0.0f, 0.0f };
    Camera3D camera_{};
    int windowedWidth_ = kWindowWidth;
    int windowedHeight_ = kWindowHeight;

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
    void UpdateUIScale();
    void ToggleFullscreenMode();
    void ToggleViewMode();
    void ToggleThemeMode();
    void ResetCamera3D();
    void UpdateCamera3D();

    void DrawTopBar();
    void DrawLeftPanel();
    void DrawRightPanel();
    void DrawBottomPanel();
    void DrawMapCanvas();
    void DrawTerrain3D();
    void DrawGradientBackground(Rectangle bounds) const;
    void DrawBlockTerrain3D(const SearchFrame* frame) const;
    void DrawSmoothTerrain3D(const SearchFrame* frame) const;
    void DrawPath3D(const std::vector<int>& path) const;
    void DrawGraphOverlay3D() const;
    void DrawCellSelection3D(int cell, Color color) const;
    void DrawTerrainFeature3D(int cell) const;
    void HandleMapEditing();
    void ApplyEditToCell(int cell);

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
    void DrawTextUIBold(const std::string& text, float x, float y, float fontSize, Color color) const;
    float MeasureTextUI(const std::string& text, float fontSize) const;

    [[nodiscard]] float S(float value) const;
    [[nodiscard]] int FrameCount() const;
    [[nodiscard]] const SearchFrame* CurrentFrame() const;
    [[nodiscard]] int MouseToCell(Vector2 mouse) const;
    [[nodiscard]] int PickCell3D(Vector2 mouse) const;
    [[nodiscard]] BoundingBox CellBoundingBox3D(int cell) const;
    [[nodiscard]] Color CellColor(int cell, const SearchFrame* frame) const;
    [[nodiscard]] Color TerrainVisualColor(TerrainType terrain, float heightNorm, bool sideFace = false) const;
    [[nodiscard]] Color PanelColor() const;
    [[nodiscard]] Color DarkPanelColor() const;
    [[nodiscard]] Color AccentColor() const;
    [[nodiscard]] Color TextColor() const;
    [[nodiscard]] Color MutedTextColor() const;
    [[nodiscard]] Color GridLineColor() const;
    [[nodiscard]] Color DangerColor() const;
    [[nodiscard]] Color SuccessColor() const;
    [[nodiscard]] Color CanvasColor() const;
    [[nodiscard]] Color OverlayColor() const;
    [[nodiscard]] Vector3 CellWorldPosition(int cell, float extraHeight = 0.35f) const;
    [[nodiscard]] Vector3 TerrainVertexWorld(int row, int col, float extraHeight = 0.0f) const;
    [[nodiscard]] Vector3 CatmullRom(Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3, float t) const;
    [[nodiscard]] float CellSurfaceHeight(int cell) const;
    [[nodiscard]] bool ContainsCell(const std::vector<int>& values, int cell) const;
    [[nodiscard]] std::string EditModeName() const;
    [[nodiscard]] std::string MovementProfileName() const;
    [[nodiscard]] std::string ThemeName() const;
    [[nodiscard]] std::string CellInfo(int cell) const;
    [[nodiscard]] std::string SelectedMapPath() const;
};

} // namespace pathlab
