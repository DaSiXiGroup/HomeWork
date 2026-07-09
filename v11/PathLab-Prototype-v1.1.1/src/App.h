#pragma once

#include "GridMap.h"
#include "Pathfinder.h"

#include "raylib.h"

#include <string>
#include <vector>
#include <array>
#include <algorithm>

namespace pathlab{

enum class Theme {
    Light,
    Dark
};

enum class Edit {
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

struct BenchRow {
    std::string algName;
    bool found = false;
    double pathLen   = 0.0;
    int expNodes   = 0;
    int genNodes  = 0;
    double timeMs    = 0.0;
};

struct AgentRow {
    MoveProfile prof = MoveProfile::Pedestrian;
    std::string profName;
    bool found          = false;
    double pathLen   = 0.0;
    int expNodes   = 0;
    double timeMs    = 0.0;
    std::vector<int> path;
};

struct MapSnap {
    GridMap map;
    std::string label;
};

class App {
public:
    App();
    void Run();

private:
    static const int __WIN_W  = 1600;
    static const int __WIN_H = 1000;

    GridMap                         __map;
    SearchRes                    __result;
    AlgOpt                __opt{};
    std::vector<BenchRow>       __benchRows;
    std::vector<AgentRow> __agentRows;
    std::vector<MapSnap>    __undo;
    std::vector<MapSnap>    __redo;

    // var for UI
    bool            __hasRes             = false;
    bool            __play               = false;
    int             __frmIdx            = 0;
    float           __animSpd        = 35.0f;
    float           __obsProb   = 0.28f;
    float           __rowVal          = 35.0f;
    float           __colVal          = 55.0f;
    float           __heurVal  = 1.4f;
    float           __terrVal    = 1.0f;
    float           __hgtVal     = 0.25f;
    float           __acc           = 0.0f;
    float           __scale               = 1.0f;
    bool            __full            = false;
    bool            __is3D                = false;
    Theme       __theme             = Theme::Light;
    int             __selCell          = -1;
    int             __hov3D         = -1;
    float           __hgtScale    = 0.24f;
    float           __zoom2D             = 1.0f;
    Vector2         __pan2D              { 0.0f, 0.0f };
    int             __mapSlot       = 1;
    std::string     __curMapPath;
    float           __lScroll       = 0.0f;
    float           __rScroll      = 0.0f;
    bool            __smooth3D       = false;
    bool            __smoothPath            = true;
    bool            __showGraph      = false;
    bool            __showAgents   = false;
    int             __lastCell        = -1;
    MoveProfile __prof       = MoveProfile::Pedestrian;
    float           __camDist        = 58.0f;
    float           __camYaw             = 0.78f;
    float           __camPitch           = 0.58f;
    Vector3         __camTar          { 0.0f, 0.0f, 0.0f };
    Camera3D        __camera{};
    int             __winW         = __WIN_W;
    int             __winH        = __WIN_H;

    Edit        __edit              = Edit::Wall;
    std::string     __status         = "Ready";

    Font            __font{};
    bool            __fontLoaded      = false;
    std::string     __fontStat            = "default raylib font";

    Rectangle __lPanel{};
    Rectangle __rPanel{};
    Rectangle __canvas{};
    Rectangle __bPanel{};

    void __Layout();
    void __Update();
    void __Draw();
    void __UpdateScale();
    void __ToggleFull();
    void __ToggleView();
    void __ToggleTheme();
    void __ResetCam();
    void __UpdateCam();

    void __DrawTopBar();
    void __DrawLeft();
    void __DrawRight();
    void __DrawBottom();
    void __DrawMap();
    void __Draw3D();
    void __DrawGradBg(Rectangle box) const;
    void __DrawBlock3D(const SearchFrm* frm) const;
    void __DrawSmooth3D(const SearchFrm* frm) const;
    void __DrawPath3D(const std::vector<int>& path) const;
    void __DrawPath3DC(const std::vector<int>& path, Color core, float radius = 0.055f) const;
    void __DrawAgent2D(float startX, float startY, float cellSize) const;
    void __DrawAgent3D() const;
    void __DrawLegend(float x, float& y, float cw);
    void __DrawGraph3D() const;
    void __DrawSel3D(int cell, Color color) const;
    void __DrawFeat3D(int cell) const;
    void __HandleEdit();
    void __EditCell(int cell);

