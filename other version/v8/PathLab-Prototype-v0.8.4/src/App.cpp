#include "App.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace pathlab {

namespace {

std::string FormatDouble(double value, int digits = 2) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f", digits, value);
    return std::string(buffer);
}

Color Blend(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    auto mix = [t](unsigned char x, unsigned char y) -> unsigned char {
        return static_cast<unsigned char>(x + (y - x) * t);
    };
    return Color{ mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a) };
}

const char* TerrainName(TerrainType terrain) {
    switch (terrain) {
        case TerrainType::Road: return "Road";
        case TerrainType::Forest: return "Forest";
        case TerrainType::Water: return "Water";
        case TerrainType::Mountain: return "Mountain";
        case TerrainType::Plain:
        default: return "Plain";
    }
}


std::string TrimDialogPath(std::string value) {
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r' || value.back() == ' ' || value.back() == '\t')) {
        value.pop_back();
    }
    while (!value.empty() && (value.front() == '\n' || value.front() == '\r' || value.front() == ' ' || value.front() == '\t')) {
        value.erase(value.begin());
    }
    return value;
}

FILE* OpenPipe(const std::string& command) {
#ifdef _WIN32
    return _popen(command.c_str(), "r");
#else
    return popen(command.c_str(), "r");
#endif
}

int ClosePipe(FILE* pipe) {
#ifdef _WIN32
    return _pclose(pipe);
#else
    return pclose(pipe);
#endif
}

std::string CaptureCommand(const std::string& command) {
    std::string output;
    FILE* pipe = OpenPipe(command);
    if (pipe == nullptr) return output;
    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    ClosePipe(pipe);
    return TrimDialogPath(output);
}

std::string NativeMapFileDialog(bool saveDialog) {
#ifdef _WIN32
    std::string command = saveDialog
        ? "powershell -NoProfile -Sta -ExecutionPolicy Bypass -Command \"Add-Type -AssemblyName System.Windows.Forms; [Console]::OutputEncoding=[System.Text.Encoding]::UTF8; $d=New-Object System.Windows.Forms.SaveFileDialog; $d.Title='Save PathLab map'; $d.Filter='PathLab Map (*.pathmap)|*.pathmap|All Files (*.*)|*.*'; $d.DefaultExt='pathmap'; $d.FileName='map.pathmap'; if($d.ShowDialog() -eq 'OK'){ Write-Output $d.FileName }\""
        : "powershell -NoProfile -Sta -ExecutionPolicy Bypass -Command \"Add-Type -AssemblyName System.Windows.Forms; [Console]::OutputEncoding=[System.Text.Encoding]::UTF8; $d=New-Object System.Windows.Forms.OpenFileDialog; $d.Title='Open PathLab map'; $d.Filter='PathLab Map (*.pathmap)|*.pathmap|All Files (*.*)|*.*'; if($d.ShowDialog() -eq 'OK'){ Write-Output $d.FileName }\"";
    return CaptureCommand(command);
#elif defined(__APPLE__)
    std::string command = saveDialog
        ? "osascript -e 'POSIX path of (choose file name with prompt \"Save PathLab map\" default name \"map.pathmap\")' 2>/dev/null"
        : "osascript -e 'POSIX path of (choose file with prompt \"Open PathLab map\")' 2>/dev/null";
    return CaptureCommand(command);
#else
    std::string command = saveDialog
        ? "zenity --file-selection --save --confirm-overwrite --title='Save PathLab map' --filename='map.pathmap' 2>/dev/null"
        : "zenity --file-selection --title='Open PathLab map' 2>/dev/null";
    return CaptureCommand(command);
#endif
}

std::string EnsurePathMapExtension(std::string path) {
    if (path.empty()) return path;
    std::filesystem::path p(path);
    if (p.extension().empty()) p.replace_extension(".pathmap");
    return p.string();
}

std::string CompactDisplayPath(const std::string& path) {
    if (path.empty()) return "No file selected";
    std::filesystem::path p(path);
    std::string name = p.filename().string();
    if (name.empty()) name = path;
    return name.size() > 34 ? (name.substr(0, 31) + "...") : name;
}

std::vector<std::string> FontCandidates() {
    return {
        "assets/fonts/ui.ttf",
        "assets/fonts/ui.otf",
        "../assets/fonts/ui.ttf",
        "../assets/fonts/ui.otf",
        "../../assets/fonts/ui.ttf",
        "../../assets/fonts/ui.otf",
        "../../../assets/fonts/ui.ttf",
        "../../../assets/fonts/ui.otf",
        "C:/Windows/Fonts/seguisb.ttf",
        "C:/Windows/Fonts/segoeuib.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arialbd.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial Bold.ttf",
        "/Library/Fonts/Arial.ttf"
    };
}
} // namespace

App::App() : map_(35, 55) {
    GenerateRandomMap();
}

void App::Run() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_WINDOW_MAXIMIZED);
    InitWindow(kWindowWidth, kWindowHeight, "PathLab Prototype v0.8.4 - file dialogs + clearer controls");
    SetWindowMinSize(1280, 760);
    MaximizeWindow();
    SetTargetFPS(60);
    uiFont_ = GetFontDefault();
    LoadUIFont();
    ResetCamera3D();

    while (!WindowShouldClose()) {
        Layout();
        Update();
        Draw();
    }

    UnloadUIFont();
    CloseWindow();
}

void App::Layout() {
    UpdateUIScale();

    const float topH = S(64.0f);
    const float bottomH = S(96.0f);
    const float leftW = S(360.0f);
    const float rightW = S(410.0f);

    leftPanel_ = { 0, topH, leftW, static_cast<float>(GetScreenHeight()) - topH - bottomH };
    rightPanel_ = { static_cast<float>(GetScreenWidth()) - rightW, topH, rightW, static_cast<float>(GetScreenHeight()) - topH - bottomH };
    canvas_ = { leftW, topH, static_cast<float>(GetScreenWidth()) - leftW - rightW, static_cast<float>(GetScreenHeight()) - topH - bottomH };
    bottomPanel_ = { 0, static_cast<float>(GetScreenHeight()) - bottomH, static_cast<float>(GetScreenWidth()), bottomH };
}

void App::Update() {
    if (IsKeyPressed(KEY_F11)) {
        ToggleFullscreenMode();
    }
    if (IsKeyPressed(KEY_F10) && !fullscreen_) {
        MaximizeWindow();
    }
    if (IsKeyPressed(KEY_F5)) {
        ToggleViewMode();
    }
    if (IsKeyPressed(KEY_F6)) {
        ToggleThemeMode();
    }
    const Vector2 mouse = GetMousePosition();
    const float wheel = GetMouseWheelMove();
    if (std::abs(wheel) > 0.001f && CheckCollisionPointRec(mouse, leftPanel_)) {
        leftPanelScroll_ = std::clamp(leftPanelScroll_ + wheel * S(54.0f), -S(520.0f), 0.0f);
    } else if (std::abs(wheel) > 0.001f && CheckCollisionPointRec(mouse, rightPanel_)) {
        rightPanelScroll_ = std::clamp(rightPanelScroll_ + wheel * S(54.0f), -S(420.0f), 0.0f);
    } else if (view3D_) {
        UpdateCamera3D();
    } else if (CheckCollisionPointRec(mouse, canvas_)) {
        if (std::abs(wheel) > 0.001f) {
            const float oldZoom = mapZoom2D_;
            mapZoom2D_ *= std::pow(1.12f, wheel);
            mapZoom2D_ = std::clamp(mapZoom2D_, 0.55f, 5.0f);
            const float zoomRatio = mapZoom2D_ / oldZoom;
            Vector2 center{ canvas_.x + canvas_.width * 0.5f, canvas_.y + canvas_.height * 0.5f };
            mapPan2D_.x = mouse.x - center.x - (mouse.x - center.x - mapPan2D_.x) * zoomRatio;
            mapPan2D_.y = mouse.y - center.y - (mouse.y - center.y - mapPan2D_.y) * zoomRatio;
            statusMessage_ = "2D zoom: " + FormatDouble(mapZoom2D_, 2) + "x";
        }
        if (IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
            Vector2 delta = GetMouseDelta();
            mapPan2D_.x += delta.x;
            mapPan2D_.y += delta.y;
        }
    }

    HandleMapEditing();

    if (playing_ && hasResult_ && FrameCount() > 0) {
        accumulator_ += GetFrameTime();
        const float interval = 1.0f / std::max(1.0f, animationSpeed_);
        while (accumulator_ >= interval) {
            accumulator_ -= interval;
            frameIndex_++;
            if (frameIndex_ >= FrameCount() - 1) {
                frameIndex_ = FrameCount() - 1;
                playing_ = false;
                statusMessage_ = "Animation finished";
                break;
            }
        }
    }
}

void App::Draw() {
    BeginDrawing();
    ClearBackground(CanvasColor());

    DrawTopBar();
    DrawLeftPanel();
    DrawRightPanel();
    DrawBottomPanel();
    if (view3D_) {
        DrawTerrain3D();
    } else {
        DrawMapCanvas();
    }

    EndDrawing();
}

void App::UpdateUIScale() {
    const float widthScale = static_cast<float>(GetScreenWidth()) / 1440.0f;
    const float heightScale = static_cast<float>(GetScreenHeight()) / 900.0f;
    uiScale_ = std::clamp(std::min(widthScale, heightScale), 0.95f, 1.35f);
}

void App::ToggleFullscreenMode() {
    if (!fullscreen_) {
        windowedWidth_ = GetScreenWidth();
        windowedHeight_ = GetScreenHeight();
        const int monitor = GetCurrentMonitor();
        SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
        ToggleFullscreen();
        fullscreen_ = true;
        statusMessage_ = "Fullscreen enabled; press F11 to return";
    } else {
        ToggleFullscreen();
        SetWindowSize(windowedWidth_, windowedHeight_);
        fullscreen_ = false;
        statusMessage_ = "Windowed mode restored";
    }
}

void App::ToggleViewMode() {
    view3D_ = !view3D_;
    if (view3D_) {
        ResetCamera3D();
        statusMessage_ = "3D terrain view enabled. Left click selects/edits; right or middle drag rotates.";
    } else {
        statusMessage_ = "2D map view enabled. Editing tools are available.";
    }
}

void App::ToggleThemeMode() {
    themeMode_ = (themeMode_ == ThemeMode::Light) ? ThemeMode::Dark : ThemeMode::Light;
    statusMessage_ = "Theme switched to " + ThemeName() + ".";
}

Color App::PanelColor() const {
    return themeMode_ == ThemeMode::Dark ? Color{ 30, 30, 45, 248 } : Color{ 252, 244, 238, 248 };
}

Color App::DarkPanelColor() const {
    return themeMode_ == ThemeMode::Dark ? Color{ 20, 22, 35, 255 } : Color{ 239, 220, 222, 255 };
}

