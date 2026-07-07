#pragma once

#include "GridMap.h"
#include "Pathfinder.h"

#include "raylib.h"

#include <string>
#include <vector>
#include <array>

namespace pathlab{

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

struct AgentComparisonRow {
    MovementProfile profile = MovementProfile::Pedestrian;
    std::string profileName;
    bool found          = false;
    double pathLength   = 0.0;
    int expandedNodes   = 0;
    double elapsedMs    = 0.0;
    std::vector<int> path;
};

struct MapHistoryState {
    GridMap map;
    std::string label;
};

class App {
public:
    App();
    void Run();

private:
    static const int __windowWidth  = 1600;
    static const int __windowHeight = 1000;

    GridMap                         __map;
    SearchResult                    __result;
    AlgorithmOptions                __options{};
    std::vector<BenchmarkRow>       __benchmarkRows;
    std::vector<AgentComparisonRow> __agentComparisonRows;
    std::vector<MapHistoryState>    __undoStack;
    std::vector<MapHistoryState>    __redoStack;

    bool            __hasResult             = false;
    bool            __playing               = false;
    int             __frameIndex            = 0;
    float           __animationSpeed        = 35.0f;
    float           __obstacleProbability   = 0.28f;
    float           __mapRowsValue          = 35.0f;
    float           __mapColsValue          = 55.0f;
    float           __heuristicWeightValue  = 1.4f;
    float           __terrainWeightValue    = 1.0f;
    float           __heightWeightValue     = 0.25f;
    float           __accumulator           = 0.0f;
    float           __uiScale               = 1.0f;
    bool            __fullscreen            = false;
    bool            __view3D                = false;
    ThemeMode       __themeMode             = ThemeMode::Light;
    int             __selectedCell          = -1;
    int             __hoveredCell3D         = -1;
    float           __terrainHeightScale    = 0.24f;
    float           __mapZoom2D             = 1.0f;
    Vector2         __mapPan2D              { 0.0f, 0.0f };
    int             __selectedMapSlot       = 1;
    std::string     __currentMapPath;
    float           __leftPanelScroll       = 0.0f;
    float           __rightPanelScroll      = 0.0f;
    bool            __smoothTerrain3D       = false;
    bool            __smoothPath            = true;
    bool            __showGraphOverlay      = false;
    bool            __showAgentComparison   = false;
    int             __lastEditedCell        = -1;
    MovementProfile __movementProfile       = MovementProfile::Pedestrian;
    float           __cameraDistance        = 58.0f;
    float           __cameraYaw             = 0.78f;
    float           __cameraPitch           = 0.58f;
    Vector3         __cameraTarget          { 0.0f, 0.0f, 0.0f };
    Camera3D        __camera{};
    int             __windowedWidth         = __windowWidth;
    int             __windowedHeight        = __windowHeight;

    EditMode        __editMode              = EditMode::Wall;
    std::string     __statusMessage         = "Ready";

    Font            __uiFont{};
    bool            __customFontLoaded      = false;
    std::string     __fontStatus            = "default raylib font";

    Rectangle __leftPanel{};
    Rectangle __rightPanel{};
    Rectangle __canvas{};
    Rectangle __bottomPanel{};

    void __Layout();
    void __Update();
    void __Draw();
    void __UpdateUIScale();
    void __ToggleFullscreenMode();
    void __ToggleViewMode();
    void __ToggleThemeMode();
    void __ResetCamera3D();
    void __UpdateCamera3D();

    void __DrawTopBar();
    void __DrawLeftPanel();
    void __DrawRightPanel();
    void __DrawBottomPanel();
    void __DrawMapCanvas();
    void __DrawTerrain3D();
    void __DrawGradientBackground(Rectangle bounds) const;
    void __DrawBlockTerrain3D(const SearchFrame* frame) const;
    void __DrawSmoothTerrain3D(const SearchFrame* frame) const;
    void __DrawPath3D(const std::vector<int>& path) const;
    void __DrawPath3DColored(const std::vector<int>& path, Color core, float radius = 0.055f) const;
    void __DrawAgentComparison2D(float startX, float startY, float cellSize) const;
    void __DrawAgentComparison3D() const;
    void __DrawLegend(float x, float& y, float contentW);
    void __DrawGraphOverlay3D() const;
    void __DrawCellSelection3D(int cell, Color color) const;
    void __DrawTerrainFeature3D(int cell) const;
    void __HandleMapEditing();
    void __ApplyEditToCell(int cell);