    void __GenRandom();
    void __GenTerrain();
    void __GenMaze();
    void __GenCity();
    void __GenRiver();
    void __GenMountain();
    void __RunDemo();
    void __ApplyMapSize();
    void __RunAlg(AlgKind Type);
    void __RunBench();
    void __CmpAgents();
    void __ExportPng();
    void __SaveMap();
    void __LoadMap();
    void __ExportCsv();
    void __ResetVis();
    void __SyncOpt();
    void __PushUndo(const std::string& label);
    void __ClearHist();
    void __UndoEdit();
    void __RedoEdit();

    void __LoadFont();
    void __UnloadFont();

    bool __Button(Rectangle box, const char* text);
    bool __TogBtn(Rectangle box, const char* text, bool active);
    bool __Slider(Rectangle box, const char* label, float* val, float loVal, float hiVal, bool intDisp = false);

    void    __Text(const std::string& text, float x, float y, float fs, Color color) const;
    void    __TextBold(const std::string& text, float x, float y, float fs, Color color) const;
    float   __MeasureText(const std::string& text, float fs) const;

    [[nodiscard]] float                 __S(float val) const;
    [[nodiscard]] int                   __FrameCnt() const;
    [[nodiscard]] const SearchFrm*    __CurFrame() const;
    [[nodiscard]] int                   __MouseCell(Vector2 mouse) const;
    [[nodiscard]] int                   __Pick3D(Vector2 mouse) const;
    [[nodiscard]] BoundingBox           __CellBox3D(int cell) const;
    [[nodiscard]] float                 __SoftHgtAt(int row, int col) const;
    [[nodiscard]] float                 __CellHgt3D(int cell) const;
    [[nodiscard]] Color                 __CellColor(int cell, const SearchFrm* frm) const;
    [[nodiscard]] Color                 __TerrainColor(Terrain terrain, float h01, bool side = false) const;
    [[nodiscard]] Color                 __PanelColor() const;
    [[nodiscard]] Color                 __PanelDeep() const;
    [[nodiscard]] Color                 __AccentColor() const;
    [[nodiscard]] Color                 __TextColor() const;
    [[nodiscard]] Color                 __MutedColor() const;
    [[nodiscard]] Color                 __GridColor() const;
    [[nodiscard]] Color                 __DangerColor() const;
    [[nodiscard]] Color                 __SuccessColor() const;
    [[nodiscard]] Color                 __CanvasColor() const;
    [[nodiscard]] Color                 __OverlayColor() const;
    [[nodiscard]] Vector3               __CellPos(int cell, float lift = 0.35f) const;
    [[nodiscard]] Vector3               __TerrVtx(int row, int col, float lift = 0.0f) const;
    [[nodiscard]] Vector3               __CatmullRom(Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3, float t) const;
    [[nodiscard]] float                 __CellHgt(int cell) const;
    [[nodiscard]] bool                  __HasCell(const std::vector<int>& values, int cell) const;
    [[nodiscard]] std::string           __EditName() const;
    [[nodiscard]] std::string           __ProfName() const;
    [[nodiscard]] std::string           __ThemeName() const;
    [[nodiscard]] std::string           __CellInfo(int cell) const;
    [[nodiscard]] std::string           __SelMapPath() const;
};

// I carefully choose these color
// see them at https://mhg-lab.github.io/pages/color/
// maybe I'll choose better style
namespace Pal {
    struct Ramp { Color low, mid, high; };

    const unsigned char SelFillA = 42;
    const Color Transparent{ 0, 0, 0, 0 };

    const Color PanelD{ 30, 30, 45, 248 };
    const Color PanelL{ 252, 244, 238, 248 };
    const Color PanelDeepD{ 20, 22, 35, 255 };
    const Color PanelDeepL{ 239, 220, 222, 255 };
    const Color AccentD{ 241, 170, 117, 255 };
    const Color AccentL{ 231, 111, 81, 255 };
    const Color TextD{ 246, 240, 255, 255 };
    const Color TextL{ 64, 55, 75, 255 };
    const Color TextMuteD{ 188, 178, 210, 255 };
    const Color TextMuteL{ 126, 106, 123, 255 };
    const Color GridD{ 93, 76, 121, 120 };
    const Color GridL{ 212, 188, 191, 150 };
    const Color DangerD{ 255, 126, 140, 255 };
    const Color DangerL{ 220, 70, 80, 255 };
    const Color SuccessD{ 120, 220, 170, 255 };
    const Color SuccessL{ 42, 157, 143, 255 };
    const Color CanvasD{ 18, 20, 34, 255 };
    const Color CanvasL{ 255, 237, 226, 255 };
    const Color OverlayD{ 255, 245, 214, 255 };
    const Color OverlayL{ 93, 64, 94, 255 };
    const Color BtnTextOn{ 255, 248, 235, 255 };