Color App::AccentColor() const {
    return themeMode_ == ThemeMode::Dark ? Color{ 241, 170, 117, 255 } : Color{ 231, 111, 81, 255 };
}

Color App::TextColor() const {
    return themeMode_ == ThemeMode::Dark ? Color{ 246, 240, 255, 255 } : Color{ 64, 55, 75, 255 };
}

Color App::MutedTextColor() const {
    return themeMode_ == ThemeMode::Dark ? Color{ 188, 178, 210, 255 } : Color{ 126, 106, 123, 255 };
}

Color App::GridLineColor() const {
    return themeMode_ == ThemeMode::Dark ? Color{ 93, 76, 121, 120 } : Color{ 212, 188, 191, 150 };
}

Color App::DangerColor() const {
    return themeMode_ == ThemeMode::Dark ? Color{ 255, 126, 140, 255 } : Color{ 220, 70, 80, 255 };
}

Color App::SuccessColor() const {
    return themeMode_ == ThemeMode::Dark ? Color{ 120, 220, 170, 255 } : Color{ 42, 157, 143, 255 };
}

Color App::CanvasColor() const {
    return themeMode_ == ThemeMode::Dark ? Color{ 18, 20, 34, 255 } : Color{ 255, 237, 226, 255 };
}

Color App::OverlayColor() const {
    return themeMode_ == ThemeMode::Dark ? Color{ 255, 245, 214, 255 } : Color{ 93, 64, 94, 255 };
}

Color App::TerrainVisualColor(TerrainType terrain, float heightNorm, bool sideFace) const {
    heightNorm = std::clamp(heightNorm, 0.0f, 1.0f);

    Color low{};
    Color mid{};
    Color high{};

    if (themeMode_ == ThemeMode::Dark) {
        switch (terrain) {
            case TerrainType::Road:
                low = Color{ 242, 189, 140, 255 }; mid = Color{ 226, 143, 112, 255 }; high = Color{ 207, 103, 116, 255 }; break;
            case TerrainType::Forest:
                low = Color{ 128, 207, 169, 255 }; mid = Color{ 86, 173, 142, 255 }; high = Color{ 58, 138, 133, 255 }; break;
            case TerrainType::Water:
                low = Color{ 122, 223, 242, 235 }; mid = Color{ 87, 165, 214, 235 }; high = Color{ 91, 124, 204, 235 }; break;
            case TerrainType::Mountain:
                // Keep mountains deliberately light in dark mode so F5/3D never blends into the background.
                low = Color{ 236, 207, 255, 255 }; mid = Color{ 203, 164, 226, 255 }; high = Color{ 171, 124, 201, 255 }; break;
            case TerrainType::Plain:
            default:
                low = Color{ 224, 205, 229, 255 }; mid = Color{ 194, 164, 216, 255 }; high = Color{ 158, 126, 195, 255 }; break;
        }
    } else {
        switch (terrain) {
            case TerrainType::Road:
                low = Color{ 247, 216, 196, 255 }; mid = Color{ 232, 181, 157, 255 }; high = Color{ 217, 154, 126, 255 }; break;
            case TerrainType::Forest:
                low = Color{ 168, 213, 186, 255 }; mid = Color{ 127, 182, 154, 255 }; high = Color{ 92, 141, 137, 255 }; break;
            case TerrainType::Water:
                low = Color{ 169, 222, 249, 225 }; mid = Color{ 137, 194, 217, 225 }; high = Color{ 94, 155, 203, 225 }; break;
            case TerrainType::Mountain:
                low = Color{ 217, 196, 208, 255 }; mid = Color{ 198, 174, 200, 255 }; high = Color{ 169, 141, 184, 255 }; break;
            case TerrainType::Plain:
            default:
                low = Color{ 246, 232, 217, 255 }; mid = Color{ 233, 202, 163, 255 }; high = Color{ 221, 186, 133, 255 }; break;
        }
    }

    Color color = heightNorm < 0.55f
        ? Blend(low, mid, heightNorm / 0.55f)
        : Blend(mid, high, (heightNorm - 0.55f) / 0.45f);

    if (sideFace) {
        Color shade = themeMode_ == ThemeMode::Dark ? Color{ 35, 26, 54, color.a } : Color{ 141, 105, 122, color.a };
        color = Blend(color, shade, themeMode_ == ThemeMode::Dark ? 0.22f : 0.18f);
    }
    return color;
}

void App::DrawGradientBackground(Rectangle bounds) const {
    Color top = themeMode_ == ThemeMode::Dark ? Color{ 14, 22, 38, 255 } : Color{ 168, 218, 220, 255 };
    Color mid = themeMode_ == ThemeMode::Dark ? Color{ 35, 29, 54, 255 } : Color{ 205, 180, 219, 255 };
    Color bottom = themeMode_ == ThemeMode::Dark ? Color{ 57, 43, 78, 255 } : Color{ 255, 229, 217, 255 };

    const int strips = 96;
    for (int i = 0; i < strips; ++i) {
        float t0 = static_cast<float>(i) / static_cast<float>(strips);
        float t1 = static_cast<float>(i + 1) / static_cast<float>(strips);
        float midT = (t0 + t1) * 0.5f;
        Color c = midT < 0.52f ? Blend(top, mid, midT / 0.52f) : Blend(mid, bottom, (midT - 0.52f) / 0.48f);
        Rectangle strip{ bounds.x, bounds.y + bounds.height * t0, bounds.width, bounds.height * (t1 - t0) + 1.0f };
        DrawRectangleRec(strip, c);
    }
}

void App::ResetCamera3D() {
    const float mapSize = static_cast<float>(std::max(map_.Rows(), map_.Cols()));
    cameraTarget_ = { 0.0f, smoothTerrain3D_ ? 1.15f : 0.0f, 0.0f };
    cameraDistance_ = std::max(smoothTerrain3D_ ? 22.0f : 28.0f, mapSize * (smoothTerrain3D_ ? 0.66f : 0.95f));
    cameraYaw_ = 0.78f;
    // Soft heightfield needs a lower grazing angle; otherwise it reads like a flat color patch.
    cameraPitch_ = smoothTerrain3D_ ? 0.36f : 0.58f;

    camera_.up = { 0.0f, 1.0f, 0.0f };
    camera_.fovy = smoothTerrain3D_ ? 38.0f : 45.0f;
    camera_.projection = CAMERA_PERSPECTIVE;
    UpdateCamera3D();
}

void App::UpdateCamera3D() {
    if (IsKeyPressed(KEY_R) && CheckCollisionPointRec(GetMousePosition(), canvas_)) {
        ResetCamera3D();
    }

    if (CheckCollisionPointRec(GetMousePosition(), canvas_)) {
        Vector2 delta = GetMouseDelta();
        if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON) || IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
            cameraYaw_ -= delta.x * 0.008f;
            cameraPitch_ += delta.y * 0.008f;
            cameraPitch_ = std::clamp(cameraPitch_, 0.18f, 1.34f);
        }

        float wheel = GetMouseWheelMove();
        if (std::abs(wheel) > 0.001f) {
            cameraDistance_ *= (1.0f - wheel * 0.10f);
            cameraDistance_ = std::clamp(cameraDistance_, 12.0f, 180.0f);
        }
    }

    float move = GetFrameTime() * cameraDistance_ * 0.55f;
    Vector3 forward{ static_cast<float>(std::sin(cameraYaw_)), 0.0f, static_cast<float>(std::cos(cameraYaw_)) };
    Vector3 right{ static_cast<float>(std::cos(cameraYaw_)), 0.0f, static_cast<float>(-std::sin(cameraYaw_)) };
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) move *= 2.0f;

    if (IsKeyDown(KEY_W)) {
        cameraTarget_.x += forward.x * move;
        cameraTarget_.z += forward.z * move;
    }
    if (IsKeyDown(KEY_S)) {
        cameraTarget_.x -= forward.x * move;
        cameraTarget_.z -= forward.z * move;
    }
    if (IsKeyDown(KEY_D)) {
        cameraTarget_.x += right.x * move;
        cameraTarget_.z += right.z * move;
    }
    if (IsKeyDown(KEY_A)) {
        cameraTarget_.x -= right.x * move;
        cameraTarget_.z -= right.z * move;
    }
    if (IsKeyDown(KEY_E)) cameraTarget_.y += move * 0.35f;
    if (IsKeyDown(KEY_Q)) cameraTarget_.y -= move * 0.35f;

    const float cp = static_cast<float>(std::cos(cameraPitch_));
    camera_.target = cameraTarget_;
    camera_.position = {
        cameraTarget_.x + cameraDistance_ * static_cast<float>(std::sin(cameraYaw_)) * cp,
        cameraTarget_.y + cameraDistance_ * static_cast<float>(std::sin(cameraPitch_)),
        cameraTarget_.z + cameraDistance_ * static_cast<float>(std::cos(cameraYaw_)) * cp
    };
}

void App::DrawTopBar() {
    DrawRectangle(0, 0, GetScreenWidth(), static_cast<int>(S(64.0f)), DarkPanelColor());
    DrawTextUIBold("PathLab", S(20), S(15), 27, TextColor());
    DrawTextUI("v0.8.3", S(128), S(24), 15, MutedTextColor());

    const float buttonW = S(132.0f);
    const float buttonH = S(36.0f);
    const float gap = S(12.0f);
    float cx = static_cast<float>(GetScreenWidth()) * 0.5f - (buttonW * 3.0f + gap * 2.0f) * 0.5f;
    if (Button({ cx, S(14.0f), buttonW, buttonH }, view3D_ ? "View: 3D" : "View: 2D")) {
        ToggleViewMode();
    }
    if (Button({ cx + buttonW + gap, S(14.0f), buttonW, buttonH }, themeMode_ == ThemeMode::Dark ? "Theme: Dark" : "Theme: Light")) {
        ToggleThemeMode();
    }
    if (Button({ cx + (buttonW + gap) * 2.0f, S(14.0f), buttonW, buttonH }, view3D_ ? "Reset 3D" : "Reset 2D")) {
        if (view3D_) {
            ResetCamera3D();
            statusMessage_ = "3D camera reset";
        } else {
            mapZoom2D_ = 1.0f;
            mapPan2D_ = { 0.0f, 0.0f };
            statusMessage_ = "2D view reset";
        }
    }

    const char* shortcuts = fullscreen_ ? "F11 exit | F5 view | F6 theme | wheel zoom" : "F11 fullscreen | F10 maximize | F5 view | F6 theme | wheel zoom";
    DrawTextUI(shortcuts, static_cast<float>(GetScreenWidth()) - S(610.0f), S(23.0f), 14, MutedTextColor());
}