    void __GenerateRandomMap();
    void __GenerateTerrainMap();
    void __GenerateMazeMap();
    void __GenerateCityPreset();
    void __GenerateRiverPreset();
    void __GenerateMountainPreset();
    void __RunDemoMode();
    void __ApplyMapSize();
    void __RunAlgorithm(AlgorithmKind kind);
    void __RunBenchmark();
    void __CompareAgents();
    void __ExportScreenshot();
    void __SaveCurrentMap();
    void __LoadCurrentMap();
    void __ExportBenchmarkCsv();
    void __ResetVisualization();
    void __SyncOptionsFromSliders();
    void __PushUndoState(const std::string& label);
    void __ClearHistory();
    void __UndoEdit();
    void __RedoEdit();

    void __LoadUIFont();
    void __UnloadUIFont();

    bool __Button(Rectangle bounds, const char* text);
    bool __ToggleButton(Rectangle bounds, const char* text, bool active);
    bool __Slider(Rectangle bounds, const char* label, float* value, float minValue, float maxValue, bool integerDisplay = false);

    void    __DrawTextUI(const std::string& text, float x, float y, float fontSize, Color color) const;
    void    __DrawTextUIBold(const std::string& text, float x, float y, float fontSize, Color color) const;
    float   __MeasureTextUI(const std::string& text, float fontSize) const;

    [[nodiscard]] float                 __S(float value) const;
    [[nodiscard]] int                   __FrameCount() const;
    [[nodiscard]] const SearchFrame*    __CurrentFrame() const;
    [[nodiscard]] int                   __MouseToCell(Vector2 mouse) const;
    [[nodiscard]] int                   __PickCell3D(Vector2 mouse) const;
    [[nodiscard]] BoundingBox           __CellBoundingBox3D(int cell) const;
    [[nodiscard]] float                 __SoftSurfaceHeightAt(int row, int col) const;
    [[nodiscard]] float                 __CellVisualHeight3D(int cell) const;
    [[nodiscard]] Color                 __CellColor(int cell, const SearchFrame* frame) const;
    [[nodiscard]] Color                 __TerrainVisualColor(TerrainType terrain, float heightNorm, bool sideFace = false) const;
    [[nodiscard]] Color                 __PanelColor() const;
    [[nodiscard]] Color                 __DarkPanelColor() const;
    [[nodiscard]] Color                 __AccentColor() const;
    [[nodiscard]] Color                 __TextColor() const;
    [[nodiscard]] Color                 __MutedTextColor() const;
    [[nodiscard]] Color                 __GridLineColor() const;
    [[nodiscard]] Color                 __DangerColor() const;
    [[nodiscard]] Color                 __SuccessColor() const;
    [[nodiscard]] Color                 __CanvasColor() const;
    [[nodiscard]] Color                 __OverlayColor() const;
    [[nodiscard]] Vector3               __CellWorldPosition(int cell, float extraHeight = 0.35f) const;
    [[nodiscard]] Vector3               __TerrainVertexWorld(int row, int col, float extraHeight = 0.0f) const;
    [[nodiscard]] Vector3               __CatmullRom(Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3, float t) const;
    [[nodiscard]] float                 __CellSurfaceHeight(int cell) const;
    [[nodiscard]] bool                  __ContainsCell(const std::vector<int>& values, int cell) const;
    [[nodiscard]] std::string           __EditModeName() const;
    [[nodiscard]] std::string           __MovementProfileName() const;
    [[nodiscard]] std::string           __ThemeName() const;
    [[nodiscard]] std::string           __CellInfo(int cell) const;
    [[nodiscard]] std::string           __SelectedMapPath() const;
};

namespace ColorSet {
    struct Ramp { Color low, mid, high; };