    const Color TrackD{ 70, 58, 92, 255 };
    const Color TrackL{ 224, 198, 204, 255 };
    const Color BtnHoverD{ 74, 64, 98, 255 };
    const Color BtnHoverL{ 244, 210, 202, 255 };
    const Color BtnIdleD{ 52, 45, 70, 255 };
    const Color BtnIdleL{ 246, 226, 219, 255 };

    const std::array<Color, 4> ProfD{
        Color{ 255, 214, 139, 245 }, Color{ 119, 216, 255, 245 },
        Color{ 132, 225, 166, 245 }, Color{ 220, 166, 255, 245 }
    };
    const std::array<Color, 4> ProfL{
        Color{ 231, 111, 81, 245 }, Color{ 50, 145, 205, 245 },
        Color{ 59, 158, 112, 245 }, Color{ 138, 91, 198, 245 }
    };
    const Color ProfFallbackD{ 255, 245, 214, 245 };
    const Color ProfFallbackL{ 93, 64, 94, 245 };

    const Ramp PlainD{ Color{ 224, 205, 229, 255 }, Color{ 194, 164, 216, 255 }, Color{ 158, 126, 195, 255 } };
    const Ramp RoadD{ Color{ 242, 189, 140, 255 }, Color{ 226, 143, 112, 255 }, Color{ 207, 103, 116, 255 } };
    const Ramp ForestD{ Color{ 128, 207, 169, 255 }, Color{ 86, 173, 142, 255 }, Color{ 58, 138, 133, 255 } };
    const Ramp WaterD{ Color{ 122, 223, 242, 235 }, Color{ 87, 165, 214, 235 }, Color{ 91, 124, 204, 235 } };
    const Ramp MountainD{ Color{ 236, 207, 255, 255 }, Color{ 203, 164, 226, 255 }, Color{ 171, 124, 201, 255 } };

    const Ramp PlainL{ Color{ 246, 232, 217, 255 }, Color{ 233, 202, 163, 255 }, Color{ 221, 186, 133, 255 } };
    const Ramp RoadL{ Color{ 247, 216, 196, 255 }, Color{ 232, 181, 157, 255 }, Color{ 217, 154, 126, 255 } };
    const Ramp ForestL{ Color{ 168, 213, 186, 255 }, Color{ 127, 182, 154, 255 }, Color{ 92, 141, 137, 255 } };
    const Ramp WaterL{ Color{ 169, 222, 249, 225 }, Color{ 137, 194, 217, 225 }, Color{ 94, 155, 203, 225 } };
    const Ramp MountainL{ Color{ 217, 196, 208, 255 }, Color{ 198, 174, 200, 255 }, Color{ 169, 141, 184, 255 } };

    const std::array<Ramp, 5> TerrD{ PlainD, RoadD, ForestD, WaterD, MountainD };
    const std::array<Ramp, 5> TerrL{ PlainL, RoadL, ForestL, WaterL, MountainL };
    const Color TerrShadeD{ 35, 26, 54, 255 };
    const Color TerrShadeL{ 141, 105, 122, 255 };

    const Color GradTopD{ 14, 22, 38, 255 };
    const Color GradTopL{ 168, 218, 220, 255 };
    const Color GradMidD{ 35, 29, 54, 255 };
    const Color GradMidL{ 205, 180, 219, 255 };
    const Color GradBotD{ 57, 43, 78, 255 };
    const Color GradBotL{ 255, 229, 217, 255 };

    const Color Path2CoreD{ 255, 209, 139, 248 };
    const Color Path2CoreL{ 238, 91, 64, 248 };
    const Color Path2OutD{ 38, 28, 50, 185 };
    const Color Path2OutL{ 255, 247, 230, 210 };
    const Color Agent2OutD{ 20, 18, 32, 170 };
    const Color Agent2OutL{ 255, 250, 236, 190 };

    const Color FloorD{ 38, 31, 54, 255 };
    const Color FloorL{ 250, 219, 204, 255 };
    const Color BlockWaterD{ 44, 80, 145, 255 };
    const Color BlockWaterL{ 116, 190, 222, 255 };
    const Color BlockWireD{ 255, 238, 214, 95 };
    const Color BlockWireL{ 96, 72, 91, 120 };