void App::DrawLeftPanel() {
    DrawRectangleRec(leftPanel_, PanelColor());
    DrawRectangleLinesEx(leftPanel_, 1.0f, GridLineColor());
    BeginScissorMode(static_cast<int>(leftPanel_.x), static_cast<int>(leftPanel_.y), static_cast<int>(leftPanel_.width), static_cast<int>(leftPanel_.height));

    float x = leftPanel_.x + S(22.0f);
    float y = leftPanel_.y + S(18.0f) + leftPanelScroll_;
    const float contentW = leftPanel_.width - S(40.0f);
    const float gap = S(10.0f);
    const float bw = (contentW - gap) * 0.5f;

    auto section = [&](const char* title) {
        DrawTextUIBold(title, x, y, 20, TextColor());
        DrawRectangleRec({ x, y + S(29.0f), contentW, 1.0f }, GridLineColor());
        y += S(42.0f);
    };

    section("MAP");
    if (Button({ x, y, bw, S(34) }, "Random")) GenerateRandomMap();
    if (Button({ x + bw + gap, y, bw, S(34) }, "Terrain")) GenerateTerrainMap();
    y += S(40);
    if (Button({ x, y, bw, S(34) }, "Maze")) GenerateMazeMap();
    if (Button({ x + bw + gap, y, bw, S(34) }, "Clear")) {
        map_.ClearAll();
        ResetVisualization();
        statusMessage_ = "Map cleared";
    }
    y += S(40);
    Slider({ x, y, contentW, S(24) }, "Rows", &mapRowsValue_, 10.0f, 80.0f, true);
    y += S(38);
    Slider({ x, y, contentW, S(24) }, "Columns", &mapColsValue_, 10.0f, 120.0f, true);
    y += S(38);
    Slider({ x, y, contentW, S(24) }, "Obstacle", &obstacleProbability_, 0.0f, 0.65f);
    y += S(40);
    if (Button({ x, y, contentW, S(34) }, "Apply map size")) ApplyMapSize();
    y += S(46);

    section("ALGORITHMS");
    if (Button({ x, y, bw, S(34) }, "BFS")) RunAlgorithm(AlgorithmKind::BFS);
    if (Button({ x + bw + gap, y, bw, S(34) }, "Dijkstra")) RunAlgorithm(AlgorithmKind::Dijkstra);
    y += S(40);
    if (Button({ x, y, bw, S(34) }, "A*")) RunAlgorithm(AlgorithmKind::AStar);
    if (Button({ x + bw + gap, y, bw, S(34) }, "Weighted A*")) RunAlgorithm(AlgorithmKind::WeightedAStar);
    y += S(40);
    if (Button({ x, y, bw, S(34) }, "Greedy")) RunAlgorithm(AlgorithmKind::GreedyBestFirst);
    if (Button({ x + bw + gap, y, bw, S(34) }, "Benchmark")) RunBenchmark();
    y += S(46);

    section("AGENT & COST");
    if (ToggleButton({ x, y, contentW, S(34) }, options_.allowDiagonal ? "Diagonal movement: ON" : "Diagonal movement: OFF", options_.allowDiagonal)) {
        options_.allowDiagonal = !options_.allowDiagonal;
        statusMessage_ = options_.allowDiagonal ? "Diagonal movement enabled" : "Diagonal movement disabled";
    }
    y += S(40);
    if (ToggleButton({ x, y, bw, S(34) }, "Pedestrian", movementProfile_ == MovementProfile::Pedestrian)) movementProfile_ = MovementProfile::Pedestrian;
    if (ToggleButton({ x + bw + gap, y, bw, S(34) }, "Car", movementProfile_ == MovementProfile::Car)) movementProfile_ = MovementProfile::Car;
    y += S(40);
    if (ToggleButton({ x, y, bw, S(34) }, "Offroad", movementProfile_ == MovementProfile::Offroad)) movementProfile_ = MovementProfile::Offroad;
    if (ToggleButton({ x + bw + gap, y, bw, S(34) }, "Drone", movementProfile_ == MovementProfile::Drone)) movementProfile_ = MovementProfile::Drone;
    y += S(38);
    Slider({ x, y, contentW, S(24) }, "Heuristic", &heuristicWeightValue_, 1.0f, 5.0f);
    y += S(36);
    Slider({ x, y, contentW, S(24) }, "Terrain cost", &terrainWeightValue_, 0.0f, 5.0f);
    y += S(36);
    Slider({ x, y, contentW, S(24) }, "Height cost", &heightWeightValue_, 0.0f, 2.0f);
    y += S(40);

    section("VIEW");
    Slider({ x, y, contentW, S(24) }, "2D zoom", &mapZoom2D_, 0.55f, 5.0f);
    y += S(36);
    Slider({ x, y, contentW, S(24) }, "3D height", &terrainHeightScale_, 0.08f, 0.90f);
    y += S(40);
    if (ToggleButton({ x, y, bw, S(34) }, smoothTerrain3D_ ? "Surface: soft" : "Surface: solid", smoothTerrain3D_)) {
        smoothTerrain3D_ = !smoothTerrain3D_;
        if (view3D_) ResetCamera3D();
        statusMessage_ = smoothTerrain3D_ ? "Soft terrain mode: camera adjusted for heightfield readability" : "Solid terrain mode enabled";
    }
    if (ToggleButton({ x + bw + gap, y, bw, S(34) }, smoothPath_ ? "Path: curve" : "Path: polyline", smoothPath_)) smoothPath_ = !smoothPath_;
    y += S(40);
    if (ToggleButton({ x, y, contentW, S(34) }, showGraphOverlay_ ? "Algorithm graph: ON" : "Algorithm graph: OFF", showGraphOverlay_)) showGraphOverlay_ = !showGraphOverlay_;
    EndScissorMode();
}

void App::DrawRightPanel() {
    DrawRectangleRec(rightPanel_, PanelColor());
    DrawRectangleLinesEx(rightPanel_, 1.0f, GridLineColor());
    BeginScissorMode(static_cast<int>(rightPanel_.x), static_cast<int>(rightPanel_.y), static_cast<int>(rightPanel_.width), static_cast<int>(rightPanel_.height));

    float x = rightPanel_.x + S(22.0f);
    float y = rightPanel_.y + S(18.0f) + rightPanelScroll_;
    const float contentW = rightPanel_.width - S(40.0f);
    const float gap = S(8.0f);
    const float bw = (contentW - gap * 2.0f) / 3.0f;
    const float gh = S(34.0f);

    auto section = [&](const char* title) {
        DrawTextUIBold(title, x, y, 20, TextColor());
        DrawRectangleRec({ x, y + S(29.0f), contentW, 1.0f }, GridLineColor());
        y += S(42.0f);
    };

    section("TOOLS");
    auto tool = [&](EditMode mode, const char* label, int col, int row) {
        if (ToggleButton({ x + col * (bw + gap), y + row * (gh + gap), bw, gh }, label, editMode_ == mode)) {
            editMode_ = mode;
        }
    };
    tool(EditMode::Select, "Select", 0, 0);
    tool(EditMode::Wall, "Wall", 1, 0);
    tool(EditMode::Erase, "Erase", 2, 0);
    tool(EditMode::Start, "Start", 0, 1);
    tool(EditMode::Goal, "Goal", 1, 1);
    tool(EditMode::Road, "Road", 2, 1);
    tool(EditMode::Forest, "Forest", 0, 2);
    tool(EditMode::Mountain, "Mountain", 1, 2);
    tool(EditMode::Water, "Water", 2, 2);
    tool(EditMode::Plain, "Plain", 0, 3);
    tool(EditMode::Raise, "Height+", 1, 3);
    tool(EditMode::Lower, "Height-", 2, 3);
    y += 4 * (gh + gap) + S(12);

    DrawTextUI("Mode: " + EditModeName(), x, y, 14, MutedTextColor());
    y += S(24);
    DrawTextUI(CellInfo(selectedCell_), x, y, 14, selectedCell_ >= 0 ? TextColor() : MutedTextColor());
    y += S(38);

    section("PLAYBACK");
    if (Button({ x, y, bw, gh }, playing_ ? "Pause" : "Play")) {
        if (hasResult_) playing_ = !playing_;
    }
    if (Button({ x + bw + gap, y, bw, gh }, "Reset")) {
        frameIndex_ = 0;
        playing_ = false;
    }
    if (Button({ x + (bw + gap) * 2.0f, y, bw, gh }, "End")) {
        if (FrameCount() > 0) frameIndex_ = FrameCount() - 1;
        playing_ = false;
    }
    y += S(40);
    if (Button({ x, y, bw, gh }, "Prev")) {
        frameIndex_ = std::max(0, frameIndex_ - 1);
        playing_ = false;
    }
    if (Button({ x + bw + gap, y, bw, gh }, "Next")) {
        frameIndex_ = std::min(FrameCount() - 1, frameIndex_ + 1);
        playing_ = false;
    }
    if (Button({ x + (bw + gap) * 2.0f, y, bw, gh }, "Replay")) {
        if (hasResult_) {
            frameIndex_ = 0;
            playing_ = true;
        }
    }
    y += S(40);
    Slider({ x, y, contentW, S(24) }, "Speed", &animationSpeed_, 1.0f, 160.0f, true);
    y += S(46);

    section("OUTPUT");
    const float halfW = (contentW - gap) * 0.5f;
    if (Button({ x, y, halfW, gh }, "Save as...")) SaveCurrentMap();
    if (Button({ x + halfW + gap, y, halfW, gh }, "Open...")) LoadCurrentMap();
    y += S(40);
    if (Button({ x, y, contentW, gh }, "Export benchmark CSV")) ExportBenchmarkCsv();
    y += S(38);
    DrawTextUI("Map file: " + CompactDisplayPath(currentMapPath_), x, y, 14, MutedTextColor());
    y += S(34);

    section("STATS");
    std::string algorithm = hasResult_ ? result_.algorithmName : "None";
    DrawTextUI("Algorithm: " + algorithm, x, y, 14, TextColor());
    y += S(24);
    DrawTextUI("Agent: " + MovementProfileName(), x, y, 14, TextColor());
    y += S(24);
    DrawTextUI("Frame: " + std::to_string(FrameCount() == 0 ? 0 : frameIndex_ + 1) + " / " + std::to_string(FrameCount()), x, y, 14, TextColor());
    y += S(24);

    if (hasResult_) {
        DrawTextUI("Found: " + std::string(result_.found ? "Yes" : "No") + " | Cost: " + FormatDouble(result_.pathLength, 2), x, y, 14, result_.found ? SuccessColor() : DangerColor());
        y += S(24);
        DrawTextUI("Expanded: " + std::to_string(result_.expandedNodes) + " | Generated: " + std::to_string(result_.generatedNodes), x, y, 14, TextColor());
        y += S(24);
        DrawTextUI("Time: " + FormatDouble(result_.elapsedMs, 3) + " ms", x, y, 14, TextColor());
        y += S(23);
    } else {
        DrawTextUI("Run an algorithm to see data.", x, y, 14, MutedTextColor());
        y += S(26);
    }

    const SearchFrame* frame = CurrentFrame();
    if (frame != nullptr) {
        DrawTextUI("Open: " + std::to_string(frame->open.size()) + " | Closed: " + std::to_string(frame->closed.size()) + " | Current: " + std::to_string(frame->current), x, y, 14, TextColor());
        y += S(28);
    }

    DrawTextUI("BENCHMARK", x, y, 16, TextColor());
    y += S(24);
    if (benchmarkRows_.empty()) {
        DrawTextUI("Click Benchmark to compare algorithms.", x, y, 13, MutedTextColor());
    } else {
        DrawTextUI("Algorithm        Cost    Expanded   Time", x, y, 12, MutedTextColor());
        y += S(18);
        for (const auto& row : benchmarkRows_) {
            char buffer[256];
            std::snprintf(buffer, sizeof(buffer), "%-13s %7.2f %8d %7.3f", row.algorithmName.c_str(), row.pathLength, row.expandedNodes, row.elapsedMs);
            DrawTextUI(buffer, x, y, 12, row.found ? TextColor() : DangerColor());
            y += S(17);
        }
    }
    EndScissorMode();
}