    const unsigned char SelectionFillAlpha = 42;
    const Color Transparent{ 0, 0, 0, 0 };

    const Color PanelDark{ 30, 30, 45, 248 };
    const Color PanelLight{ 252, 244, 238, 248 };
    const Color PanelDeepDark{ 20, 22, 35, 255 };
    const Color PanelDeepLight{ 239, 220, 222, 255 };
    const Color AccentDark{ 241, 170, 117, 255 };
    const Color AccentLight{ 231, 111, 81, 255 };
    const Color TextDark{ 246, 240, 255, 255 };
    const Color TextLight{ 64, 55, 75, 255 };
    const Color TextMutedDark{ 188, 178, 210, 255 };
    const Color TextMutedLight{ 126, 106, 123, 255 };
    const Color GridLineDark{ 93, 76, 121, 120 };
    const Color GridLineLight{ 212, 188, 191, 150 };
    const Color DangerDark{ 255, 126, 140, 255 };
    const Color DangerLight{ 220, 70, 80, 255 };
    const Color SuccessDark{ 120, 220, 170, 255 };
    const Color SuccessLight{ 42, 157, 143, 255 };
    const Color CanvasDark{ 18, 20, 34, 255 };
    const Color CanvasLight{ 255, 237, 226, 255 };
    const Color OverlayDark{ 255, 245, 214, 255 };
    const Color OverlayLight{ 93, 64, 94, 255 };
    const Color ActiveButtonText{ 255, 248, 235, 255 };

    const Color ControlTrackDark{ 70, 58, 92, 255 };
    const Color ControlTrackLight{ 224, 198, 204, 255 };
    const Color ButtonHoverDark{ 74, 64, 98, 255 };
    const Color ButtonHoverLight{ 244, 210, 202, 255 };
    const Color ButtonIdleDark{ 52, 45, 70, 255 };
    const Color ButtonIdleLight{ 246, 226, 219, 255 };

    const std::array<Color, 4> ProfileDark{
        Color{ 255, 214, 139, 245 }, Color{ 119, 216, 255, 245 },
        Color{ 132, 225, 166, 245 }, Color{ 220, 166, 255, 245 }
    };
    const std::array<Color, 4> ProfileLight{
        Color{ 231, 111, 81, 245 }, Color{ 50, 145, 205, 245 },
        Color{ 59, 158, 112, 245 }, Color{ 138, 91, 198, 245 }
    };
    const Color ProfileFallbackDark{ 255, 245, 214, 245 };
    const Color ProfileFallbackLight{ 93, 64, 94, 245 };

    const Ramp PlainTerrainDark{ Color{ 224, 205, 229, 255 }, Color{ 194, 164, 216, 255 }, Color{ 158, 126, 195, 255 } };
    const Ramp RoadTerrainDark{ Color{ 242, 189, 140, 255 }, Color{ 226, 143, 112, 255 }, Color{ 207, 103, 116, 255 } };
    const Ramp ForestTerrainDark{ Color{ 128, 207, 169, 255 }, Color{ 86, 173, 142, 255 }, Color{ 58, 138, 133, 255 } };
    const Ramp WaterTerrainDark{ Color{ 122, 223, 242, 235 }, Color{ 87, 165, 214, 235 }, Color{ 91, 124, 204, 235 } };
    const Ramp MountainTerrainDark{ Color{ 236, 207, 255, 255 }, Color{ 203, 164, 226, 255 }, Color{ 171, 124, 201, 255 } };