    const Color DioBaseTopD{ 52, 42, 72, 255 };
    const Color DioBaseTopL{ 252, 222, 207, 255 };
    const Color DioBaseSideD{ 31, 27, 45, 255 };
    const Color DioBaseSideL{ 214, 174, 158, 255 };
    const Color DioTrimD{ 233, 205, 255, 70 };
    const Color DioTrimL{ 94, 66, 86, 75 };
    const Color DioPathD{ 255, 205, 132, 185 };
    const Color DioPathL{ 238, 91, 64, 172 };
    const Color DioCurD{ 255, 136, 112, 185 };
    const Color DioCurL{ 231, 82, 62, 172 };
    const Color DioOpenD{ 92, 208, 224, 110 };
    const Color DioOpenL{ 67, 178, 199, 98 };
    const Color DioClosedD{ 122, 111, 158, 90 };
    const Color DioClosedL{ 156, 136, 186, 82 };
    const Color DioWaterD{ 85, 177, 222, 210 };
    const Color DioWaterL{ 98, 188, 226, 198 };
    const Color DioRoadD{ 255, 180, 112, 225 };
    const Color DioRoadL{ 235, 143, 101, 220 };
    const Color DioStripeD{ 255, 236, 184, 175 };
    const Color DioStripeL{ 255, 244, 215, 170 };
    const Color DioTrunkD{ 157, 104, 73, 255 };
    const Color DioTrunkL{ 119, 82, 56, 255 };
    const Color DioCanopyD{ 116, 218, 162, 242 };
    const Color DioCanopyL{ 74, 157, 112, 236 };
    const Color DioRockD{ 210, 190, 236, 238 };
    const Color DioRockL{ 143, 119, 160, 222 };
    const Color DioSnowD{ 242, 226, 255, 232 };
    const Color DioSnowL{ 197, 170, 212, 210 };
    const Color DioBuildD{ 83, 75, 104, 255 };
    const Color DioBuildL{ 164, 149, 160, 255 };
    const Color DioRoofD{ 110, 96, 137, 255 };
    const Color DioRoofL{ 190, 173, 185, 255 };

    const Color Path3CoreD{ 255, 209, 139, 255 };
    const Color Path3CoreL{ 238, 91, 64, 255 };
    const Color Path3OutD{ 28, 22, 42, 210 };
    const Color Path3OutL{ 255, 247, 230, 225 };
    const Color GraphEdgeD{ 121, 218, 232, 84 };
    const Color GraphEdgeL{ 112, 110, 170, 76 };
    const Color GraphNodeD{ 255, 245, 214, 118 };
    const Color GraphNodeL{ 93, 64, 94, 108 };

    const Color FeatRoadD{ 220, 143, 113, 210 };
    const Color FeatRoadL{ 230, 190, 171, 220 };
    const Color FeatStripeD{ 255, 228, 178, 220 };
    const Color FeatStripeL{ 255, 243, 212, 220 };
    const Color FeatTrunkD{ 154, 103, 72, 255 };
    const Color FeatTrunkL{ 122, 83, 57, 255 };
    const Color FeatCanopyD{ 121, 216, 167, 245 };
    const Color FeatCanopyL{ 85, 159, 121, 245 };
    const Color FeatPeakD{ 236, 207, 255, 245 };
    const Color FeatPeakL{ 170, 141, 184, 235 };
    const Color FeatStoneD{ 204, 164, 226, 230 };
    const Color FeatStoneL{ 139, 119, 150, 230 };
    const Color FeatWaterD{ 100, 202, 232, 170 };
    const Color FeatWaterL{ 121, 201, 228, 170 };

    const Color CellObsD{ 73, 66, 91, 255 };
    const Color CellObsL{ 170, 154, 164, 255 };
    const Color CellClosedD{ 129, 115, 158, 230 };
    const Color CellClosedL{ 181, 170, 198, 230 };
    const Color CellOpenD{ 112, 215, 232, 240 };
    const Color CellOpenL{ 111, 196, 214, 240 };
    const Color CellPathD{ 255, 206, 135, 255 };
    const Color CellPathL{ 244, 162, 97, 255 };
    const Color CellCurD{ 255, 143, 119, 255 };
}
inline Color Blend(Color a, Color b, float t) {
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

inline Color ScaleColor(Color c, float factor) {
    auto scale = [factor](unsigned char channel) -> unsigned char {
        const float val = static_cast<float>(channel) * factor;
        return static_cast<unsigned char>(std::clamp(val, 0.0f, 255.0f));
    };
    Color out{};
    out.r = scale(c.r);
    out.g = scale(c.g);
    out.b = scale(c.b);
    out.a = c.a;
    return out;
}

inline Color PickColor(Theme theme, Color darkColor, Color lightColor) {
    return theme == Theme::Dark ? darkColor : lightColor;
}

inline Color WithAlpha(Color color, unsigned char alpha) {
    color.a = alpha;
    return color;
}
}