void App::DrawBottomPanel() {
    DrawRectangleRec(bottomPanel_, DarkPanelColor());
    DrawRectangleLinesEx(bottomPanel_, 1.0f, GridLineColor());

    float x = leftPanel_.width + S(24.0f);
    float y = bottomPanel_.y + S(15.0f);
    float rightLimit = rightPanel_.x - S(24.0f);
    DrawTextUI("Timeline", S(20), y, 20, TextColor());

    Rectangle bar{ x, bottomPanel_.y + S(22), std::max(S(160.0f), rightLimit - x), S(10) };
    DrawRectangleRounded(bar, 0.4f, 8, themeMode_ == ThemeMode::Dark ? Color{ 70, 58, 92, 255 } : Color{ 224, 198, 204, 255 });

    float t = 0.0f;
    if (FrameCount() > 1) {
        t = static_cast<float>(frameIndex_) / static_cast<float>(FrameCount() - 1);
    }
    Rectangle progress = bar;
    progress.width *= t;
    DrawRectangleRounded(progress, 0.4f, 8, AccentColor());

    float knobX = bar.x + bar.width * t;
    DrawCircle(static_cast<int>(knobX), static_cast<int>(bar.y + bar.height / 2.0f), 8.0f, themeMode_ == ThemeMode::Dark ? Color{ 255, 245, 214, 255 } : Color{ 93, 64, 94, 255 });

    if (CheckCollisionPointRec(GetMousePosition(), { bar.x - 8, bar.y - 14, bar.width + 16, bar.height + 28 }) && IsMouseButtonDown(MOUSE_LEFT_BUTTON) && FrameCount() > 1) {
        float ratio = (GetMouseX() - bar.x) / bar.width;
        ratio = std::clamp(ratio, 0.0f, 1.0f);
        frameIndex_ = static_cast<int>(ratio * static_cast<float>(FrameCount() - 1));
        playing_ = false;
    }

    std::string info = hasResult_
        ? (result_.algorithmName + " | step " + std::to_string(frameIndex_ + 1) + " / " + std::to_string(FrameCount()))
        : "Generate a map, choose a tool, then run an algorithm.";
    DrawTextUI(info, x, bottomPanel_.y + S(47.0f), 14, MutedTextColor());
    DrawTextUI("Status: " + statusMessage_, S(20), bottomPanel_.y + S(55.0f), 14, MutedTextColor());

    const char* help = view3D_
        ? "3D: left select/edit | right/middle rotate | wheel zoom | WASD pan | R reset"
        : "2D: wheel zoom | middle drag pan | left paint | right erase | Shift start | Ctrl goal";
    float helpW = MeasureTextUI(help, 13);
    DrawTextUI(help, static_cast<float>(GetScreenWidth()) - helpW - S(20.0f), bottomPanel_.y + S(55.0f), 13, MutedTextColor());
}

void App::DrawMapCanvas() {
    DrawRectangleRec(canvas_, CanvasColor());
    DrawRectangleLinesEx(canvas_, 1.0f, GridLineColor());

    const float padding = S(28.0f);
    const float usableW = canvas_.width - 2.0f * padding;
    const float usableH = canvas_.height - 2.0f * padding;
    const float baseCellSize = std::floor(std::min(usableW / static_cast<float>(map_.Cols()), usableH / static_cast<float>(map_.Rows())));
    const float cellSize = std::max(3.0f, baseCellSize * mapZoom2D_);
    if (cellSize <= 0.0f) return;

    const float gridW = cellSize * static_cast<float>(map_.Cols());
    const float gridH = cellSize * static_cast<float>(map_.Rows());
    const float startX = canvas_.x + (canvas_.width - gridW) / 2.0f + mapPan2D_.x;
    const float startY = canvas_.y + (canvas_.height - gridH) / 2.0f + mapPan2D_.y;

    const SearchFrame* frame = CurrentFrame();

    BeginScissorMode(static_cast<int>(canvas_.x), static_cast<int>(canvas_.y), static_cast<int>(canvas_.width), static_cast<int>(canvas_.height));
    for (int r = 0; r < map_.Rows(); ++r) {
        for (int c = 0; c < map_.Cols(); ++c) {
            int idx = map_.Index(r, c);
            Rectangle rect{ startX + static_cast<float>(c) * cellSize, startY + static_cast<float>(r) * cellSize, cellSize, cellSize };
            if (rect.x + rect.width < canvas_.x || rect.y + rect.height < canvas_.y || rect.x > canvas_.x + canvas_.width || rect.y > canvas_.y + canvas_.height) continue;
            DrawRectangleRec(rect, CellColor(idx, frame));
            if (cellSize >= 8.0f) DrawRectangleLinesEx(rect, 1.0f, GridLineColor());

            const Cell& cell = map_.GetCell(idx);
            if (cellSize >= 24.0f && !map_.IsObstacle(idx)) {
                const std::string label = std::to_string(cell.height);
                const float labelSize = std::clamp((cellSize * 0.34f) / uiScale_, 13.0f, 26.0f);
                const float tw = MeasureTextUI(label, labelSize);
                DrawTextUIBold(label, rect.x + (cellSize - tw) * 0.5f, rect.y + cellSize * 0.32f, labelSize, MutedTextColor());
            }
        }
    }

    if (frame != nullptr && frame->path.size() >= 2) {
        auto centerOf = [&](int idx) {
            GridCoord gc = map_.Coord(idx);
            return Vector2{ startX + (static_cast<float>(gc.col) + 0.5f) * cellSize,
                            startY + (static_cast<float>(gc.row) + 0.5f) * cellSize };
        };
        auto catmull2 = [](Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, float t) {
            const float t2 = t * t;
            const float t3 = t2 * t;
            auto blend = [&](float a, float b, float c, float d) {
                return 0.5f * ((2.0f * b) + (-a + c) * t
                    + (2.0f * a - 5.0f * b + 4.0f * c - d) * t2
                    + (-a + 3.0f * b - 3.0f * c + d) * t3);
            };
            return Vector2{ blend(p0.x, p1.x, p2.x, p3.x), blend(p0.y, p1.y, p2.y, p3.y) };
        };
        Color pathColor = (themeMode_ == ThemeMode::Dark ? Color{ 255, 206, 135, 245 } : Color{ 244, 112, 83, 245 });
        if (!smoothPath_ || frame->path.size() < 4) {
            for (size_t i = 1; i < frame->path.size(); ++i) {
                DrawLineEx(centerOf(frame->path[i - 1]), centerOf(frame->path[i]), std::max(2.5f, cellSize * 0.18f), pathColor);
            }
        } else {
            for (size_t i = 0; i + 1 < frame->path.size(); ++i) {
                Vector2 p0 = centerOf(frame->path[(i == 0) ? i : i - 1]);
                Vector2 p1 = centerOf(frame->path[i]);
                Vector2 p2 = centerOf(frame->path[i + 1]);
                Vector2 p3 = centerOf(frame->path[(i + 2 < frame->path.size()) ? i + 2 : i + 1]);
                Vector2 prev = p1;
                for (int s = 1; s <= 8; ++s) {
                    Vector2 cur = catmull2(p0, p1, p2, p3, static_cast<float>(s) / 8.0f);
                    DrawLineEx(prev, cur, std::max(2.5f, cellSize * 0.18f), pathColor);
                    prev = cur;
                }
            }
        }
    }

    const int hovered = MouseToCell(GetMousePosition());
    if (hovered >= 0) {
        GridCoord hc = map_.Coord(hovered);
        Rectangle rect{ startX + static_cast<float>(hc.col) * cellSize, startY + static_cast<float>(hc.row) * cellSize, cellSize, cellSize };
        DrawRectangleLinesEx(rect, 2.5f, OverlayColor());
    }
    EndScissorMode();

    const int hoverCell = MouseToCell(GetMousePosition());
    if (hoverCell >= 0) {
        GridCoord hc = map_.Coord(hoverCell);
        const Cell& cell = map_.GetCell(hoverCell);
        std::string tip = "cell " + std::to_string(hc.row) + "," + std::to_string(hc.col)
            + " | " + TerrainName(cell.terrain) + " | h=" + std::to_string(cell.height)
            + " | zoom=" + FormatDouble(mapZoom2D_, 2) + "x";
        DrawTextUI(tip, canvas_.x + S(22.0f), canvas_.y + S(22.0f), 17, TextColor());
    } else {
        DrawTextUI("2D map editor | wheel zoom, middle drag pan", canvas_.x + S(22.0f), canvas_.y + S(22.0f), 18, TextColor());
    }
}