    const Ramp PlainTerrainLight{ Color{ 246, 232, 217, 255 }, Color{ 233, 202, 163, 255 }, Color{ 221, 186, 133, 255 } };
    const Ramp RoadTerrainLight{ Color{ 247, 216, 196, 255 }, Color{ 232, 181, 157, 255 }, Color{ 217, 154, 126, 255 } };
    const Ramp ForestTerrainLight{ Color{ 168, 213, 186, 255 }, Color{ 127, 182, 154, 255 }, Color{ 92, 141, 137, 255 } };
    const Ramp WaterTerrainLight{ Color{ 169, 222, 249, 225 }, Color{ 137, 194, 217, 225 }, Color{ 94, 155, 203, 225 } };
    const Ramp MountainTerrainLight{ Color{ 217, 196, 208, 255 }, Color{ 198, 174, 200, 255 }, Color{ 169, 141, 184, 255 } };

    const std::array<Ramp, 5> TerrainDark{ PlainTerrainDark, RoadTerrainDark, ForestTerrainDark, WaterTerrainDark, MountainTerrainDark };
    const std::array<Ramp, 5> TerrainLight{ PlainTerrainLight, RoadTerrainLight, ForestTerrainLight, WaterTerrainLight, MountainTerrainLight };
    const Color TerrainSideShadeDark{ 35, 26, 54, 255 };
    const Color TerrainSideShadeLight{ 141, 105, 122, 255 };

    const Color GradientTopDark{ 14, 22, 38, 255 };
    const Color GradientTopLight{ 168, 218, 220, 255 };
    const Color GradientMidDark{ 35, 29, 54, 255 };
    const Color GradientMidLight{ 205, 180, 219, 255 };
    const Color GradientBottomDark{ 57, 43, 78, 255 };
    const Color GradientBottomLight{ 255, 229, 217, 255 };

    const Color Path2DCoreDark{ 255, 209, 139, 248 };
    const Color Path2DCoreLight{ 238, 91, 64, 248 };
    const Color Path2DOutlineDark{ 38, 28, 50, 185 };
    const Color Path2DOutlineLight{ 255, 247, 230, 210 };
    const Color Agent2DOutlineDark{ 20, 18, 32, 170 };
    const Color Agent2DOutlineLight{ 255, 250, 236, 190 };

    const Color TerrainFloorDark{ 38, 31, 54, 255 };
    const Color TerrainFloorLight{ 250, 219, 204, 255 };
    const Color BlockWaterTintDark{ 44, 80, 145, 255 };
    const Color BlockWaterTintLight{ 116, 190, 222, 255 };
    const Color BlockWireDark{ 255, 238, 214, 95 };
    const Color BlockWireLight{ 96, 72, 91, 120 };

    const Color DioramaBaseTopDark{ 52, 42, 72, 255 };
    const Color DioramaBaseTopLight{ 252, 222, 207, 255 };
    const Color DioramaBaseSideDark{ 31, 27, 45, 255 };
    const Color DioramaBaseSideLight{ 214, 174, 158, 255 };
    const Color DioramaBoardTrimDark{ 233, 205, 255, 70 };
    const Color DioramaBoardTrimLight{ 94, 66, 86, 75 };
    const Color DioramaPathDark{ 255, 205, 132, 185 };
    const Color DioramaPathLight{ 238, 91, 64, 172 };
    const Color DioramaCurrentDark{ 255, 136, 112, 185 };
    const Color DioramaCurrentLight{ 231, 82, 62, 172 };
    const Color DioramaOpenDark{ 92, 208, 224, 110 };
    const Color DioramaOpenLight{ 67, 178, 199, 98 };
    const Color DioramaClosedDark{ 122, 111, 158, 90 };
    const Color DioramaClosedLight{ 156, 136, 186, 82 };
    const Color DioramaWaterDark{ 85, 177, 222, 210 };
    const Color DioramaWaterLight{ 98, 188, 226, 198 };
    const Color DioramaRoadDark{ 255, 180, 112, 225 };
    const Color DioramaRoadLight{ 235, 143, 101, 220 };
    const Color DioramaRoadStripeDark{ 255, 236, 184, 175 };
    const Color DioramaRoadStripeLight{ 255, 244, 215, 170 };
    const Color DioramaTrunkDark{ 157, 104, 73, 255 };
    const Color DioramaTrunkLight{ 119, 82, 56, 255 };
    const Color DioramaCanopyDark{ 116, 218, 162, 242 };
    const Color DioramaCanopyLight{ 74, 157, 112, 236 };
    const Color DioramaRockDark{ 210, 190, 236, 238 };
    const Color DioramaRockLight{ 143, 119, 160, 222 };
    const Color DioramaSnowDark{ 242, 226, 255, 232 };
    const Color DioramaSnowLight{ 197, 170, 212, 210 };
    const Color DioramaBuildingDark{ 83, 75, 104, 255 };
    const Color DioramaBuildingLight{ 164, 149, 160, 255 };
    const Color DioramaRoofDark{ 110, 96, 137, 255 };
    const Color DioramaRoofLight{ 190, 173, 185, 255 };