void App::DrawTerrain3D() {
    BeginScissorMode(static_cast<int>(canvas_.x), static_cast<int>(canvas_.y), static_cast<int>(canvas_.width), static_cast<int>(canvas_.height));
    DrawGradientBackground(canvas_);

    BeginMode3D(camera_);

    const float worldW = static_cast<float>(map_.Cols());
    const float worldH = static_cast<float>(map_.Rows());
    Color floorColor = themeMode_ == ThemeMode::Dark ? Color{ 38, 31, 54, 255 } : Color{ 250, 219, 204, 255 };
    DrawPlane({ 0.0f, -0.035f, 0.0f }, { worldW + 3.0f, worldH + 3.0f }, floorColor);

    const SearchFrame* frame = CurrentFrame();
    if (smoothTerrain3D_) {
        DrawSmoothTerrain3D(frame);
    } else {
        DrawBlockTerrain3D(frame);
    }

    if (map_.Count() <= 9000) {
        for (int idx = 0; idx < map_.Count(); ++idx) {
            DrawTerrainFeature3D(idx);
        }
    }

    if (showGraphOverlay_) {
        DrawGraphOverlay3D();
    }

    if (frame != nullptr && frame->path.size() >= 2) {
        DrawPath3D(frame->path);
    }

    DrawSphere(CellWorldPosition(map_.Start(), 0.65f), 0.32f, SuccessColor());
    DrawSphere(CellWorldPosition(map_.Goal(), 0.65f), 0.32f, DangerColor());

    if (frame != nullptr && frame->current >= 0) {
        DrawSphere(CellWorldPosition(frame->current, 0.92f), 0.26f, AccentColor());
    }

    if (selectedCell_ >= 0) {
        DrawCellSelection3D(selectedCell_, AccentColor());
    }
    if (hoveredCell3D_ >= 0 && hoveredCell3D_ != selectedCell_) {
        DrawCellSelection3D(hoveredCell3D_, OverlayColor());
    }

    if (!smoothTerrain3D_) {
        DrawGrid(std::max(map_.Rows(), map_.Cols()) + 4, 1.0f);
    }
    EndMode3D();
    EndScissorMode();

    DrawRectangleLinesEx(canvas_, 1.0f, GridLineColor());
    DrawTextUI(smoothTerrain3D_ ? "3D soft heightfield" : "3D solid terrain editor", canvas_.x + S(22.0f), canvas_.y + S(22.0f), 19, TextColor());
    DrawTextUI("Soft mode uses a calm continuous heightfield; solid mode is best for editing clarity.", canvas_.x + S(22.0f), canvas_.y + S(49.0f), 14, MutedTextColor());
    DrawTextUI("Agent: " + MovementProfileName() + " | " + std::string(showGraphOverlay_ ? "graph overlay ON" : "graph overlay OFF"), canvas_.x + S(22.0f), canvas_.y + S(75.0f), 15, MutedTextColor());

    if (hoveredCell3D_ >= 0) {
        DrawTextUI("Hover: " + CellInfo(hoveredCell3D_), canvas_.x + S(22.0f), canvas_.y + S(101.0f), 15, TextColor());
    }
    if (selectedCell_ >= 0) {
        DrawTextUI("Selected: " + CellInfo(selectedCell_), canvas_.x + S(22.0f), canvas_.y + S(126.0f), 15, AccentColor());
    }
}

void App::DrawBlockTerrain3D(const SearchFrame* frame) const {
    const float cellScale = 1.0f;
    for (int r = 0; r < map_.Rows(); ++r) {
        for (int c = 0; c < map_.Cols(); ++c) {
            const int idx = map_.Index(r, c);
            const Cell& cell = map_.GetCell(idx);
            const bool blocked = map_.IsObstacle(idx) || map_.IsWater(idx);

            const float h = CellSurfaceHeight(idx);
            Vector3 center = CellWorldPosition(idx, 0.0f);
            center.y = h * 0.5f;
            Vector3 size{ cellScale * 0.92f, h, cellScale * 0.92f };

            Color color = CellColor(idx, frame);
            if (cell.terrain == TerrainType::Water) color = Blend(color, themeMode_ == ThemeMode::Dark ? Color{ 44, 80, 145, 255 } : Color{ 116, 190, 222, 255 }, 0.25f);
            DrawCubeV(center, size, color);

            if (blocked || idx == map_.Start() || idx == map_.Goal()) {
                DrawCubeWiresV(center, size, themeMode_ == ThemeMode::Dark ? Color{ 255, 238, 214, 95 } : Color{ 96, 72, 91, 120 });
            }
        }
    }
}

void App::DrawSmoothTerrain3D(const SearchFrame* frame) const {
    auto rawHeight01 = [&](int idx) {
        if (idx < 0 || idx >= map_.Count()) return 0.0f;
        return static_cast<float>(map_.GetCell(idx).height) / 10.0f;
    };

    auto softHeightAt = [&](int row, int col) {
        float sum = 0.0f;
        int count = 0;
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                int rr = std::clamp(row + dr, 0, map_.Rows() - 1);
                int cc = std::clamp(col + dc, 0, map_.Cols() - 1);
                int idx = map_.Index(rr, cc);
                float h = rawHeight01(idx);
                if (map_.IsWater(idx)) h *= 0.15f;
                sum += h;
                ++count;
            }
        }
        // Soft mode intentionally exaggerates height. The old version was mathematically smooth but visually flat.
        return 0.08f + (sum / static_cast<float>(count)) * terrainHeightScale_ * 2.85f;
    };

    auto vertex = [&](int row, int col, float lift = 0.0f) {
        row = std::clamp(row, 0, map_.Rows() - 1);
        col = std::clamp(col, 0, map_.Cols() - 1);
        const float x = static_cast<float>(col) - static_cast<float>(map_.Cols() - 1) * 0.5f;
        const float z = static_cast<float>(row) - static_cast<float>(map_.Rows() - 1) * 0.5f;
        return Vector3{ x, softHeightAt(row, col) + lift, z };
    };

    auto heightColor = [&](float h, float shade) {
        h = std::clamp(h, 0.0f, 1.0f);
        Color low = themeMode_ == ThemeMode::Dark ? Color{ 47, 48, 72, 255 } : Color{ 244, 213, 188, 255 };
        Color mid = themeMode_ == ThemeMode::Dark ? Color{ 104, 83, 134, 255 } : Color{ 221, 181, 151, 255 };
        Color high = themeMode_ == ThemeMode::Dark ? Color{ 224, 188, 247, 255 } : Color{ 151, 131, 180, 255 };
        Color base = h < 0.55f ? Blend(low, mid, h / 0.55f) : Blend(mid, high, (h - 0.55f) / 0.45f);
        Color light = themeMode_ == ThemeMode::Dark ? Color{ 255, 238, 255, base.a } : Color{ 255, 247, 235, base.a };
        Color dark = themeMode_ == ThemeMode::Dark ? Color{ 18, 19, 35, base.a } : Color{ 128, 89, 108, base.a };
        if (shade >= 0.0f) return Blend(base, light, std::clamp(shade, 0.0f, 0.45f));
        return Blend(base, dark, std::clamp(-shade, 0.0f, 0.50f));
    };

    for (int r = 0; r < map_.Rows() - 1; ++r) {
        for (int c = 0; c < map_.Cols() - 1; ++c) {
            const int idx = map_.Index(r, c);
            const int idxRight = map_.Index(r, c + 1);
            const int idxDown = map_.Index(r + 1, c);
            const int idxDiag = map_.Index(r + 1, c + 1);

            Vector3 a = vertex(r, c);
            Vector3 b = vertex(r, c + 1);
            Vector3 d = vertex(r + 1, c);
            Vector3 e = vertex(r + 1, c + 1);

            float h = (rawHeight01(idx) + rawHeight01(idxRight) + rawHeight01(idxDown) + rawHeight01(idxDiag)) * 0.25f;
            float dx = (rawHeight01(idxRight) + rawHeight01(idxDiag) - rawHeight01(idx) - rawHeight01(idxDown)) * 0.5f;
            float dz = (rawHeight01(idxDown) + rawHeight01(idxDiag) - rawHeight01(idx) - rawHeight01(idxRight)) * 0.5f;
            float shade = std::clamp((-dx * 1.20f + dz * 0.85f), -0.50f, 0.45f);
            Color color = heightColor(h, shade);

            const bool anyWater = map_.IsWater(idx) || map_.IsWater(idxRight) || map_.IsWater(idxDown) || map_.IsWater(idxDiag);
            const bool anyRoad = map_.GetCell(idx).terrain == TerrainType::Road || map_.GetCell(idxRight).terrain == TerrainType::Road
                || map_.GetCell(idxDown).terrain == TerrainType::Road || map_.GetCell(idxDiag).terrain == TerrainType::Road;
            const bool anyForest = map_.GetCell(idx).terrain == TerrainType::Forest || map_.GetCell(idxRight).terrain == TerrainType::Forest
                || map_.GetCell(idxDown).terrain == TerrainType::Forest || map_.GetCell(idxDiag).terrain == TerrainType::Forest;

            if (anyWater) {
                color = themeMode_ == ThemeMode::Dark ? Color{ 88, 146, 190, 242 } : Color{ 124, 190, 216, 235 };
            } else if (anyRoad) {
                color = Blend(color, themeMode_ == ThemeMode::Dark ? Color{ 237, 166, 124, 255 } : Color{ 229, 164, 128, 255 }, 0.36f);
            } else if (anyForest) {
                color = Blend(color, themeMode_ == ThemeMode::Dark ? Color{ 77, 166, 132, 255 } : Color{ 114, 174, 143, 255 }, 0.22f);
            }

            // Keep each quad one calm color. This avoids the old distracting triangle mosaic.
            DrawTriangle3D(a, b, d, color);
            DrawTriangle3D(b, e, d, color);
        }
    }

    // Subtle contour/grid lines every few cells: enough to read the heightfield, not enough to become noisy.
    Color contour = themeMode_ == ThemeMode::Dark ? Color{ 255, 235, 255, 78 } : Color{ 88, 67, 91, 72 };
    const int step = std::max(3, std::max(map_.Rows(), map_.Cols()) / 16);
    for (int r = 0; r < map_.Rows(); r += step) {
        for (int c = 1; c < map_.Cols(); ++c) DrawLine3D(vertex(r, c - 1, 0.018f), vertex(r, c, 0.018f), contour);
    }
    for (int c = 0; c < map_.Cols(); c += step) {
        for (int r = 1; r < map_.Rows(); ++r) DrawLine3D(vertex(r - 1, c, 0.018f), vertex(r, c, 0.018f), contour);
    }

    // Obstacles remain readable, but they no longer dominate the whole surface.
    for (int idx = 0; idx < map_.Count(); ++idx) {
        const Cell& cell = map_.GetCell(idx);
        if (map_.IsWater(idx)) continue;
        if (map_.IsObstacle(idx)) {
            GridCoord g = map_.Coord(idx);
            Vector3 p = vertex(g.row, g.col, 0.10f);
            Color cap = themeMode_ == ThemeMode::Dark ? Color{ 65, 59, 83, 215 } : Color{ 136, 124, 136, 190 };
            DrawCubeV({ p.x, p.y, p.z }, { 0.52f, 0.16f, 0.52f }, cap);
        }
        if (cell.terrain == TerrainType::Road && !map_.IsObstacle(idx)) {
            GridCoord g = map_.Coord(idx);
            Vector3 p = vertex(g.row, g.col, 0.035f);
            Color road = themeMode_ == ThemeMode::Dark ? Color{ 242, 176, 132, 190 } : Color{ 221, 165, 132, 165 };
            DrawCubeV({ p.x, p.y, p.z }, { 0.68f, 0.035f, 0.68f }, road);
        }
    }

    // Algorithm state is drawn as a separate clear overlay so the base terrain stays readable.
    if (frame != nullptr) {
        Color closed = themeMode_ == ThemeMode::Dark ? Color{ 127, 112, 154, 145 } : Color{ 126, 104, 164, 130 };
        Color open = themeMode_ == ThemeMode::Dark ? Color{ 108, 210, 226, 165 } : Color{ 49, 155, 178, 145 };
        Color current = themeMode_ == ThemeMode::Dark ? Color{ 255, 143, 119, 225 } : Color{ 231, 91, 68, 215 };
        auto plate = [&](int cell, Color c) {
            GridCoord g = map_.Coord(cell);
            Vector3 p = vertex(g.row, g.col, 0.13f);
            DrawCubeV({ p.x, p.y, p.z }, { 0.62f, 0.055f, 0.62f }, c);
        };
        for (int cell : frame->closed) plate(cell, closed);
        for (int cell : frame->open) plate(cell, open);
        if (frame->current >= 0) plate(frame->current, current);
    }
}

void App::DrawPath3D(const std::vector<int>& path) const {
    if (path.size() < 2) return;

    auto pointFor = [&](int cell) {
        return CellWorldPosition(cell, 0.58f);
    };

    if (!smoothPath_ || path.size() < 4) {
        for (size_t i = 1; i < path.size(); ++i) {
            Vector3 a = pointFor(path[i - 1]);
            Vector3 b = pointFor(path[i]);
            DrawLine3D(a, b, (themeMode_ == ThemeMode::Dark ? Color{ 255, 206, 135, 255 } : Color{ 244, 162, 97, 255 }));
            DrawSphere(a, 0.105f, (themeMode_ == ThemeMode::Dark ? Color{ 255, 206, 135, 255 } : Color{ 244, 162, 97, 255 }));
        }
        DrawSphere(pointFor(path.back()), 0.105f, (themeMode_ == ThemeMode::Dark ? Color{ 255, 206, 135, 255 } : Color{ 244, 162, 97, 255 }));
        return;
    }

    for (size_t i = 0; i + 1 < path.size(); ++i) {
        Vector3 p0 = pointFor(path[(i == 0) ? i : i - 1]);
        Vector3 p1 = pointFor(path[i]);
        Vector3 p2 = pointFor(path[i + 1]);
        Vector3 p3 = pointFor(path[(i + 2 < path.size()) ? i + 2 : i + 1]);

        Vector3 prev = p1;
        constexpr int kSegments = 8;
        for (int s = 1; s <= kSegments; ++s) {
            float t = static_cast<float>(s) / static_cast<float>(kSegments);
            Vector3 cur = CatmullRom(p0, p1, p2, p3, t);
            DrawLine3D(prev, cur, (themeMode_ == ThemeMode::Dark ? Color{ 255, 206, 135, 255 } : Color{ 244, 162, 97, 255 }));
            prev = cur;
        }
    }
    for (size_t i = 0; i < path.size(); i += std::max<size_t>(1, path.size() / 25)) {
        DrawSphere(pointFor(path[i]), 0.085f, (themeMode_ == ThemeMode::Dark ? Color{ 255, 206, 135, 210 } : Color{ 244, 162, 97, 210 }));
    }
    DrawSphere(pointFor(path.back()), 0.11f, (themeMode_ == ThemeMode::Dark ? Color{ 255, 206, 135, 255 } : Color{ 244, 162, 97, 255 }));
}

void App::DrawGraphOverlay3D() const {
    const int stride = std::max(1, std::max(map_.Rows(), map_.Cols()) / 45);
    Color edgeColor = themeMode_ == ThemeMode::Dark ? Color{ 121, 218, 232, 84 } : Color{ 112, 110, 170, 76 };
    Color nodeColor = themeMode_ == ThemeMode::Dark ? Color{ 255, 245, 214, 118 } : Color{ 93, 64, 94, 108 };
    for (int r = 0; r < map_.Rows(); r += stride) {
        for (int c = 0; c < map_.Cols(); c += stride) {
            int idx = map_.Index(r, c);
            if (!map_.IsWalkable(idx, movementProfile_)) continue;
            Vector3 p = CellWorldPosition(idx, 0.10f);
            DrawSphere(p, 0.045f, nodeColor);
            if (c + stride < map_.Cols()) {
                int right = map_.Index(r, c + stride);
                if (map_.IsWalkable(right, movementProfile_)) DrawLine3D(p, CellWorldPosition(right, 0.10f), edgeColor);
            }
            if (r + stride < map_.Rows()) {
                int down = map_.Index(r + stride, c);
                if (map_.IsWalkable(down, movementProfile_)) DrawLine3D(p, CellWorldPosition(down, 0.10f), edgeColor);
            }
        }
    }
}

void App::DrawCellSelection3D(int cell, Color color) const {
    if (cell < 0 || cell >= map_.Count()) return;
    BoundingBox box = CellBoundingBox3D(cell);
    const float lift = 0.035f;
    box.min.x -= 0.035f;
    box.min.z -= 0.035f;
    box.max.x += 0.035f;
    box.max.z += 0.035f;
    box.min.y = box.max.y + lift;
    box.max.y = box.min.y + 0.045f;
    Vector3 center{
        (box.min.x + box.max.x) * 0.5f,
        (box.min.y + box.max.y) * 0.5f,
        (box.min.z + box.max.z) * 0.5f
    };
    Vector3 size{
        box.max.x - box.min.x,
        box.max.y - box.min.y,
        box.max.z - box.min.z
    };
    DrawCubeWiresV(center, size, color);
    DrawCubeV(center, { size.x, 0.012f, size.z }, Color{ color.r, color.g, color.b, 42 });
}

void App::DrawTerrainFeature3D(int cell) const {
    if (cell < 0 || cell >= map_.Count()) return;
    const Cell& c = map_.GetCell(cell);
    if (map_.IsObstacle(cell) && c.terrain != TerrainType::Water) return;

    GridCoord gc = map_.Coord(cell);
    const int featureHash = (gc.row * 73856093) ^ (gc.col * 19349663);
    if (c.terrain == TerrainType::Forest && (std::abs(featureHash) % (smoothTerrain3D_ ? 11 : 7)) != 0) return;
    if (c.terrain == TerrainType::Mountain && (std::abs(featureHash) % (smoothTerrain3D_ ? 13 : 8)) != 0) return;
    if (c.terrain == TerrainType::Road && (std::abs(featureHash) % (smoothTerrain3D_ ? 6 : 3)) != 0) return;

    Vector3 p = CellWorldPosition(cell, 0.02f);

    switch (c.terrain) {
        case TerrainType::Road: {
            // A road is visually flat, cheap to move through, and less affected by slope.
            Color slab = themeMode_ == ThemeMode::Dark ? Color{ 220, 143, 113, 210 } : Color{ 230, 190, 171, 220 };
            Color stripe = themeMode_ == ThemeMode::Dark ? Color{ 255, 228, 178, 220 } : Color{ 255, 243, 212, 220 };
            DrawCubeV({ p.x, p.y + 0.010f, p.z }, { 0.62f, 0.028f, 0.62f }, slab);
            DrawCubeV({ p.x, p.y + 0.026f, p.z }, { 0.065f, 0.014f, 0.46f }, stripe);
            break;
        }
        case TerrainType::Forest: {
            // Forest adds movement resistance; draw small stylized trees independently of terrain height.
            Color trunk = themeMode_ == ThemeMode::Dark ? Color{ 154, 103, 72, 255 } : Color{ 122, 83, 57, 255 };
            Color canopy = themeMode_ == ThemeMode::Dark ? Color{ 121, 216, 167, 245 } : Color{ 85, 159, 121, 245 };
            DrawCylinder({ p.x, p.y + 0.085f, p.z }, 0.045f, 0.045f, 0.18f, 7, trunk);
            DrawCylinder({ p.x, p.y + 0.30f, p.z }, 0.0f, 0.18f, 0.34f, 8, canopy);
            break;
        }
        case TerrainType::Mountain: {
            // Mountain is a rough surface. In dark mode it is intentionally light lavender for contrast.
            Color peak = themeMode_ == ThemeMode::Dark ? Color{ 236, 207, 255, 245 } : Color{ 170, 141, 184, 235 };
            Color stone = themeMode_ == ThemeMode::Dark ? Color{ 204, 164, 226, 230 } : Color{ 139, 119, 150, 230 };
            DrawCylinder({ p.x, p.y + 0.18f, p.z }, 0.0f, 0.23f, 0.38f, 6, peak);
            DrawSphere({ p.x + 0.08f, p.y + 0.12f, p.z - 0.08f }, 0.075f, stone);
            break;
        }
        case TerrainType::Water: {
            // Water is blocked in the current path-planning model; render as a soft translucent plate.
            Color water = themeMode_ == ThemeMode::Dark ? Color{ 100, 202, 232, 170 } : Color{ 121, 201, 228, 170 };
            DrawCubeV({ p.x, p.y + 0.028f, p.z }, { 0.82f, 0.045f, 0.82f }, water);
            break;
        }
        case TerrainType::Plain:
        default:
            break;
    }
}

void App::HandleMapEditing() {
    Vector2 mouse = GetMousePosition();

    if (view3D_) {
        hoveredCell3D_ = CheckCollisionPointRec(mouse, canvas_) ? PickCell3D(mouse) : -1;
        if (hoveredCell3D_ < 0) return;

        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            selectedCell_ = hoveredCell3D_;
            if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
                map_.SetStart(hoveredCell3D_);
                ResetVisualization();
                return;
            }
            if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
                map_.SetGoal(hoveredCell3D_);
                ResetVisualization();
                return;
            }
            ApplyEditToCell(hoveredCell3D_);
        }
        return;
    }

    hoveredCell3D_ = -1;
    if (!CheckCollisionPointRec(mouse, canvas_)) return;

    int cell = MouseToCell(mouse);
    if (cell < 0) return;

    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        selectedCell_ = cell;
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
            map_.SetStart(cell);
            ResetVisualization();
            return;
        }
        if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
            map_.SetGoal(cell);
            ResetVisualization();
            return;
        }
        ApplyEditToCell(cell);
    }

    if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
        selectedCell_ = cell;
        map_.SetObstacle(cell, false);
        if (map_.GetCell(cell).terrain == TerrainType::Water) map_.SetTerrain(cell, TerrainType::Plain);
        ResetVisualization();
    }
}