    const Color Path3DCoreDark{ 255, 209, 139, 255 };
    const Color Path3DCoreLight{ 238, 91, 64, 255 };
    const Color Path3DOutlineDark{ 28, 22, 42, 210 };
    const Color Path3DOutlineLight{ 255, 247, 230, 225 };
    const Color GraphEdgeDark{ 121, 218, 232, 84 };
    const Color GraphEdgeLight{ 112, 110, 170, 76 };
    const Color GraphNodeDark{ 255, 245, 214, 118 };
    const Color GraphNodeLight{ 93, 64, 94, 108 };

    const Color FeatureRoadSlabDark{ 220, 143, 113, 210 };
    const Color FeatureRoadSlabLight{ 230, 190, 171, 220 };
    const Color FeatureRoadStripeDark{ 255, 228, 178, 220 };
    const Color FeatureRoadStripeLight{ 255, 243, 212, 220 };
    const Color FeatureTrunkDark{ 154, 103, 72, 255 };
    const Color FeatureTrunkLight{ 122, 83, 57, 255 };
    const Color FeatureCanopyDark{ 121, 216, 167, 245 };
    const Color FeatureCanopyLight{ 85, 159, 121, 245 };
    const Color FeaturePeakDark{ 236, 207, 255, 245 };
    const Color FeaturePeakLight{ 170, 141, 184, 235 };
    const Color FeatureStoneDark{ 204, 164, 226, 230 };
    const Color FeatureStoneLight{ 139, 119, 150, 230 };
    const Color FeatureWaterDark{ 100, 202, 232, 170 };
    const Color FeatureWaterLight{ 121, 201, 228, 170 };

    const Color CellObstacleDark{ 73, 66, 91, 255 };
    const Color CellObstacleLight{ 170, 154, 164, 255 };
    const Color CellClosedDark{ 129, 115, 158, 230 };
    const Color CellClosedLight{ 181, 170, 198, 230 };
    const Color CellOpenDark{ 112, 215, 232, 240 };
    const Color CellOpenLight{ 111, 196, 214, 240 };
    const Color CellPathDark{ 255, 206, 135, 255 };
    const Color CellPathLight{ 244, 162, 97, 255 };
    const Color CellCurrentDark{ 255, 143, 119, 255 };
}
Color Blend(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    auto mix = [t](unsigned char x, unsigned char y) -> unsigned char {
        return static_cast<unsigned char>(x + (y - x) * t);
        };
    Color out{};
    out.r = mix(a.r, b.r);
    out.g = mix(a.g, b.g);
    out.b = mix(a.b, b.b);
    out.a = mix(a.a, b.a);
    return out;
}

Color ScaleColor(Color c, float factor) {
    auto scale = [factor](unsigned char channel) -> unsigned char {
        const float value = static_cast<float>(channel) * factor;
        return static_cast<unsigned char>(std::clamp(value, 0.0f, 255.0f));
        };
    Color out{};
    out.r = scale(c.r);
    out.g = scale(c.g);
    out.b = scale(c.b);
    out.a = c.a;
    return out;
}

Color PickColor(ThemeMode theme, Color darkColor, Color lightColor) {
    return theme == ThemeMode::Dark ? darkColor : lightColor;
}

Color WithAlpha(Color color, unsigned char alpha) {
    color.a = alpha;
    return color;
}
}