void App::ApplyEditToCell(int cell) {
    if (cell < 0 || cell >= map_.Count()) return;

    switch (editMode_) {
        case EditMode::Select:
            statusMessage_ = "Selected " + CellInfo(cell);
            return;
        case EditMode::Wall:
            map_.SetObstacle(cell, true);
            break;
        case EditMode::Erase:
            map_.SetObstacle(cell, false);
            if (map_.GetCell(cell).terrain == TerrainType::Water) map_.SetTerrain(cell, TerrainType::Plain);
            break;
        case EditMode::Start:
            map_.SetStart(cell);
            break;
        case EditMode::Goal:
            map_.SetGoal(cell);
            break;
        case EditMode::Raise:
            map_.AddHeight(cell, 1);
            break;
        case EditMode::Lower:
            map_.AddHeight(cell, -1);
            break;
        case EditMode::Plain:
            map_.SetObstacle(cell, false);
            map_.SetTerrain(cell, TerrainType::Plain);
            break;
        case EditMode::Road:
            map_.SetObstacle(cell, false);
            map_.SetTerrain(cell, TerrainType::Road);
            break;
        case EditMode::Forest:
            map_.SetObstacle(cell, false);
            map_.SetTerrain(cell, TerrainType::Forest);
            break;
        case EditMode::Mountain:
            map_.SetObstacle(cell, false);
            map_.SetTerrain(cell, TerrainType::Mountain);
            break;
        case EditMode::Water:
            map_.SetObstacle(cell, false);
            map_.SetTerrain(cell, TerrainType::Water);
            break;
    }
    ResetVisualization();
}

void App::GenerateRandomMap() {
    map_.GenerateRandom(obstacleProbability_);
    selectedCell_ = -1;
    hoveredCell3D_ = -1;
    ResetVisualization();
    statusMessage_ = "Random map generated";
}

void App::GenerateTerrainMap() {
    map_.GenerateTerrain();
    selectedCell_ = -1;
    hoveredCell3D_ = -1;
    ResetVisualization();
    statusMessage_ = "Continuous terrain map generated: smooth hills, road, river, forest and mountains";
}

void App::GenerateMazeMap() {
    map_.GenerateMaze();
    selectedCell_ = -1;
    hoveredCell3D_ = -1;
    ResetVisualization();
    statusMessage_ = "Maze map generated";
}

void App::ApplyMapSize() {
    int rows = static_cast<int>(std::round(mapRowsValue_));
    int cols = static_cast<int>(std::round(mapColsValue_));
    map_.Resize(rows, cols);
    selectedCell_ = -1;
    hoveredCell3D_ = -1;
    ResetVisualization();
    ResetCamera3D();
    mapPan2D_ = { 0.0f, 0.0f };
    mapZoom2D_ = 1.0f;
    statusMessage_ = "Map size changed to " + std::to_string(map_.Rows()) + " x " + std::to_string(map_.Cols());
}

void App::RunAlgorithm(AlgorithmKind kind) {
    SyncOptionsFromSliders();
    result_ = Pathfinder::Run(map_, kind, options_);
    hasResult_ = true;
    frameIndex_ = 0;
    playing_ = true;
    accumulator_ = 0.0f;
    statusMessage_ = "Running " + result_.algorithmName + " as " + MovementProfileName();
}

void App::RunBenchmark() {
    SyncOptionsFromSliders();
    benchmarkRows_.clear();
    const std::vector<AlgorithmKind> algorithms = {
        AlgorithmKind::BFS,
        AlgorithmKind::Dijkstra,
        AlgorithmKind::AStar,
        AlgorithmKind::WeightedAStar,
        AlgorithmKind::GreedyBestFirst
    };

    for (AlgorithmKind algorithm : algorithms) {
        SearchResult r = Pathfinder::Run(map_, algorithm, options_);
        benchmarkRows_.push_back({ r.algorithmName, r.found, r.pathLength, r.expandedNodes, r.generatedNodes, r.elapsedMs });
    }
    statusMessage_ = "Benchmark finished for " + std::to_string(benchmarkRows_.size()) + " algorithms as " + MovementProfileName();
}

void App::SaveCurrentMap() {
    std::string path = EnsurePathMapExtension(NativeMapFileDialog(true));
    if (path.empty()) {
        statusMessage_ = "Save canceled";
        return;
    }

    try {
        std::filesystem::path p(path);
        if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
    } catch (...) {
        statusMessage_ = "Failed to prepare target folder";
        return;
    }

    if (map_.SaveToFile(path)) {
        currentMapPath_ = path;
        statusMessage_ = "Map saved: " + CompactDisplayPath(path);
    } else {
        statusMessage_ = "Failed to save: " + CompactDisplayPath(path);
    }
}

void App::LoadCurrentMap() {
    std::string path = NativeMapFileDialog(false);
    if (path.empty()) {
        statusMessage_ = "Open canceled";
        return;
    }

    std::string error;
    if (map_.LoadFromFile(path, &error)) {
        currentMapPath_ = path;
        mapRowsValue_ = static_cast<float>(map_.Rows());
        mapColsValue_ = static_cast<float>(map_.Cols());
        selectedCell_ = -1;
        hoveredCell3D_ = -1;
        ResetVisualization();
        ResetCamera3D();
        statusMessage_ = "Map loaded: " + CompactDisplayPath(path);
    } else {
        statusMessage_ = error.empty() ? ("Failed to open: " + CompactDisplayPath(path)) : error;
    }
}

void App::ExportBenchmarkCsv() {
    try {
        std::filesystem::create_directories("exports");
        std::ofstream out("exports/benchmark.csv");
        if (!out) {
            statusMessage_ = "Failed to create exports/benchmark.csv";
            return;
        }
        out << "algorithm,found,path_cost,expanded_nodes,generated_nodes,time_ms\n";
        for (const auto& row : benchmarkRows_) {
            out << row.algorithmName << ',' << (row.found ? "true" : "false") << ','
                << row.pathLength << ',' << row.expandedNodes << ',' << row.generatedNodes << ',' << row.elapsedMs << '\n';
        }
        statusMessage_ = "Benchmark exported to exports/benchmark.csv";
    } catch (...) {
        statusMessage_ = "Failed to export benchmark CSV";
    }
}

void App::ResetVisualization() {
    hasResult_ = false;
    result_ = SearchResult{};
    frameIndex_ = 0;
    playing_ = false;
    accumulator_ = 0.0f;
}

void App::SyncOptionsFromSliders() {
    options_.heuristicWeight = static_cast<double>(heuristicWeightValue_);
    options_.terrainWeight = static_cast<double>(terrainWeightValue_);
    options_.heightWeight = static_cast<double>(heightWeightValue_);
    options_.profile = movementProfile_;
}

void App::LoadUIFont() {
    for (const auto& path : FontCandidates()) {
        if (!FileExists(path.c_str())) continue;
        Font loaded = LoadFontEx(path.c_str(), 192, nullptr, 0);
        if (loaded.texture.id != 0) {
            uiFont_ = loaded;
            SetTextureFilter(uiFont_.texture, TEXTURE_FILTER_BILINEAR);
            customFontLoaded_ = true;
            fontStatus_ = path;
            return;
        }
    }
    uiFont_ = GetFontDefault();
    SetTextureFilter(uiFont_.texture, TEXTURE_FILTER_BILINEAR);
    customFontLoaded_ = false;
    fontStatus_ = "default raylib font; add assets/fonts/ui.ttf to override";
}

void App::UnloadUIFont() {
    if (customFontLoaded_) {
        UnloadFont(uiFont_);
        customFontLoaded_ = false;
    }
}

bool App::Button(Rectangle bounds, const char* text) {
    Vector2 mouse = GetMousePosition();
    bool hover = CheckCollisionPointRec(mouse, bounds);
    bool pressed = hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    Color fill = hover ? (themeMode_ == ThemeMode::Dark ? Color{ 74, 64, 98, 255 } : Color{ 244, 210, 202, 255 }) : (themeMode_ == ThemeMode::Dark ? Color{ 52, 45, 70, 255 } : Color{ 246, 226, 219, 255 });
    if (pressed) fill = AccentColor();

    DrawRectangleRounded(bounds, 0.18f, 8, fill);
    DrawRectangleRoundedLines(bounds, 0.18f, 8, 1.0f, GridLineColor());

    float textWidth = MeasureTextUI(text, 17);
    DrawTextUIBold(text, bounds.x + (bounds.width - textWidth) / 2.0f, bounds.y + (bounds.height - S(21.0f)) * 0.5f, 17, TextColor());
    return pressed;
}

bool App::ToggleButton(Rectangle bounds, const char* text, bool active) {
    Vector2 mouse = GetMousePosition();
    bool hover = CheckCollisionPointRec(mouse, bounds);
    bool pressed = hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    Color fill = active ? AccentColor() : (hover ? (themeMode_ == ThemeMode::Dark ? Color{ 74, 64, 98, 255 } : Color{ 244, 210, 202, 255 }) : (themeMode_ == ThemeMode::Dark ? Color{ 52, 45, 70, 255 } : Color{ 246, 226, 219, 255 }));

    DrawRectangleRounded(bounds, 0.18f, 8, fill);
    DrawRectangleRoundedLines(bounds, 0.18f, 8, 1.0f, GridLineColor());

    float textWidth = MeasureTextUI(text, 17);
    DrawTextUIBold(text, bounds.x + (bounds.width - textWidth) / 2.0f, bounds.y + (bounds.height - S(21.0f)) * 0.5f, 17, active ? Color{255, 248, 235, 255} : TextColor());
    return pressed;
}

bool App::Slider(Rectangle bounds, const char* label, float* value, float minValue, float maxValue, bool integerDisplay) {
    const float labelW = std::min(S(132.0f), bounds.width * 0.34f);
    const float valueW = S(58.0f);
    const float gap = S(10.0f);
    const float barX = bounds.x + labelW + gap;
    const float barW = std::max(S(70.0f), bounds.width - labelW - valueW - gap * 2.0f);
    const float barY = bounds.y + bounds.height * 0.5f - S(4.0f);

    DrawTextUIBold(label, bounds.x, bounds.y + S(3.0f), 15, TextColor());

    Rectangle bar{ barX, barY, barW, S(8.0f) };
    DrawRectangleRounded(bar, 0.4f, 8, themeMode_ == ThemeMode::Dark ? Color{ 70, 58, 92, 255 } : Color{ 224, 198, 204, 255 });

    float ratio = (*value - minValue) / (maxValue - minValue);
    ratio = std::clamp(ratio, 0.0f, 1.0f);
    Rectangle progress = bar;
    progress.width *= ratio;
    DrawRectangleRounded(progress, 0.4f, 8, AccentColor());

    float knobX = bar.x + bar.width * ratio;
    DrawCircle(static_cast<int>(knobX), static_cast<int>(bar.y + bar.height / 2.0f), S(8.0f), themeMode_ == ThemeMode::Dark ? Color{ 255, 245, 214, 255 } : Color{ 93, 64, 94, 255 });

    char buffer[64];
    if (std::string(label) == "Obstacle") {
        std::snprintf(buffer, sizeof(buffer), "%.0f%%", *value * 100.0f);
    } else if (integerDisplay) {
        std::snprintf(buffer, sizeof(buffer), "%.0f", *value);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%.2f", *value);
    }
    DrawTextUIBold(buffer, bar.x + bar.width + gap, bounds.y + S(3.0f), 15, MutedTextColor());

    bool changed = false;
    if (CheckCollisionPointRec(GetMousePosition(), { bar.x - S(8.0f), bar.y - S(14.0f), bar.width + S(16.0f), bar.height + S(28.0f) }) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        float newRatio = (GetMouseX() - bar.x) / bar.width;
        newRatio = std::clamp(newRatio, 0.0f, 1.0f);
        *value = minValue + newRatio * (maxValue - minValue);
        changed = true;
    }
    return changed;
}

float App::S(float value) const {
    return value * uiScale_;
}

void App::DrawTextUI(const std::string& text, float x, float y, float fontSize, Color color) const {
    const float scaledSize = fontSize * uiScale_;
    const float spacing = 0.20f * uiScale_;
    DrawTextEx(uiFont_, text.c_str(), Vector2{ x, y }, scaledSize, spacing, color);
}

void App::DrawTextUIBold(const std::string& text, float x, float y, float fontSize, Color color) const {
    const float scaledSize = fontSize * uiScale_;
    const float spacing = 0.20f * uiScale_;
    const float offset = std::max(0.55f, 0.55f * uiScale_);
    DrawTextEx(uiFont_, text.c_str(), Vector2{ x, y }, scaledSize, spacing, color);
    DrawTextEx(uiFont_, text.c_str(), Vector2{ x + offset, y }, scaledSize, spacing, color);
    DrawTextEx(uiFont_, text.c_str(), Vector2{ x, y + offset * 0.35f }, scaledSize, spacing, color);
}

float App::MeasureTextUI(const std::string& text, float fontSize) const {
    const float scaledSize = fontSize * uiScale_;
    return MeasureTextEx(uiFont_, text.c_str(), scaledSize, 0.20f * uiScale_).x;
}

int App::FrameCount() const {
    return hasResult_ ? static_cast<int>(result_.frames.size()) : 0;
}

const SearchFrame* App::CurrentFrame() const {
    if (!hasResult_ || result_.frames.empty()) return nullptr;
    int idx = std::clamp(frameIndex_, 0, FrameCount() - 1);
    return &result_.frames[static_cast<size_t>(idx)];
}

int App::MouseToCell(Vector2 mouse) const {
    const float padding = S(28.0f);
    const float usableW = canvas_.width - 2.0f * padding;
    const float usableH = canvas_.height - 2.0f * padding;
    const float baseCellSize = std::floor(std::min(usableW / static_cast<float>(map_.Cols()), usableH / static_cast<float>(map_.Rows())));
    const float cellSize = std::max(3.0f, baseCellSize * mapZoom2D_);
    if (cellSize <= 0.0f) return -1;

    const float gridW = cellSize * static_cast<float>(map_.Cols());
    const float gridH = cellSize * static_cast<float>(map_.Rows());
    const float startX = canvas_.x + (canvas_.width - gridW) / 2.0f + mapPan2D_.x;
    const float startY = canvas_.y + (canvas_.height - gridH) / 2.0f + mapPan2D_.y;

    if (mouse.x < startX || mouse.y < startY || mouse.x >= startX + gridW || mouse.y >= startY + gridH) {
        return -1;
    }

    int col = static_cast<int>((mouse.x - startX) / cellSize);
    int row = static_cast<int>((mouse.y - startY) / cellSize);
    if (!map_.InBounds(row, col)) return -1;
    return map_.Index(row, col);
}

int App::PickCell3D(Vector2 mouse) const {
    if (!CheckCollisionPointRec(mouse, canvas_)) return -1;

    Ray ray = GetMouseRay(mouse, camera_);
    int bestCell = -1;
    float bestDistance = 1.0e30f;

    for (int cell = 0; cell < map_.Count(); ++cell) {
        BoundingBox box = CellBoundingBox3D(cell);
        RayCollision hit = GetRayCollisionBox(ray, box);
        if (hit.hit && hit.distance < bestDistance) {
            bestDistance = hit.distance;
            bestCell = cell;
        }
    }

    return bestCell;
}

BoundingBox App::CellBoundingBox3D(int cell) const {
    if (cell < 0 || cell >= map_.Count()) {
        return BoundingBox{ Vector3{ 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 0.0f } };
    }

    GridCoord g = map_.Coord(cell);
    const float x = static_cast<float>(g.col) - static_cast<float>(map_.Cols() - 1) * 0.5f;
    const float z = static_cast<float>(g.row) - static_cast<float>(map_.Rows() - 1) * 0.5f;
    const float half = 0.46f;
    const float h = CellSurfaceHeight(cell);

    return BoundingBox{
        Vector3{ x - half, 0.0f, z - half },
        Vector3{ x + half, std::max(0.10f, h), z + half }
    };
}

Color App::CellColor(int cell, const SearchFrame* frame) const {
    const Cell& mapCell = map_.GetCell(cell);
    const float hNorm = static_cast<float>(mapCell.height) / 10.0f;
    Color color = TerrainVisualColor(mapCell.terrain, hNorm, false);

    if (map_.IsObstacle(cell) && mapCell.terrain != TerrainType::Water) {
        color = themeMode_ == ThemeMode::Dark ? Color{ 73, 66, 91, 255 } : Color{ 170, 154, 164, 255 };
    }

    if (frame != nullptr) {
        Color closed = themeMode_ == ThemeMode::Dark ? Color{ 129, 115, 158, 230 } : Color{ 181, 170, 198, 230 };
        Color open = themeMode_ == ThemeMode::Dark ? Color{ 112, 215, 232, 240 } : Color{ 111, 196, 214, 240 };
        Color path = themeMode_ == ThemeMode::Dark ? Color{ 255, 206, 135, 255 } : Color{ 244, 162, 97, 255 };
        Color current = themeMode_ == ThemeMode::Dark ? Color{ 255, 143, 119, 255 } : Color{ 231, 111, 81, 255 };
        if (ContainsCell(frame->closed, cell)) color = Blend(color, closed, 0.68f);
        if (ContainsCell(frame->open, cell)) color = Blend(color, open, 0.70f);
        if (ContainsCell(frame->path, cell)) color = path;
        if (frame->current == cell) color = current;
    }

    if (cell == map_.Start()) return SuccessColor();
    if (cell == map_.Goal()) return DangerColor();
    return color;
}


float App::CellSurfaceHeight(int cell) const {
    if (cell < 0 || cell >= map_.Count()) return 0.0f;
    const Cell& c = map_.GetCell(cell);
    float h = 0.12f + static_cast<float>(c.height) * terrainHeightScale_;
    if (map_.IsObstacle(cell) || map_.IsWater(cell)) h = std::max(0.65f, h + 0.55f);
    if (c.terrain == TerrainType::Water) h = std::max(0.18f, h * 0.32f);
    return h;
}

Vector3 App::CellWorldPosition(int cell, float extraHeight) const {
    GridCoord g = map_.Coord(cell);
    const float x = static_cast<float>(g.col) - static_cast<float>(map_.Cols() - 1) * 0.5f;
    const float z = static_cast<float>(g.row) - static_cast<float>(map_.Rows() - 1) * 0.5f;
    return { x, CellSurfaceHeight(cell) + extraHeight, z };
}

Vector3 App::TerrainVertexWorld(int row, int col, float extraHeight) const {
    row = std::clamp(row, 0, map_.Rows() - 1);
    col = std::clamp(col, 0, map_.Cols() - 1);
    const float x = static_cast<float>(col) - static_cast<float>(map_.Cols() - 1) * 0.5f;
    const float z = static_cast<float>(row) - static_cast<float>(map_.Rows() - 1) * 0.5f;

    float sum = 0.0f;
    int count = 0;
    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            int rr = row + dr;
            int cc = col + dc;
            if (rr < 0 || rr >= map_.Rows() || cc < 0 || cc >= map_.Cols()) continue;
            sum += CellSurfaceHeight(map_.Index(rr, cc));
            ++count;
        }
    }
    const float averagedHeight = count > 0 ? sum / static_cast<float>(count) : CellSurfaceHeight(map_.Index(row, col));
    return { x, averagedHeight + extraHeight, z };
}

Vector3 App::CatmullRom(Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3, float t) const {
    const float t2 = t * t;
    const float t3 = t2 * t;
    auto blend = [&](float a, float b, float c, float d) {
        return 0.5f * ((2.0f * b)
            + (-a + c) * t
            + (2.0f * a - 5.0f * b + 4.0f * c - d) * t2
            + (-a + 3.0f * b - 3.0f * c + d) * t3);
    };
    return { blend(p0.x, p1.x, p2.x, p3.x), blend(p0.y, p1.y, p2.y, p3.y), blend(p0.z, p1.z, p2.z, p3.z) };
}

bool App::ContainsCell(const std::vector<int>& values, int cell) const {
    return std::find(values.begin(), values.end(), cell) != values.end();
}

std::string App::EditModeName() const {
    switch (editMode_) {
        case EditMode::Select: return "Select";
        case EditMode::Wall: return "Wall";
        case EditMode::Erase: return "Erase";
        case EditMode::Start: return "Start";
        case EditMode::Goal: return "Goal";
        case EditMode::Raise: return "Height+";
        case EditMode::Lower: return "Height-";
        case EditMode::Plain: return "Plain";
        case EditMode::Road: return "Road";
        case EditMode::Forest: return "Forest";
        case EditMode::Mountain: return "Mountain";
        case EditMode::Water: return "Water";
        default: return "Unknown";
    }
}

std::string App::MovementProfileName() const {
    switch (movementProfile_) {
        case MovementProfile::Pedestrian: return "Pedestrian";
        case MovementProfile::Car: return "Car";
        case MovementProfile::Offroad: return "Offroad";
        case MovementProfile::Drone: return "Drone";
        default: return "Unknown";
    }
}

std::string App::ThemeName() const {
    return themeMode_ == ThemeMode::Dark ? "Dark" : "Light";
}

std::string App::SelectedMapPath() const {
    int slot = std::clamp(selectedMapSlot_, 1, 3);
    return "maps/slot" + std::to_string(slot) + ".pathmap";
}

std::string App::CellInfo(int cell) const {
    if (cell < 0 || cell >= map_.Count()) return "Selected: none";

    GridCoord g = map_.Coord(cell);
    const Cell& c = map_.GetCell(cell);
    std::string type = map_.IsWalkable(cell, movementProfile_) ? "walkable for " + MovementProfileName() : "blocked for " + MovementProfileName();
    if (cell == map_.Start()) type = "start";
    if (cell == map_.Goal()) type = "goal";

    return "cell " + std::to_string(g.row) + "," + std::to_string(g.col)
        + " | " + TerrainName(c.terrain)
        + " | h=" + std::to_string(c.height)
        + " | " + type;
}

} // namespace pathlab
