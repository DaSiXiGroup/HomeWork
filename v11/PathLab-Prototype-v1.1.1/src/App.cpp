#include "App.h"

#include <algorithm>
#include <vector>
#include <array>
#include <cmath>
#include <random>

#include <cstdio>
#include <cstdlib>
#include <string>

#include <filesystem>
#include <fstream>


namespace pathlab {

    namespace {

        std::string FormatDouble(double value, int digits = 2) {
            char buffer[64];
            std::snprintf(buffer, sizeof(buffer), "%.*f", digits, value);
            return std::string(buffer);
        }

        void DrawQuad3D(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color color) {
            DrawTriangle3D(a, b, c, color);
            DrawTriangle3D(a, c, d, color);
            DrawTriangle3D(c, b, a, color);
            DrawTriangle3D(d, c, a, color);
        }

        void DrawShadedCubeV(Vector3 center, Vector3 size, Color base) {
            const float x0 = center.x - size.x * 0.5f;
            const float x1 = center.x + size.x * 0.5f;
            const float y0 = center.y - size.y * 0.5f;
            const float y1 = center.y + size.y * 0.5f;
            const float z0 = center.z - size.z * 0.5f;
            const float z1 = center.z + size.z * 0.5f;

            const Vector3 v000{ x0, y0, z0 };
            const Vector3 v001{ x0, y0, z1 };
            const Vector3 v010{ x0, y1, z0 };
            const Vector3 v011{ x0, y1, z1 };
            const Vector3 v100{ x1, y0, z0 };
            const Vector3 v101{ x1, y0, z1 };
            const Vector3 v110{ x1, y1, z0 };
            const Vector3 v111{ x1, y1, z1 };

            const Color top = ScaleColor(base, 1.08f);
            const Color east = ScaleColor(base, 0.92f);
            const Color west = ScaleColor(base, 0.80f);
            const Color south = ScaleColor(base, 0.98f);
            const Color north = ScaleColor(base, 0.86f);

            DrawQuad3D(v010, v110, v111, v011, top);
            DrawQuad3D(v100, v101, v111, v110, east);
            DrawQuad3D(v000, v010, v011, v001, west);
            DrawQuad3D(v001, v011, v111, v101, south);
            DrawQuad3D(v000, v100, v110, v010, north);
        }

        void DrawFlatShadedCubeV(Vector3 center, Vector3 size, Color base) {
            const Color top = ScaleColor(base, 1.06f);
            const Color side = ScaleColor(base, 0.94f);
            DrawShadedCubeV(center, size, side);

            const float x0 = center.x - size.x * 0.5f;
            const float x1 = center.x + size.x * 0.5f;
            const float y = center.y + size.y * 0.5f + 0.002f;
            const float z0 = center.z - size.z * 0.5f;
            const float z1 = center.z + size.z * 0.5f;
            DrawQuad3D({ x0, y, z0 }, { x1, y, z0 }, { x1, y, z1 }, { x0, y, z1 }, top);
        }

        const std::array<const char*, 5> TERRAIN_NAMES{ "Plain", "Road", "Forest", "Water", "Mountain" };
        const std::array<const char*, 4> PROFILE_NAMES{ "Pedestrian", "Car", "Offroad", "Drone" };

        const char* TerrainName(TerrainType terrain) {
            return TERRAIN_NAMES[static_cast<size_t>(terrain)];
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

        std::string NativePngFileDialog() {
#ifdef _WIN32
            std::string command = "powershell -NoProfile -Sta -ExecutionPolicy Bypass -Command \"Add-Type -AssemblyName System.Windows.Forms; [Console]::OutputEncoding=[System.Text.Encoding]::UTF8; $d=New-Object System.Windows.Forms.SaveFileDialog; $d.Title='Export PathLab screenshot'; $d.Filter='PNG Image (*.png)|*.png|All Files (*.*)|*.*'; $d.DefaultExt='png'; $d.FileName='pathlab_screenshot.png'; if($d.ShowDialog() -eq 'OK'){ Write-Output $d.FileName }\"";
            return CaptureCommand(command);
#elif defined(__APPLE__)
            std::string command = "osascript -e 'POSIX path of (choose file name with prompt \"Export PathLab screenshot\" default name \"pathlab_screenshot.png\")' 2>/dev/null";
            return CaptureCommand(command);
#else
            std::string command = "zenity --file-selection --save --confirm-overwrite --title='Export PathLab screenshot' --filename='pathlab_screenshot.png' 2>/dev/null";
            return CaptureCommand(command);
#endif
        }

        std::string EnsurePngExtension(std::string path) {
            if (path.empty()) return path;
            std::filesystem::path p(path);
            if (p.extension().empty()) p.replace_extension(".png");
            return p.string();
        }

        std::string CompactDisplayPath(const std::string& path) {
            if (path.empty()) return "No file selected";
            std::filesystem::path p(path);
            std::string name = p.filename().string();
            if (name.empty()) name = path;
            return name.size() > 34 ? (name.substr(0, 31) + "...") : name;
        }

        float Vec2Distance(Vector2 a, Vector2 b) {
            const float dx = a.x - b.x;
            const float dy = a.y - b.y;
            return std::sqrt(dx * dx + dy * dy);
        }

        Vector2 Vec2Add(Vector2 a, Vector2 b) { return Vector2{ a.x + b.x, a.y + b.y }; }
        Vector2 Vec2Sub(Vector2 a, Vector2 b) { return Vector2{ a.x - b.x, a.y - b.y }; }
        Vector2 Vec2Scale(Vector2 v, float s) { return Vector2{ v.x * s, v.y * s }; }

        float Vec2Length(Vector2 v) {
            return std::sqrt(v.x * v.x + v.y * v.y);
        }

        Vector2 Vec2Normalize(Vector2 v) {
            const float len = Vec2Length(v);
            if (len <= 0.0001f) return Vector2{ 0.0f, 0.0f };
            return Vector2{ v.x / len, v.y / len };
        }

        float Vec3Distance(Vector3 a, Vector3 b) {
            const float dx = a.x - b.x;
            const float dy = a.y - b.y;
            const float dz = a.z - b.z;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }

        Vector3 Vec3Add(Vector3 a, Vector3 b) { return Vector3{ a.x + b.x, a.y + b.y, a.z + b.z }; }
        Vector3 Vec3Sub(Vector3 a, Vector3 b) { return Vector3{ a.x - b.x, a.y - b.y, a.z - b.z }; }
        Vector3 Vec3Scale(Vector3 v, float s) { return Vector3{ v.x * s, v.y * s, v.z * s }; }

        float Vec3Length(Vector3 v) {
            return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        }

        Vector3 Vec3Normalize(Vector3 v) {
            const float len = Vec3Length(v);
            if (len <= 0.0001f) return Vector3{ 0.0f, 0.0f, 0.0f };
            return Vector3{ v.x / len, v.y / len, v.z / len };
        }

        Vector2 QuadraticBezier2(Vector2 a, Vector2 control, Vector2 b, float t) {
            const float u = 1.0f - t;
            return Vector2{
                u * u * a.x + 2.0f * u * t * control.x + t * t * b.x,
                u * u * a.y + 2.0f * u * t * control.y + t * t * b.y
            };
        }

        Vector3 QuadraticBezier3(Vector3 a, Vector3 control, Vector3 b, float t) {
            const float u = 1.0f - t;
            return Vector3{
                u * u * a.x + 2.0f * u * t * control.x + t * t * b.x,
                u * u * a.y + 2.0f * u * t * control.y + t * t * b.y,
                u * u * a.z + 2.0f * u * t * control.z + t * t * b.z
            };
        }

        std::vector<Vector2> CompressPolyline2D(const std::vector<Vector2>& points) {
            if (points.size() <= 2) return points;
            std::vector<Vector2> out;
            out.push_back(points.front());
            for (size_t i = 1; i + 1 < points.size(); ++i) {
                Vector2 a = Vec2Normalize(Vec2Sub(points[i], points[i - 1]));
                Vector2 b = Vec2Normalize(Vec2Sub(points[i + 1], points[i]));
                const float cross = std::fabs(a.x * b.y - a.y * b.x);
                const float dot = a.x * b.x + a.y * b.y;
                if (cross > 0.015f || dot < 0.985f) out.push_back(points[i]);
            }
            out.push_back(points.back());
            return out;
        }

        std::vector<Vector3> CompressPolyline3D(const std::vector<Vector3>& points) {
            if (points.size() <= 2) return points;
            std::vector<Vector3> out;
            out.push_back(points.front());
            for (size_t i = 1; i + 1 < points.size(); ++i) {
                Vector3 a = Vec3Normalize(Vec3Sub(points[i], points[i - 1]));
                Vector3 b = Vec3Normalize(Vec3Sub(points[i + 1], points[i]));
                const float crossLen = std::sqrt(
                    (a.y * b.z - a.z * b.y) * (a.y * b.z - a.z * b.y) +
                    (a.z * b.x - a.x * b.z) * (a.z * b.x - a.x * b.z) +
                    (a.x * b.y - a.y * b.x) * (a.x * b.y - a.y * b.x));
                const float dot = a.x * b.x + a.y * b.y + a.z * b.z;
                if (crossLen > 0.015f || dot < 0.985f) out.push_back(points[i]);
            }
            out.push_back(points.back());
            return out;
        }

        std::vector<Vector2> RoundedPolyline2D(const std::vector<Vector2>& points, float radius, int curveSteps) {
            std::vector<Vector2> p = CompressPolyline2D(points);
            if (p.size() <= 2) return p;
            std::vector<Vector2> out;
            out.reserve(p.size() * static_cast<size_t>(curveSteps + 2));
            out.push_back(p.front());
            for (size_t i = 1; i + 1 < p.size(); ++i) {
                Vector2 prev = p[i - 1];
                Vector2 corner = p[i];
                Vector2 next = p[i + 1];
                const float lenIn = Vec2Distance(prev, corner);
                const float lenOut = Vec2Distance(corner, next);
                if (lenIn <= 0.0001f || lenOut <= 0.0001f) continue;
                const float trim = std::min({ radius, lenIn * 0.42f, lenOut * 0.42f });
                Vector2 inDir = Vec2Normalize(Vec2Sub(prev, corner));
                Vector2 outDir = Vec2Normalize(Vec2Sub(next, corner));
                Vector2 a = Vec2Add(corner, Vec2Scale(inDir, trim));
                Vector2 b = Vec2Add(corner, Vec2Scale(outDir, trim));
                if (Vec2Distance(out.back(), a) > 0.35f) out.push_back(a);
                for (int s = 1; s <= curveSteps; ++s) {
                    out.push_back(QuadraticBezier2(a, corner, b, static_cast<float>(s) / static_cast<float>(curveSteps)));
                }
            }
            if (Vec2Distance(out.back(), p.back()) > 0.35f) out.push_back(p.back());
            return out;
        }

        std::vector<Vector3> RoundedPolyline3D(const std::vector<Vector3>& points, float radius, int curveSteps) {
            std::vector<Vector3> p = CompressPolyline3D(points);
            if (p.size() <= 2) return p;
            std::vector<Vector3> out;
            out.reserve(p.size() * static_cast<size_t>(curveSteps + 2));
            out.push_back(p.front());
            for (size_t i = 1; i + 1 < p.size(); ++i) {
                Vector3 prev    = p[i - 1];
                Vector3 corner  = p[i];
                Vector3 next    = p[i + 1];
                const float lenIn   = Vec3Distance(prev, corner);
                const float lenOut  = Vec3Distance(corner, next);
                if (lenIn <= 0.0001f || lenOut <= 0.0001f) continue;
                const float trim = std::min({ radius, lenIn * 0.42f, lenOut * 0.42f });
                Vector3 inDir   = Vec3Normalize(Vec3Sub(prev, corner));
                Vector3 outDir  = Vec3Normalize(Vec3Sub(next, corner));
                Vector3 a       = Vec3Add(corner, Vec3Scale(inDir, trim));
                Vector3 b       = Vec3Add(corner, Vec3Scale(outDir, trim));
                if (Vec3Distance(out.back(), a) > 0.02f) out.push_back(a);
                for (int s = 1; s <= curveSteps; ++s) {
                    out.push_back(QuadraticBezier3(a, corner, b, static_cast<float>(s) / static_cast<float>(curveSteps)));
                }
            }
            if (Vec3Distance(out.back(), p.back()) > 0.02f) out.push_back(p.back());
            return out;
        }

        void DrawPathLine2D(const std::vector<Vector2>& points, float width, Color outline, Color core) {
            if (points.size() < 2) return;
            for (size_t i = 1; i < points.size(); ++i) DrawLineEx(points[i - 1], points[i], width + 3.0f, outline);
            for (size_t i = 1; i < points.size(); ++i) DrawLineEx(points[i - 1], points[i], width, core);
        }

        void DrawPathTube3D(const std::vector<Vector3>& points, float radius, Color outline, Color core) {
            if (points.size() < 2) return;
            for (size_t i = 1; i < points.size(); ++i) {
                if (Vec3Distance(points[i - 1], points[i]) > 0.001f) {
                    DrawCylinderEx(points[i - 1], points[i], radius * 1.45f, radius * 1.45f, 10, outline);
                }
            }
            for (size_t i = 1; i < points.size(); ++i) {
                if (Vec3Distance(points[i - 1], points[i]) > 0.001f) {
                    DrawCylinderEx(points[i - 1], points[i], radius, radius, 12, core);
                }
            }
            const size_t step = std::max<size_t>(1, points.size() / 18);
            for (size_t i = 0; i < points.size(); i += step) DrawSphere(points[i], radius * 1.12f, core);
            DrawSphere(points.back(), radius * 1.25f, core);
        }

        Color ProfileColor(MovementProfile profile, ThemeMode theme) {
            const int id = static_cast<int>(profile);
            if (id < 0 || id >= static_cast<int>(ColorSet::ProfileDark.size())) {
                return PickColor(theme, ColorSet::ProfileFallbackDark, ColorSet::ProfileFallbackLight);
            }
            const size_t index = static_cast<size_t>(id);
            return PickColor(theme, ColorSet::ProfileDark[index], ColorSet::ProfileLight[index]);
        }

        const char* ProfileDisplayName(MovementProfile profile) {
            return PROFILE_NAMES[static_cast<size_t>(profile)];
        }

        const std::array<const char*, 17>& FontCandidates() {
            static const std::array<const char*, 17> fonts{
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
            return fonts;
        }
    }

    App::App() : __map(35, 55) {
        __GenerateRandomMap();
    }

    void App::Run() {
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_WINDOW_MAXIMIZED);
        InitWindow(__windowWidth, __windowHeight, "PathLab Prototype v1.1.1 - fixed solid 3D rendering order");
        SetWindowMinSize(1280, 760);
        MaximizeWindow();
        SetTargetFPS(60);
        __uiFont = GetFontDefault();
        __LoadUIFont();
        __ResetCamera3D();

        while (!WindowShouldClose()) {
            __Layout();
            __Update();
            __Draw();
        }

        __UnloadUIFont();
        CloseWindow();
    }

    void App::__Layout() {
        __UpdateUIScale();

        const float topH    = __S(64.0f);
        const float bottomH = __S(96.0f);
        const float leftW   = __S(360.0f);
        const float rightW  = __S(410.0f);

        __leftPanel     = { 0, topH, leftW, static_cast<float>(GetScreenHeight()) - topH - bottomH };
        __rightPanel    = { static_cast<float>(GetScreenWidth()) - rightW, topH, rightW, static_cast<float>(GetScreenHeight()) - topH - bottomH };
        __canvas        = { leftW, topH, static_cast<float>(GetScreenWidth()) - leftW - rightW, static_cast<float>(GetScreenHeight()) - topH - bottomH };
        __bottomPanel   = { 0, static_cast<float>(GetScreenHeight()) - bottomH, static_cast<float>(GetScreenWidth()), bottomH };
    }

    void App::__Update() {
        if (IsKeyPressed(KEY_F11)) {
            __ToggleFullscreenMode();
        }
        if (IsKeyPressed(KEY_F10) && !__fullscreen) {
            MaximizeWindow();
        }
        if (IsKeyPressed(KEY_F5)) {
            __ToggleViewMode();
        }
        if (IsKeyPressed(KEY_F6)) {
            __ToggleThemeMode();
        }
        if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_Z)) {
            __UndoEdit();
        }
        if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_Y)) {
            __RedoEdit();
        }
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) || IsMouseButtonReleased(MOUSE_RIGHT_BUTTON)) {
            __lastEditedCell = -1;
        }
        const Vector2 mouse = GetMousePosition();
        const float wheel = GetMouseWheelMove();
        if (std::abs(wheel) > 0.001f && CheckCollisionPointRec(mouse, __leftPanel)) {
            __leftPanelScroll = std::clamp(__leftPanelScroll + wheel * __S(54.0f), -__S(760.0f), 0.0f);
        }
        else if (std::abs(wheel) > 0.001f && CheckCollisionPointRec(mouse, __rightPanel)) {
            __rightPanelScroll = std::clamp(__rightPanelScroll + wheel * __S(54.0f), -__S(660.0f), 0.0f);
        }
        else if (__view3D) {
            __UpdateCamera3D();
        }
        else if (CheckCollisionPointRec(mouse, __canvas)) {
            if (std::abs(wheel) > 0.001f) {
                const float oldZoom     =   __mapZoom2D;
                __mapZoom2D             *=  std::pow(1.12f, wheel);
                __mapZoom2D             =   std::clamp(__mapZoom2D, 0.55f, 5.0f);
                const float zoomRatio   =   __mapZoom2D / oldZoom;
                Vector2 center{ __canvas.x + __canvas.width * 0.5f, __canvas.y + __canvas.height * 0.5f };
                __mapPan2D.x = mouse.x - center.x - (mouse.x - center.x - __mapPan2D.x) * zoomRatio;
                __mapPan2D.y = mouse.y - center.y - (mouse.y - center.y - __mapPan2D.y) * zoomRatio;
                __statusMessage = "2D zoom: " + FormatDouble(__mapZoom2D, 2) + "x";
            }
            if (IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
                Vector2 delta = GetMouseDelta();
                __mapPan2D.x += delta.x;
                __mapPan2D.y += delta.y;
            }
        }

        __HandleMapEditing();

        if (__playing && __hasResult && __FrameCount() > 0) {
            __accumulator += GetFrameTime();
            const float interval = 1.0f / std::max(1.0f, __animationSpeed);
            while (__accumulator >= interval) {
                __accumulator -= interval;
                __frameIndex++;
                if (__frameIndex >= __FrameCount() - 1) {
                    __frameIndex    = __FrameCount() - 1;
                    __playing       = false;
                    __statusMessage = "Animation finished";
                    break;
                }
            }
        }
    }

    void App::__Draw() {
        BeginDrawing();
        ClearBackground(__CanvasColor());

        __DrawTopBar();
        __DrawLeftPanel();
        __DrawRightPanel();
        __DrawBottomPanel();
        if (__view3D) {
            __DrawTerrain3D();
        }
        else {
            __DrawMapCanvas();
        }

        EndDrawing();
    }

    void App::__UpdateUIScale() {
        const float widthScale = static_cast<float>(GetScreenWidth()) / 1440.0f;
        const float heightScale = static_cast<float>(GetScreenHeight()) / 900.0f;
        __uiScale = std::clamp(std::min(widthScale, heightScale), 0.95f, 1.35f);
    }

    void App::__ToggleFullscreenMode() {
        if (!__fullscreen) {
            __windowedWidth     = GetScreenWidth();
            __windowedHeight    = GetScreenHeight();
            const int monitor   = GetCurrentMonitor();
            SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
            ToggleFullscreen();
            __fullscreen    = true;
            __statusMessage = "Fullscreen enabled; press F11 to return";
        }
        else {
            ToggleFullscreen();
            SetWindowSize(__windowedWidth, __windowedHeight);
            __fullscreen    = false;
            __statusMessage = "Windowed mode restored";
        }
    }

    void App::__ToggleViewMode() {
        __view3D = !__view3D;
        if (__view3D) {
            __ResetCamera3D();
            __statusMessage = "3D terrain view enabled. Left click selects/edits; right or middle drag rotates.";
        }
        else {
            __statusMessage = "2D map view enabled. Editing tools are available.";
        }
    }

    void App::__ToggleThemeMode() {
        __themeMode = (__themeMode == ThemeMode::Light) ? ThemeMode::Dark : ThemeMode::Light;
        __statusMessage = "Theme switched to " + __ThemeName() + ".";
    }

    Color App::__PanelColor() const {
        return __themeMode == ThemeMode::Dark ? ColorSet::PanelDark : ColorSet::PanelLight;
    }

    Color App::__DarkPanelColor() const {
        return __themeMode == ThemeMode::Dark ? ColorSet::PanelDeepDark : ColorSet::PanelDeepLight;
    }

    Color App::__AccentColor() const {
        return __themeMode == ThemeMode::Dark ? ColorSet::AccentDark : ColorSet::AccentLight;
    }

    Color App::__TextColor() const {
        return __themeMode == ThemeMode::Dark ? ColorSet::TextDark : ColorSet::TextLight;
    }

    Color App::__MutedTextColor() const {
        return __themeMode == ThemeMode::Dark ? ColorSet::TextMutedDark : ColorSet::TextMutedLight;
    }

    Color App::__GridLineColor() const {
        return __themeMode == ThemeMode::Dark ? ColorSet::GridLineDark : ColorSet::GridLineLight;
    }

    Color App::__DangerColor() const {
        return __themeMode == ThemeMode::Dark ? ColorSet::DangerDark : ColorSet::DangerLight;
    }

    Color App::__SuccessColor() const {
        return __themeMode == ThemeMode::Dark ? ColorSet::SuccessDark : ColorSet::SuccessLight;
    }

    Color App::__CanvasColor() const {
        return __themeMode == ThemeMode::Dark ? ColorSet::CanvasDark : ColorSet::CanvasLight;
    }

    Color App::__OverlayColor() const {
        return __themeMode == ThemeMode::Dark ? ColorSet::OverlayDark : ColorSet::OverlayLight;
    }

    Color App::__TerrainVisualColor(TerrainType terrain, float heightNorm, bool sideFace) const {
        heightNorm = std::clamp(heightNorm, 0.0f, 1.0f);

        int id = static_cast<int>(terrain);
        if (id < 0 || id >= static_cast<int>(ColorSet::TerrainDark.size())) id = 0;

        const auto& ramp = (__themeMode == ThemeMode::Dark ? ColorSet::TerrainDark : ColorSet::TerrainLight)[static_cast<size_t>(id)];
        Color color = heightNorm < 0.55f
            ? Blend(ramp.low, ramp.mid, heightNorm / 0.55f)
            : Blend(ramp.mid, ramp.high, (heightNorm - 0.55f) / 0.45f);

        if (sideFace) {
            const Color shade = PickColor(
                __themeMode,
                WithAlpha(ColorSet::TerrainSideShadeDark, color.a),
                WithAlpha(ColorSet::TerrainSideShadeLight, color.a));
            color = Blend(color, shade, __themeMode == ThemeMode::Dark ? 0.22f : 0.18f);
        }
        return color;
    }
    void App::__DrawGradientBackground(Rectangle bounds) const {
        Color top       = __themeMode == ThemeMode::Dark ? ColorSet::GradientTopDark    : ColorSet::GradientTopLight;
        Color mid       = __themeMode == ThemeMode::Dark ? ColorSet::GradientMidDark    : ColorSet::GradientMidLight;
        Color bottom    = __themeMode == ThemeMode::Dark ? ColorSet::GradientBottomDark : ColorSet::GradientBottomLight;

        const int strips = 96;
        for (int i = 0; i < strips; ++i) {
            float t0    = static_cast<float>(i) / static_cast<float>(strips);
            float t1    = static_cast<float>(i + 1) / static_cast<float>(strips);
            float midT  = (t0 + t1) * 0.5f;
            Color c     = midT < 0.52f ? Blend(top, mid, midT / 0.52f) : Blend(mid, bottom, (midT - 0.52f) / 0.48f);
            Rectangle strip{ bounds.x, bounds.y + bounds.height * t0, bounds.width, bounds.height * (t1 - t0) + 1.0f };
            DrawRectangleRec(strip, c);
        }
    }

    void App::__ResetCamera3D() {
        const float mapSize = static_cast<float>(std::max(__map.Rows(), __map.Cols()));
        __cameraTarget      = { 0.0f, __smoothTerrain3D ? 0.55f : 0.0f, 0.0f };
        __cameraDistance    = std::max(__smoothTerrain3D ? 24.0f : 28.0f, mapSize * (__smoothTerrain3D ? 0.82f : 0.95f));
        __cameraYaw         = 0.78f;
        __cameraPitch       = __smoothTerrain3D ? 0.70f : 0.58f;

        __camera.up         = { 0.0f, 1.0f, 0.0f };
        __camera.fovy       = __smoothTerrain3D ? 38.0f : 45.0f;
        __camera.projection = CAMERA_PERSPECTIVE;
        __UpdateCamera3D();
    }

    void App::__UpdateCamera3D() {
        if (IsKeyPressed(KEY_R) && CheckCollisionPointRec(GetMousePosition(), __canvas)) {
            __ResetCamera3D();
        }

        if (CheckCollisionPointRec(GetMousePosition(), __canvas)) {
            Vector2 delta = GetMouseDelta();
            if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON) || IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
                __cameraYaw     -=  delta.x * 0.008f;
                __cameraPitch   +=  delta.y * 0.008f;
                __cameraPitch   =   std::clamp(__cameraPitch, 0.18f, 1.34f);
            }

            float wheel = GetMouseWheelMove();
            if (std::abs(wheel) > 0.001f) {
                __cameraDistance *= (1.0f - wheel * 0.10f);
                __cameraDistance =  std::clamp(__cameraDistance, 12.0f, 180.0f);
            }
        }

        float move = GetFrameTime() * __cameraDistance * 0.55f;
        Vector3 forward{ static_cast<float>(std::sin(__cameraYaw)), 0.0f, static_cast<float>(std::cos(__cameraYaw)) };
        Vector3 right{ static_cast<float>(std::cos(__cameraYaw)), 0.0f, static_cast<float>(-std::sin(__cameraYaw)) };
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) move *= 2.0f;

        if (IsKeyDown(KEY_W)) {
            __cameraTarget.x += forward.x * move;
            __cameraTarget.z += forward.z * move;
        }
        if (IsKeyDown(KEY_S)) {
            __cameraTarget.x -= forward.x * move;
            __cameraTarget.z -= forward.z * move;
        }
        if (IsKeyDown(KEY_D)) {
            __cameraTarget.x += right.x * move;
            __cameraTarget.z += right.z * move;
        }
        if (IsKeyDown(KEY_A)) {
            __cameraTarget.x -= right.x * move;
            __cameraTarget.z -= right.z * move;
        }
        if (IsKeyDown(KEY_E)) __cameraTarget.y += move * 0.35f;
        if (IsKeyDown(KEY_Q)) __cameraTarget.y -= move * 0.35f;

        const float cp = static_cast<float>(std::cos(__cameraPitch));
        __camera.target = __cameraTarget;
        __camera.position = {
            __cameraTarget.x + __cameraDistance * static_cast<float>(std::sin(__cameraYaw)) * cp,
            __cameraTarget.y + __cameraDistance * static_cast<float>(std::sin(__cameraPitch)),
            __cameraTarget.z + __cameraDistance * static_cast<float>(std::cos(__cameraYaw)) * cp
        };
    }

    void App::__DrawTopBar() {
        DrawRectangle(0, 0, GetScreenWidth(), static_cast<int>(__S(64.0f)), __DarkPanelColor());
        __DrawTextUIBold("PathLab", __S(20), __S(15), 27, __TextColor());
        __DrawTextUI("v1.1.1", __S(128), __S(24), 15, __MutedTextColor());

        const float buttonW = __S(132.0f);
        const float buttonH = __S(36.0f);
        const float gap = __S(12.0f);
        float cx = static_cast<float>(GetScreenWidth()) * 0.5f - (buttonW * 3.0f + gap * 2.0f) * 0.5f;
        if (__Button({ cx, __S(14.0f), buttonW, buttonH }, __view3D ? "View: 3D" : "View: 2D")) {
            __ToggleViewMode();
        }
        if (__Button({ cx + buttonW + gap, __S(14.0f), buttonW, buttonH }, __themeMode == ThemeMode::Dark ? "Theme: Dark" : "Theme: Light")) {
            __ToggleThemeMode();
        }
        if (__Button({ cx + (buttonW + gap) * 2.0f, __S(14.0f), buttonW, buttonH }, __view3D ? "Reset 3D" : "Reset 2D")) {
            if (__view3D) {
                __ResetCamera3D();
                __statusMessage = "3D camera reset";
            }
            else {
                __mapZoom2D     = 1.0f;
                __mapPan2D      = { 0.0f, 0.0f };
                __statusMessage = "2D view reset";
            }
        }

        const char* shortcuts = __fullscreen ? "F11 exit | F5 view | F6 theme | wheel zoom" : "F11 fullscreen | F10 maximize | F5 view | F6 theme | wheel zoom";
        __DrawTextUI(shortcuts, static_cast<float>(GetScreenWidth()) - __S(610.0f), __S(23.0f), 14, __MutedTextColor());
    }

    void App::__DrawLeftPanel() {
        DrawRectangleRec(__leftPanel, __PanelColor());
        DrawRectangleLinesEx(__leftPanel, 1.0f, __GridLineColor());
        BeginScissorMode(static_cast<int>(__leftPanel.x), static_cast<int>(__leftPanel.y), static_cast<int>(__leftPanel.width), static_cast<int>(__leftPanel.height));

        float       x           = __leftPanel.x + __S(22.0f);
        float       y           = __leftPanel.y + __S(18.0f) + __leftPanelScroll;
        const float contentW    = __leftPanel.width - __S(40.0f);
        const float gap         = __S(10.0f);
        const float bw          = (contentW - gap) * 0.5f;

        auto section = [&](const char* title) {
            __DrawTextUIBold(title, x, y, 20, __TextColor());
            DrawRectangleRec({ x, y + __S(29.0f), contentW, 1.0f }, __GridLineColor());
            y += __S(42.0f);
            };

        section("MAP");
        if (__Button({ x, y, bw, __S(34) }, "Random")) __GenerateRandomMap();
        if (__Button({ x + bw + gap, y, bw, __S(34) }, "Terrain")) __GenerateTerrainMap();
        y += __S(40);
        if (__Button({ x, y, bw, __S(34) }, "Maze")) __GenerateMazeMap();
        if (__Button({ x + bw + gap, y, bw, __S(34) }, "Clear")) {
            __PushUndoState("Clear map");
            __map.ClearAll();
            __ResetVisualization();
            __statusMessage = "Map cleared";
        }
        y += __S(40);
        if (__Button({ x, y, bw, __S(34) }, "City preset")) __GenerateCityPreset();
        if (__Button({ x + bw + gap, y, bw, __S(34) }, "River preset")) __GenerateRiverPreset();
        y += __S(40);
        if (__Button({ x, y, bw, __S(34) }, "Mountain pass")) __GenerateMountainPreset();
        if (__Button({ x + bw + gap, y, bw, __S(34) }, "Demo A*")) __RunDemoMode();
        y += __S(40);
        __Slider({ x, y, contentW, __S(24) }, "Rows", &__mapRowsValue, 10.0f, 80.0f, true);
        y += __S(38);
        __Slider({ x, y, contentW, __S(24) }, "Columns", &__mapColsValue, 10.0f, 120.0f, true);
        y += __S(38);
        __Slider({ x, y, contentW, __S(24) }, "Obstacle", &__obstacleProbability, 0.0f, 0.65f);
        y += __S(40);
        if (__Button({ x, y, contentW, __S(34) }, "Apply map size")) __ApplyMapSize();
        y += __S(46);

        section("ALGORITHMS");
        if (__Button({ x, y, bw, __S(34) }, "BFS")) __RunAlgorithm(AlgorithmKind::BFS);
        if (__Button({ x + bw + gap, y, bw, __S(34) }, "Dijkstra")) __RunAlgorithm(AlgorithmKind::Dijkstra);
        y += __S(40);
        if (__Button({ x, y, bw, __S(34) }, "A*")) __RunAlgorithm(AlgorithmKind::AStar);
        if (__Button({ x + bw + gap, y, bw, __S(34) }, "Weighted A*")) __RunAlgorithm(AlgorithmKind::WeightedAStar);
        y += __S(40);
        if (__Button({ x, y, bw, __S(34) }, "Greedy")) __RunAlgorithm(AlgorithmKind::GreedyBestFirst);
        if (__Button({ x + bw + gap, y, bw, __S(34) }, "Benchmark")) __RunBenchmark();
        y += __S(40);
        if (__ToggleButton({ x, y, contentW, __S(34) }, __showAgentComparison ? "Agent paths: ON" : "Agent paths: OFF", __showAgentComparison)) {
            if (!__showAgentComparison) __CompareAgents();
            else { __showAgentComparison = false; __agentComparisonRows.clear(); __statusMessage = "Agent comparison hidden"; }
        }
        y += __S(46);

        section("AGENT & COST");
        if (__ToggleButton({ x, y, contentW, __S(34) }, __options.allowDiagonal ? "Diagonal movement: ON" : "Diagonal movement: OFF", __options.allowDiagonal)) {
            __options.allowDiagonal = !__options.allowDiagonal;
            __statusMessage = __options.allowDiagonal ? "Diagonal movement enabled" : "Diagonal movement disabled";
        }
        y += __S(40);
        if (__ToggleButton({ x, y, bw, __S(34) }, "Pedestrian", __movementProfile == MovementProfile::Pedestrian)) __movementProfile = MovementProfile::Pedestrian;
        if (__ToggleButton({ x + bw + gap, y, bw, __S(34) }, "Car", __movementProfile == MovementProfile::Car)) __movementProfile = MovementProfile::Car;
        y += __S(40);
        if (__ToggleButton({ x, y, bw, __S(34) }, "Offroad", __movementProfile == MovementProfile::Offroad)) __movementProfile = MovementProfile::Offroad;
        if (__ToggleButton({ x + bw + gap, y, bw, __S(34) }, "Drone", __movementProfile == MovementProfile::Drone)) __movementProfile = MovementProfile::Drone;
        y += __S(38);
        __Slider({ x, y, contentW, __S(24) }, "Heuristic", &__heuristicWeightValue, 1.0f, 5.0f);
        y += __S(36);
        __Slider({ x, y, contentW, __S(24) }, "Terrain cost", &__terrainWeightValue, 0.0f, 5.0f);
        y += __S(36);
        __Slider({ x, y, contentW, __S(24) }, "Height cost", &__heightWeightValue, 0.0f, 2.0f);
        y += __S(40);

        section("VIEW");
        __Slider({ x, y, contentW, __S(24) }, "2D zoom", &__mapZoom2D, 0.55f, 5.0f);
        y += __S(36);
        __Slider({ x, y, contentW, __S(24) }, "3D height", &__terrainHeightScale, 0.08f, 0.90f);
        y += __S(40);
        if (__ToggleButton({ x, y, bw, __S(34) }, __smoothTerrain3D ? "Style: diorama" : "Style: editor", __smoothTerrain3D)) {
            __smoothTerrain3D = !__smoothTerrain3D;
            if (__view3D) __ResetCamera3D();
            __statusMessage = __smoothTerrain3D ? "Diorama mode: presentation sandbox view" : "Editor mode: precise solid-grid editing";
        }
        if (__ToggleButton({ x + bw + gap, y, bw, __S(34) }, __smoothPath ? "Path: curve" : "Path: polyline", __smoothPath)) __smoothPath = !__smoothPath;
        y += __S(40);
        if (__ToggleButton({ x, y, contentW, __S(34) }, __showGraphOverlay ? "__Algorithm graph: ON" : "__Algorithm graph: OFF", __showGraphOverlay)) __showGraphOverlay = !__showGraphOverlay;
        EndScissorMode();
    }

    void App::__DrawRightPanel() {
        DrawRectangleRec(__rightPanel, __PanelColor());
        DrawRectangleLinesEx(__rightPanel, 1.0f, __GridLineColor());
        BeginScissorMode(static_cast<int>(__rightPanel.x), static_cast<int>(__rightPanel.y), static_cast<int>(__rightPanel.width), static_cast<int>(__rightPanel.height));

        float       x           = __rightPanel.x + __S(22.0f);
        float       y           = __rightPanel.y + __S(18.0f) + __rightPanelScroll;
        const float contentW    = __rightPanel.width - __S(40.0f);
        const float gap         = __S(8.0f);
        const float bw          = (contentW - gap * 2.0f) / 3.0f;
        const float halfW       = (contentW - gap) * 0.5f;
        const float gh          = __S(34.0f);

        auto section = [&](const char* title) {
            __DrawTextUIBold(title, x, y, 20, __TextColor());
            DrawRectangleRec({ x, y + __S(29.0f), contentW, 1.0f }, __GridLineColor());
            y += __S(42.0f);
            };

        section("TOOLS");
        auto tool = [&](EditMode mode, const char* label, int col, int row) {
            if (__ToggleButton({ x + col * (bw + gap), y + row * (gh + gap), bw, gh }, label, __editMode == mode)) {
                __editMode = mode;
            }
            };
        tool(EditMode::Select,      "Select",   0, 0);
        tool(EditMode::Wall,        "Wall",     1, 0);
        tool(EditMode::Erase,       "Erase",    2, 0);
        tool(EditMode::Start,       "Start",    0, 1);
        tool(EditMode::Goal,        "Goal",     1, 1);
        tool(EditMode::Road,        "Road",     2, 1);
        tool(EditMode::Forest,      "Forest",   0, 2);
        tool(EditMode::Mountain,    "Mountain", 1, 2);
        tool(EditMode::Water,       "Water",    2, 2);
        tool(EditMode::Plain,       "Plain",    0, 3);
        tool(EditMode::Raise,       "Height+",  1, 3);
        tool(EditMode::Lower,       "Height-",  2, 3);
        y += 4 * (gh + gap) + __S(12);
        if (__Button({ x, y, halfW, gh }, "Undo")) __UndoEdit();
        if (__Button({ x + halfW + gap, y, halfW, gh }, "Redo")) __RedoEdit();
        y += __S(42);

        __DrawTextUI("Mode: " + __EditModeName(), x, y, 14, __MutedTextColor());
        y += __S(24);
        __DrawTextUI(__CellInfo(__selectedCell), x, y, 14, __selectedCell >= 0 ? __TextColor() : __MutedTextColor());
        y += __S(38);

        section("PLAYBACK");
        if (__Button({ x, y, bw, gh }, __playing ? "Pause" : "Play")) {
            if (__hasResult) __playing = !__playing;
        }
        if (__Button({ x + bw + gap, y, bw, gh }, "Reset")) {
            __frameIndex    = 0;
            __playing       = false;
        }
        if (__Button({ x + (bw + gap) * 2.0f, y, bw, gh }, "End")) {
            if (__FrameCount() > 0) __frameIndex = __FrameCount() - 1;
            __playing = false;
        }
        y += __S(40);
        if (__Button({ x, y, bw, gh }, "Prev")) {
            __frameIndex = std::max(0, __frameIndex - 1);
            __playing = false;
        }
        if (__Button({ x + bw + gap, y, bw, gh }, "Next")) {
            __frameIndex = std::min(__FrameCount() - 1, __frameIndex + 1);
            __playing = false;
        }
        if (__Button({ x + (bw + gap) * 2.0f, y, bw, gh }, "Replay")) {
            if (__hasResult) {
                __frameIndex    = 0;
                __playing       = true;
            }
        }
        y += __S(40);
        __Slider({ x, y, contentW, __S(24) }, "Speed", &__animationSpeed, 1.0f, 160.0f, true);
        y += __S(46);

        section("OUTPUT");
        if (__Button({ x, y, halfW, gh }, "Save as...")) __SaveCurrentMap();
        if (__Button({ x + halfW + gap, y, halfW, gh }, "Open...")) __LoadCurrentMap();
        y += __S(40);
        if (__Button({ x, y, halfW, gh }, "Screenshot")) __ExportScreenshot();
        if (__Button({ x + halfW + gap, y, halfW, gh }, "CSV")) __ExportBenchmarkCsv();
        y += __S(38);
        __DrawTextUI("Map file: " + CompactDisplayPath(__currentMapPath), x, y, 14, __MutedTextColor());
        y += __S(34);

        section("LEGEND");
        __DrawLegend(x, y, contentW);
        y += __S(10);

        section("STATS");
        std::string algorithm = __hasResult ? __result.algorithmName : "None";
        __DrawTextUI("__Algorithm: " + algorithm, x, y, 14, __TextColor());
        y += __S(24);
        __DrawTextUI("Agent: " + __MovementProfileName(), x, y, 14, __TextColor());
        y += __S(24);
        __DrawTextUI("Frame: " + std::to_string(__FrameCount() == 0 ? 0 : __frameIndex + 1) + " / " + std::to_string(__FrameCount()), x, y, 14, __TextColor());
        y += __S(24);

        if (__hasResult) {
            __DrawTextUI("Found: " + std::string(__result.found ? "Yes" : "No") + " | Cost: " + FormatDouble(__result.pathLength, 2), x, y, 14, __result.found ? __SuccessColor() : __DangerColor());
            y += __S(24);
            __DrawTextUI("Expanded: " + std::to_string(__result.expandedNodes) + " | Generated: " + std::to_string(__result.generatedNodes), x, y, 14, __TextColor());
            y += __S(24);
            __DrawTextUI("Time: " + FormatDouble(__result.elapsedMs, 3) + " ms", x, y, 14, __TextColor());
            y += __S(23);
        }
        else {
            __DrawTextUI("Run an algorithm to see data.", x, y, 14, __MutedTextColor());
            y += __S(26);
        }

        const SearchFrame* frame = __CurrentFrame();
        if (frame != nullptr) {
            __DrawTextUI("Open: " + std::to_string(frame->open.size()) + " | Closed: " + std::to_string(frame->closed.size()) + " | Current: " + std::to_string(frame->current), x, y, 14, __TextColor());
            y += __S(28);
        }

        if (__showAgentComparison && !__agentComparisonRows.empty()) {
            __DrawTextUI("AGENT PATHS", x, y, 16, __TextColor());
            y += __S(24);
            for (const auto& row : __agentComparisonRows) {
                Color c = ProfileColor(row.profile, __themeMode);
                DrawRectangleRec({ x, y + __S(4), __S(11), __S(11) }, c);
                __DrawTextUI(row.profileName + (row.found ? (" | " + FormatDouble(row.pathLength, 1)) : " | no path"), x + __S(18), y, 13, row.found ? __TextColor() : __DangerColor());
                y += __S(20);
            }
            y += __S(8);
        }

        __DrawTextUI("BENCHMARK", x, y, 16, __TextColor());
        y += __S(24);
        if (__benchmarkRows.empty()) {
            __DrawTextUI("Click Benchmark to compare algorithms.", x, y, 13, __MutedTextColor());
        }
        else {
            __DrawTextUI("__Algorithm        Cost    Expanded   Time", x, y, 12, __MutedTextColor());
            y += __S(18);
            for (const auto& row : __benchmarkRows) {
                char buffer[256];
                std::snprintf(buffer, sizeof(buffer), "%-13s %7.2f %8d %7.3f", row.algorithmName.c_str(), row.pathLength, row.expandedNodes, row.elapsedMs);
                __DrawTextUI(buffer, x, y, 12, row.found ? __TextColor() : __DangerColor());
                y += __S(17);
            }
        }
        EndScissorMode();
    }

    void App::__DrawLegend(float x, float& y, float contentW) {
        struct LegendItem { TerrainType terrain; const char* name; const char* rule; };
        const LegendItem items[] = {
            { TerrainType::Plain,       "Plain",    "normal" },
            { TerrainType::Road,        "Road",     "fast" },
            { TerrainType::Forest,      "Forest",   "slow" },
            { TerrainType::Mountain,    "Mountain", "steep" },
            { TerrainType::Water,       "Water",    "blocked" }
        };
        const float sw = __S(13.0f);
        for (const auto& item : items) {
            Color color = __TerrainVisualColor(item.terrain, 0.45f, false);
            DrawRectangleRounded({ x, y + __S(3), sw, sw }, 0.18f, 6, color);
            DrawRectangleRoundedLines({ x, y + __S(3), sw, sw }, 0.18f, 6, 1.0f, __GridLineColor());
            __DrawTextUIBold(item.name, x + __S(20), y, 13, __TextColor());
            __DrawTextUI(item.rule, x + contentW - __S(96), y, 13, __MutedTextColor());
            y += __S(20);
        }
        y += __S(4);
        __DrawTextUI("Agent rules: " + __MovementProfileName(), x, y, 13, __MutedTextColor());
        y += __S(20);
        std::string rule;
        switch (__movementProfile) {
        case MovementProfile::Pedestrian: rule = "walks most terrain; water blocked"; break;
        case MovementProfile::Car: rule = "prefers roads; rough terrain costly"; break;
        case MovementProfile::Offroad: rule = "handles forest/mountain better"; break;
        case MovementProfile::Drone: rule = "nearly direct; terrain mostly ignored"; break;
        default: rule = "custom movement profile"; break;
        }
        __DrawTextUI(rule, x, y, 13, __MutedTextColor());
        y += __S(24);
    }

    void App::__DrawBottomPanel() {
        DrawRectangleRec(__bottomPanel, __DarkPanelColor());
        DrawRectangleLinesEx(__bottomPanel, 1.0f, __GridLineColor());

        float x             = __leftPanel.width + __S(24.0f);
        float y             = __bottomPanel.y   + __S(15.0f);
        float rightLimit    = __rightPanel.x    - __S(24.0f);
        __DrawTextUI("Timeline", __S(20), y, 20, __TextColor());

        Rectangle bar{ x, __bottomPanel.y + __S(22), std::max(__S(160.0f), rightLimit - x), __S(10) };
        DrawRectangleRounded(bar, 0.4f, 8, __themeMode == ThemeMode::Dark ? ColorSet::ControlTrackDark : ColorSet::ControlTrackLight);

        float t = 0.0f;
        if (__FrameCount() > 1) {
            t = static_cast<float>(__frameIndex) / static_cast<float>(__FrameCount() - 1);
        }
        Rectangle progress = bar;
        progress.width *= t;
        DrawRectangleRounded(progress, 0.4f, 8, __AccentColor());

        float knobX = bar.x + bar.width * t;
        DrawCircle(static_cast<int>(knobX), static_cast<int>(bar.y + bar.height / 2.0f), 8.0f, __themeMode == ThemeMode::Dark ? ColorSet::OverlayDark : ColorSet::OverlayLight);

        if (CheckCollisionPointRec(GetMousePosition(), { bar.x - 8, bar.y - 14, bar.width + 16, bar.height + 28 }) && IsMouseButtonDown(MOUSE_LEFT_BUTTON) && __FrameCount() > 1) {
            float ratio = (GetMouseX() - bar.x) / bar.width;
            ratio           = std::clamp(ratio, 0.0f, 1.0f);
            __frameIndex    = static_cast<int>(ratio * static_cast<float>(__FrameCount() - 1));
            __playing       = false;
        }

        std::string info = __hasResult
            ? (__result.algorithmName + " | step " + std::to_string(__frameIndex + 1) + " / " + std::to_string(__FrameCount()))
            : "Generate a map, choose a tool, then run an algorithm.";
        __DrawTextUI(info, x, __bottomPanel.y + __S(47.0f), 14, __MutedTextColor());
        __DrawTextUI("Status: " + __statusMessage, __S(20), __bottomPanel.y + __S(55.0f), 14, __MutedTextColor());

        const char* help = __view3D
            ? "3D: left select/edit | right/middle rotate | wheel zoom | WASD pan | R reset"
            : "2D: wheel zoom | middle drag pan | left paint | right erase | Shift start | Ctrl goal";
        float helpW = __MeasureTextUI(help, 13);
        __DrawTextUI(help, static_cast<float>(GetScreenWidth()) - helpW - __S(20.0f), __bottomPanel.y + __S(55.0f), 13, __MutedTextColor());
    }

    void App::__DrawMapCanvas() {
        DrawRectangleRec(__canvas, __CanvasColor());
        DrawRectangleLinesEx(__canvas, 1.0f, __GridLineColor());

        const float padding = __S(28.0f);
        const float usableW = __canvas.width - 2.0f * padding;
        const float usableH = __canvas.height - 2.0f * padding;
        const float baseCellSize = std::floor(std::min(usableW / static_cast<float>(__map.Cols()), usableH / static_cast<float>(__map.Rows())));
        const float cellSize = std::max(3.0f, baseCellSize * __mapZoom2D);
        if (cellSize <= 0.0f) return;

        const float gridW = cellSize * static_cast<float>(__map.Cols());
        const float gridH = cellSize * static_cast<float>(__map.Rows());
        const float startX = __canvas.x + (__canvas.width - gridW) / 2.0f + __mapPan2D.x;
        const float startY = __canvas.y + (__canvas.height - gridH) / 2.0f + __mapPan2D.y;

        const SearchFrame* frame = __CurrentFrame();

        BeginScissorMode(static_cast<int>(__canvas.x), static_cast<int>(__canvas.y), static_cast<int>(__canvas.width), static_cast<int>(__canvas.height));
        for (int r = 0; r < __map.Rows(); ++r) {
            for (int c = 0; c < __map.Cols(); ++c) {
                int idx = __map.Index(r, c);
                Rectangle rect{ startX + static_cast<float>(c) * cellSize, startY + static_cast<float>(r) * cellSize, cellSize, cellSize };
                if (rect.x + rect.width < __canvas.x || rect.y + rect.height < __canvas.y || rect.x > __canvas.x + __canvas.width || rect.y > __canvas.y + __canvas.height) continue;
                DrawRectangleRec(rect, __CellColor(idx, frame));
                if (cellSize >= 8.0f) DrawRectangleLinesEx(rect, 1.0f, __GridLineColor());

                const Cell& cell = __map.GetCell(idx);
                if (cellSize >= 24.0f && !__map.IsObstacle(idx)) {
                    const std::string label = std::to_string(cell.height);
                    const float labelSize = std::clamp((cellSize * 0.34f) / __uiScale, 13.0f, 26.0f);
                    const float tw = __MeasureTextUI(label, labelSize);
                    __DrawTextUIBold(label, rect.x + (cellSize - tw) * 0.5f, rect.y + cellSize * 0.32f, labelSize, __MutedTextColor());
                }
            }
        }

        if (frame != nullptr && frame->path.size() >= 2) {
            auto centerOf = [&](int idx) {
                GridCoord gc = __map.Coord(idx);
                return Vector2{ startX + (static_cast<float>(gc.col) + 0.5f) * cellSize,
                                startY + (static_cast<float>(gc.row) + 0.5f) * cellSize };
                };
            std::vector<Vector2> points;
            points.reserve(frame->path.size());
            for (int cell : frame->path) points.push_back(centerOf(cell));

            const float width = std::max(3.0f, cellSize * 0.16f);
            const Color core = __themeMode == ThemeMode::Dark ? ColorSet::Path2DCoreDark : ColorSet::Path2DCoreLight;
            const Color outline = __themeMode == ThemeMode::Dark ? ColorSet::Path2DOutlineDark : ColorSet::Path2DOutlineLight;
            if (__smoothPath && frame->path.size() >= 3) {
                points = RoundedPolyline2D(points, std::clamp(cellSize * 0.46f, 7.0f, 28.0f), 10);
            }
            DrawPathLine2D(points, width, outline, core);
        }

        __DrawAgentComparison2D(startX, startY, cellSize);

        const int hovered = __MouseToCell(GetMousePosition());
        if (hovered >= 0) {
            GridCoord hc = __map.Coord(hovered);
            Rectangle rect{ startX + static_cast<float>(hc.col) * cellSize, startY + static_cast<float>(hc.row) * cellSize, cellSize, cellSize };
            DrawRectangleLinesEx(rect, 2.5f, __OverlayColor());
        }
        EndScissorMode();

        const int hoverCell = __MouseToCell(GetMousePosition());
        if (hoverCell >= 0) {
            GridCoord hc = __map.Coord(hoverCell);
            const Cell& cell = __map.GetCell(hoverCell);
            std::string tip = "cell " + std::to_string(hc.row) + "," + std::to_string(hc.col)
                + " | " + TerrainName(cell.terrain) + " | h=" + std::to_string(cell.height)
                + " | zoom=" + FormatDouble(__mapZoom2D, 2) + "x";
            __DrawTextUI(tip, __canvas.x + __S(22.0f), __canvas.y + __S(22.0f), 17, __TextColor());
        }
        else {
            __DrawTextUI("2D map editor | wheel zoom, middle drag pan", __canvas.x + __S(22.0f), __canvas.y + __S(22.0f), 18, __TextColor());
        }
    }

    void App::__DrawAgentComparison2D(float startX, float startY, float cellSize) const {
        if (!__showAgentComparison || __agentComparisonRows.empty()) return;
        auto centerOf = [&](int idx) {
            GridCoord gc = __map.Coord(idx);
            return Vector2{ startX + (static_cast<float>(gc.col) + 0.5f) * cellSize,
                            startY + (static_cast<float>(gc.row) + 0.5f) * cellSize };
            };
        const float width = std::max(2.5f, cellSize * 0.10f);
        const Color outline = __themeMode == ThemeMode::Dark ? ColorSet::Agent2DOutlineDark : ColorSet::Agent2DOutlineLight;
        for (const auto& row : __agentComparisonRows) {
            if (!row.found || row.path.size() < 2) continue;
            std::vector<Vector2> points;
            points.reserve(row.path.size());
            for (int cell : row.path) points.push_back(centerOf(cell));
            if (__smoothPath && points.size() >= 3) points = RoundedPolyline2D(points, std::clamp(cellSize * 0.40f, 6.0f, 24.0f), 8);
            DrawPathLine2D(points, width, outline, ProfileColor(row.profile, __themeMode));
        }
    }

    void App::__DrawAgentComparison3D() const {
        if (!__showAgentComparison || __agentComparisonRows.empty()) return;
        for (const auto& row : __agentComparisonRows) {
            if (!row.found || row.path.size() < 2) continue;
            __DrawPath3DColored(row.path, ProfileColor(row.profile, __themeMode), 0.046f);
        }
    }

    void App::__DrawTerrain3D() {
        BeginScissorMode(static_cast<int>(__canvas.x), static_cast<int>(__canvas.y), static_cast<int>(__canvas.width), static_cast<int>(__canvas.height));
        __DrawGradientBackground(__canvas);

        BeginMode3D(__camera);

        const float worldW = static_cast<float>(__map.Cols());
        const float worldH = static_cast<float>(__map.Rows());
        Color floorColor = __themeMode == ThemeMode::Dark ? ColorSet::TerrainFloorDark : ColorSet::TerrainFloorLight;
        DrawPlane({ 0.0f, -0.035f, 0.0f }, { worldW + 3.0f, worldH + 3.0f }, floorColor);

        const SearchFrame* frame = __CurrentFrame();
        if (__smoothTerrain3D) {
            __DrawSmoothTerrain3D(frame);
        }
        else {
            __DrawBlockTerrain3D(frame);
        }

        if (!__smoothTerrain3D && __map.Count() <= 9000) {
            for (int idx = 0; idx < __map.Count(); ++idx) {
                __DrawTerrainFeature3D(idx);
            }
        }

        if (__showGraphOverlay) {
            __DrawGraphOverlay3D();
        }

        if (frame != nullptr && frame->path.size() >= 2) {
            __DrawPath3D(frame->path);
        }
        __DrawAgentComparison3D();

        DrawSphere(__CellWorldPosition(__map.Start(), 0.65f), 0.32f, __SuccessColor());
        DrawSphere(__CellWorldPosition(__map.Goal(), 0.65f), 0.32f, __DangerColor());

        if (frame != nullptr && frame->current >= 0) {
            DrawSphere(__CellWorldPosition(frame->current, 0.92f), 0.26f, __AccentColor());
        }

        if (__selectedCell >= 0) {
            __DrawCellSelection3D(__selectedCell, __AccentColor());
        }
        if (__hoveredCell3D >= 0 && __hoveredCell3D != __selectedCell) {
            __DrawCellSelection3D(__hoveredCell3D, __OverlayColor());
        }

        if (!__smoothTerrain3D) {
            DrawGrid(std::max(__map.Rows(), __map.Cols()) + 4, 1.0f);
        }
        EndMode3D();
        EndScissorMode();

        DrawRectangleLinesEx(__canvas, 1.0f, __GridLineColor());
        __DrawTextUI(__smoothTerrain3D ? "3D diorama presentation" : "3D editor mode", __canvas.x + __S(22.0f), __canvas.y + __S(22.0f), 19, __TextColor());
        __DrawTextUI(__smoothTerrain3D ? "Diorama hides the dense grid and turns the map into a clean terrain sandbox." : "Editor mode keeps every cell visible for precise editing and picking.", __canvas.x + __S(22.0f), __canvas.y + __S(49.0f), 14, __MutedTextColor());
        __DrawTextUI("Agent: " + __MovementProfileName() + " | " + std::string(__showGraphOverlay ? "graph overlay ON" : "graph overlay OFF"), __canvas.x + __S(22.0f), __canvas.y + __S(75.0f), 15, __MutedTextColor());

        if (__hoveredCell3D >= 0) {
            __DrawTextUI("Hover: " + __CellInfo(__hoveredCell3D), __canvas.x + __S(22.0f), __canvas.y + __S(101.0f), 15, __TextColor());
        }
        if (__selectedCell >= 0) {
            __DrawTextUI("Selected: " + __CellInfo(__selectedCell), __canvas.x + __S(22.0f), __canvas.y + __S(126.0f), 15, __AccentColor());
        }
    }

    void App::__DrawBlockTerrain3D(const SearchFrame* frame) const {
        const float cellScale = 1.0f;
        for (int r = 0; r < __map.Rows(); ++r) {
            for (int c = 0; c < __map.Cols(); ++c) {
                const int idx       = __map.Index(r, c);
                const Cell& cell    = __map.GetCell(idx);
                const bool blocked  = __map.IsObstacle(idx) || __map.IsWater(idx);

                const float h       = __CellSurfaceHeight(idx);
                Vector3 center      = __CellWorldPosition(idx, 0.0f);
                center.y            = h * 0.5f;
                Vector3 size{ cellScale * 0.92f, h, cellScale * 0.92f };

                Color color = __CellColor(idx, frame);
                if (cell.terrain == TerrainType::Water) color = Blend(color, __themeMode == ThemeMode::Dark ? ColorSet::BlockWaterTintDark : ColorSet::BlockWaterTintLight, 0.25f);
                DrawShadedCubeV(center, size, color);

                if (blocked || idx == __map.Start() || idx == __map.Goal()) {
                    DrawCubeWiresV(center, size, __themeMode == ThemeMode::Dark ? ColorSet::BlockWireDark : ColorSet::BlockWireLight);
                }
            }
        }
    }

    void App::__DrawSmoothTerrain3D(const SearchFrame* frame) const {

        const float worldW      = static_cast<float>(__map.Cols());
        const float worldH      = static_cast<float>(__map.Rows());
        const Color baseTop     = __themeMode == ThemeMode::Dark ? ColorSet::DioramaBaseTopDark : ColorSet::DioramaBaseTopLight;
        const Color baseSide    = __themeMode == ThemeMode::Dark ? ColorSet::DioramaBaseSideDark : ColorSet::DioramaBaseSideLight;
        const Color boardTrim   = __themeMode == ThemeMode::Dark ? ColorSet::DioramaBoardTrimDark : ColorSet::DioramaBoardTrimLight;

        DrawShadedCubeV({ 0.0f, -0.18f, 0.0f }, { worldW + 1.2f, 0.30f, worldH + 1.2f }, baseSide);
        DrawFlatShadedCubeV({ 0.0f, 0.015f, 0.0f }, { worldW + 0.95f, 0.035f, worldH + 0.95f }, baseTop);
        DrawCubeWiresV({ 0.0f, 0.015f, 0.0f }, { worldW + 0.95f, 0.040f, worldH + 0.95f }, boardTrim);

        auto pFor = [&](int idx, float lift = 0.06f) {
            return __CellWorldPosition(idx, lift);
            };

        auto isFrameCell = [&](int idx, const std::vector<int>& list) {
            return frame != nullptr && __ContainsCell(list, idx);
            };

        auto overlayFor = [&](int idx) -> Color {
            if (frame == nullptr)                   return ColorSet::Transparent;
            if (__ContainsCell(frame->path, idx))   return __themeMode == ThemeMode::Dark ? ColorSet::DioramaPathDark : ColorSet::DioramaPathLight;
            if (frame->current == idx)              return __themeMode == ThemeMode::Dark ? ColorSet::DioramaCurrentDark : ColorSet::DioramaCurrentLight;
            if (__ContainsCell(frame->open, idx))   return __themeMode == ThemeMode::Dark ? ColorSet::DioramaOpenDark : ColorSet::DioramaOpenLight;
            if (__ContainsCell(frame->closed, idx)) return __themeMode == ThemeMode::Dark ? ColorSet::DioramaClosedDark : ColorSet::DioramaClosedLight;
            return ColorSet::Transparent;
            };

        for (int idx = 0; idx < __map.Count(); ++idx) {
            const Cell& cell = __map.GetCell(idx);
            Vector3 p = pFor(idx, 0.052f);

            if (cell.terrain == TerrainType::Water) {
                Color water = __themeMode == ThemeMode::Dark ? ColorSet::DioramaWaterDark : ColorSet::DioramaWaterLight;
                DrawFlatShadedCubeV({ p.x, 0.075f, p.z }, { 0.96f, 0.030f, 0.96f }, water);
                continue;
            }

            if (cell.terrain == TerrainType::Road) {
                Color road = __themeMode == ThemeMode::Dark ? ColorSet::DioramaRoadDark : ColorSet::DioramaRoadLight;
                Color stripe = __themeMode == ThemeMode::Dark ? ColorSet::DioramaRoadStripeDark : ColorSet::DioramaRoadStripeLight;
                DrawFlatShadedCubeV({ p.x, 0.125f, p.z }, { 0.94f, 0.045f, 0.94f }, road);

                GridCoord g = __map.Coord(idx);
                bool horizontal = false;
                bool vertical = false;
                if (g.col > 0 && __map.GetCell(__map.Index(g.row, g.col - 1)).terrain == TerrainType::Road)                 horizontal = true;
                if (g.col + 1 < __map.Cols() && __map.GetCell(__map.Index(g.row, g.col + 1)).terrain == TerrainType::Road)  horizontal = true;
                if (g.row > 0 && __map.GetCell(__map.Index(g.row - 1, g.col)).terrain == TerrainType::Road)                 vertical = true;
                if (g.row + 1 < __map.Rows() && __map.GetCell(__map.Index(g.row + 1, g.col)).terrain == TerrainType::Road)  vertical = true;
                if      (horizontal && !vertical)   DrawFlatShadedCubeV({ p.x, 0.154f, p.z }, { 0.52f, 0.012f, 0.055f }, stripe);
                else if (vertical && !horizontal)   DrawFlatShadedCubeV({ p.x, 0.154f, p.z }, { 0.055f, 0.012f, 0.52f }, stripe);
                else                                DrawFlatShadedCubeV({ p.x, 0.154f, p.z }, { 0.20f, 0.012f, 0.20f }, stripe);
                continue;
            }

            if (cell.terrain == TerrainType::Forest) {
                GridCoord g = __map.Coord(idx);
                if (((idx * 31 + g.row * 17 + g.col * 13) % 11) == 0) {
                    Color trunk     = __themeMode == ThemeMode::Dark ? ColorSet::DioramaTrunkDark  : ColorSet::DioramaTrunkLight;
                    Color canopy    = __themeMode == ThemeMode::Dark ? ColorSet::DioramaCanopyDark : ColorSet::DioramaCanopyLight;
                    DrawCylinder({ p.x, 0.205f, p.z }, 0.045f, 0.045f, 0.23f, 7, trunk);
                    DrawCylinder({ p.x, 0.435f, p.z }, 0.02f, 0.21f, 0.38f, 8, canopy);
                }
                continue;
            }

            if (cell.terrain == TerrainType::Mountain) {
                GridCoord g = __map.Coord(idx);
                if (((idx * 23 + g.row * 11 + g.col * 19) % 13) == 0) {
                    Color rock  = __themeMode == ThemeMode::Dark ? ColorSet::DioramaRockDark : ColorSet::DioramaRockLight;
                    Color cap   = __themeMode == ThemeMode::Dark ? ColorSet::DioramaSnowDark : ColorSet::DioramaSnowLight;
                    float rockH = 0.42f + static_cast<float>(cell.height) * 0.035f;
                    DrawCylinder({ p.x, 0.18f + rockH * 0.50f, p.z }, 0.28f, 0.04f, rockH, 6, rock);
                    DrawSphere({ p.x + 0.08f, 0.18f + rockH * 0.80f, p.z - 0.07f }, 0.09f, cap);
                }
                continue;
            }
        }

        for (int idx = 0; idx < __map.Count(); ++idx) {
            if (!__map.IsObstacle(idx) || __map.IsWater(idx)) continue;
            const Cell& cell        = __map.GetCell(idx);
            GridCoord   g           = __map.Coord(idx);
            Vector3     p           = pFor(idx, 0.0f);
            float       h           = 0.58f + 0.055f * static_cast<float>((cell.height + g.row + g.col) % 5);
            Color       building    = __themeMode == ThemeMode::Dark ? ColorSet::DioramaBuildingDark : ColorSet::DioramaBuildingLight;
            Color       roof        = __themeMode == ThemeMode::Dark ? ColorSet::DioramaRoofDark     : ColorSet::DioramaRoofLight;
            DrawShadedCubeV({ p.x, h * 0.5f + 0.08f, p.z }, { 0.70f, h, 0.70f }, building);
            DrawFlatShadedCubeV({ p.x, h + 0.106f, p.z }, { 0.74f, 0.035f, 0.74f }, roof);
        }

        if (frame != nullptr) {
            const int stride = std::max(1, __map.Count() / 2400);
            for (int idx = 0; idx < __map.Count(); idx += stride) {
                Color ov = overlayFor(idx);
                if (ov.a == 0) continue;
                Vector3 p = pFor(idx, 0.16f);
                DrawFlatShadedCubeV({ p.x, p.y, p.z }, { 0.82f, 0.026f, 0.82f }, ov);
            }
        }
    }

    void App::__DrawPath3D(const std::vector<int>& path) const {
        const Color core = __themeMode == ThemeMode::Dark ? ColorSet::Path3DCoreDark : ColorSet::Path3DCoreLight;
        __DrawPath3DColored(path, core, 0.065f);
    }

    void App::__DrawPath3DColored(const std::vector<int>& path, Color core, float radius) const {
        if (path.size() < 2) return;

        std::vector<Vector3> points;
        points.reserve(path.size());
        for (int cell : path) points.push_back(__CellWorldPosition(cell, 0.66f));

        if (__smoothPath && path.size() >= 3) {
            points = RoundedPolyline3D(points, 0.46f, 10);
        }

        const Color outline = __themeMode == ThemeMode::Dark ? ColorSet::Path3DOutlineDark : ColorSet::Path3DOutlineLight;
        DrawPathTube3D(points, radius, outline, core);
    }

    void App::__DrawGraphOverlay3D() const {
        const int stride = std::max(1, std::max(__map.Rows(), __map.Cols()) / 45);
        Color edgeColor = __themeMode == ThemeMode::Dark ? ColorSet::GraphEdgeDark : ColorSet::GraphEdgeLight;
        Color nodeColor = __themeMode == ThemeMode::Dark ? ColorSet::GraphNodeDark : ColorSet::GraphNodeLight;
        for (int r = 0; r < __map.Rows(); r += stride) {
            for (int c = 0; c < __map.Cols(); c += stride) {
                int idx = __map.Index(r, c);
                if (!__map.IsWalkable(idx, __movementProfile)) continue;
                Vector3 p = __CellWorldPosition(idx, 0.10f);
                DrawSphere(p, 0.045f, nodeColor);
                if (c + stride < __map.Cols()) {
                    int right = __map.Index(r, c + stride);
                    if (__map.IsWalkable(right, __movementProfile)) DrawLine3D(p, __CellWorldPosition(right, 0.10f), edgeColor);
                }
                if (r + stride < __map.Rows()) {
                    int down = __map.Index(r + stride, c);
                    if (__map.IsWalkable(down, __movementProfile)) DrawLine3D(p, __CellWorldPosition(down, 0.10f), edgeColor);
                }
            }
        }
    }

    void App::__DrawCellSelection3D(int cell, Color color) const {
        if (cell < 0 || cell >= __map.Count()) return;
        BoundingBox box = __CellBoundingBox3D(cell);
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
        DrawFlatShadedCubeV(center, { size.x, 0.012f, size.z }, WithAlpha(color, ColorSet::SelectionFillAlpha));
    }

    void App::__DrawTerrainFeature3D(int cell) const {
        if (cell < 0 || cell >= __map.Count()) return;
        const Cell& c = __map.GetCell(cell);
        if (__map.IsObstacle(cell) && c.terrain != TerrainType::Water) return;

        GridCoord gc = __map.Coord(cell);
        const int featureHash = (gc.row * 73856093) ^ (gc.col * 19349663);
        if (c.terrain == TerrainType::Forest    && (std::abs(featureHash) % (__smoothTerrain3D ? 11 : 7)) != 0) return;
        if (c.terrain == TerrainType::Mountain  && (std::abs(featureHash) % (__smoothTerrain3D ? 13 : 8)) != 0) return;
        if (c.terrain == TerrainType::Road      && (std::abs(featureHash) % (__smoothTerrain3D ? 6 : 3))  != 0) return;

        Vector3 p = __CellWorldPosition(cell, 0.02f);

        switch (c.terrain) {
        case TerrainType::Road: {
            Color slab      = __themeMode == ThemeMode::Dark ? ColorSet::FeatureRoadSlabDark   : ColorSet::FeatureRoadSlabLight;
            Color stripe    = __themeMode == ThemeMode::Dark ? ColorSet::FeatureRoadStripeDark : ColorSet::FeatureRoadStripeLight;
            DrawFlatShadedCubeV({ p.x, p.y + 0.010f, p.z }, { 0.62f, 0.028f, 0.62f },  slab);
            DrawFlatShadedCubeV({ p.x, p.y + 0.026f, p.z }, { 0.065f, 0.014f, 0.46f }, stripe);
            break;
        }
        case TerrainType::Forest: {
            Color trunk     = __themeMode == ThemeMode::Dark ? ColorSet::FeatureTrunkDark  : ColorSet::FeatureTrunkLight;
            Color canopy    = __themeMode == ThemeMode::Dark ? ColorSet::FeatureCanopyDark : ColorSet::FeatureCanopyLight;
            DrawCylinder({ p.x, p.y + 0.085f, p.z }, 0.045f, 0.045f, 0.18f, 7, trunk);
            DrawCylinder({ p.x, p.y + 0.30f, p.z }, 0.0f, 0.18f, 0.34f, 8, canopy);
            break;
        }
        case TerrainType::Mountain: {
            Color peak  = __themeMode == ThemeMode::Dark ? ColorSet::FeaturePeakDark  : ColorSet::FeaturePeakLight;
            Color stone = __themeMode == ThemeMode::Dark ? ColorSet::FeatureStoneDark : ColorSet::FeatureStoneLight;
            DrawCylinder({ p.x, p.y + 0.18f, p.z }, 0.0f, 0.23f, 0.38f, 6, peak);
            DrawSphere({ p.x + 0.08f, p.y + 0.12f, p.z - 0.08f }, 0.075f, stone);
            break;
        }
        case TerrainType::Water: {
            Color water = __themeMode == ThemeMode::Dark ? ColorSet::FeatureWaterDark : ColorSet::FeatureWaterLight;
            DrawFlatShadedCubeV({ p.x, p.y + 0.028f, p.z }, { 0.82f, 0.045f, 0.82f }, water);
            break;
        }
        case TerrainType::Plain:
        default:
            break;
        }
    }

    void App::__HandleMapEditing() {
        Vector2 mouse = GetMousePosition();

        if (__view3D) {
            __hoveredCell3D = CheckCollisionPointRec(mouse, __canvas) ? __PickCell3D(mouse) : -1;
            if (__hoveredCell3D < 0) return;

            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                __selectedCell = __hoveredCell3D;
                if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
                    if (__lastEditedCell != __hoveredCell3D) __PushUndoState("Set start");
                    __lastEditedCell = __hoveredCell3D;
                    __map.SetStart(__hoveredCell3D);
                    __ResetVisualization();
                    return;
                }
                if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
                    if (__lastEditedCell != __hoveredCell3D) __PushUndoState("Set goal");
                    __lastEditedCell = __hoveredCell3D;
                    __map.SetGoal(__hoveredCell3D);
                    __ResetVisualization();
                    return;
                }
                if (__lastEditedCell != __hoveredCell3D) {
                    __PushUndoState("Edit cell");
                    __lastEditedCell = __hoveredCell3D;
                    __ApplyEditToCell(__hoveredCell3D);
                }
            }
            return;
        }

        __hoveredCell3D = -1;
        if (!CheckCollisionPointRec(mouse, __canvas)) return;

        int cell = __MouseToCell(mouse);
        if (cell < 0) return;

        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            __selectedCell = cell;
            if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
                if (__lastEditedCell != cell) __PushUndoState("Set start");
                __lastEditedCell = cell;
                __map.SetStart(cell);
                __ResetVisualization();
                return;
            }
            if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
                if (__lastEditedCell != cell) __PushUndoState("Set goal");
                __lastEditedCell = cell;
                __map.SetGoal(cell);
                __ResetVisualization();
                return;
            }
            if (__lastEditedCell != cell) {
                __PushUndoState("Edit cell");
                __lastEditedCell = cell;
                __ApplyEditToCell(cell);
            }
        }

        if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
            __selectedCell = cell;
            if (__lastEditedCell != cell) {
                __PushUndoState("Erase cell");
                __lastEditedCell = cell;
                __map.SetObstacle(cell, false);
                if (__map.GetCell(cell).terrain == TerrainType::Water) __map.SetTerrain(cell, TerrainType::Plain);
                __ResetVisualization();
            }
        }
    }

    void App::__ApplyEditToCell(int cell) {
        if (cell < 0 || cell >= __map.Count()) return;

        switch (__editMode) {
        case EditMode::Select:
            __statusMessage = "Selected " + __CellInfo(cell);
            return;
        case EditMode::Wall:
            __map.SetObstacle(cell, true);
            break;
        case EditMode::Erase:
            __map.SetObstacle(cell, false);
            if (__map.GetCell(cell).terrain == TerrainType::Water) __map.SetTerrain(cell, TerrainType::Plain);
            break;
        case EditMode::Start:
            __map.SetStart(cell);
            break;
        case EditMode::Goal:
            __map.SetGoal(cell);
            break;
        case EditMode::Raise:
            __map.AddHeight(cell, 1);
            break;
        case EditMode::Lower:
            __map.AddHeight(cell, -1);
            break;
        case EditMode::Plain:
            __map.SetObstacle(cell, false);
            __map.SetTerrain(cell, TerrainType::Plain);
            break;
        case EditMode::Road:
            __map.SetObstacle(cell, false);
            __map.SetTerrain(cell, TerrainType::Road);
            break;
        case EditMode::Forest:
            __map.SetObstacle(cell, false);
            __map.SetTerrain(cell, TerrainType::Forest);
            break;
        case EditMode::Mountain:
            __map.SetObstacle(cell, false);
            __map.SetTerrain(cell, TerrainType::Mountain);
            break;
        case EditMode::Water:
            __map.SetObstacle(cell, false);
            __map.SetTerrain(cell, TerrainType::Water);
            break;
        }
        __ResetVisualization();
    }

    void App::__GenerateRandomMap() {
        __map.GenerateRandom(__obstacleProbability);
        __selectedCell  = -1;
        __hoveredCell3D = -1;
        __ClearHistory();
        __showAgentComparison = false;
        __agentComparisonRows.clear();
        __ResetVisualization();
        __statusMessage = "Random map generated";
    }

    void App::__GenerateTerrainMap() {
        __map.GenerateTerrain();
        __selectedCell  = -1;
        __hoveredCell3D = -1;
        __ClearHistory();
        __showAgentComparison = false;
        __agentComparisonRows.clear();
        __ResetVisualization();
        __statusMessage = "Terrain map generated: hills, road, river, forest and mountains";
    }

    void App::__GenerateMazeMap() {
        __map.GenerateMaze();
        __selectedCell  = -1;
        __hoveredCell3D = -1;
        __ClearHistory();
        __showAgentComparison = false;
        __agentComparisonRows.clear();
        __ResetVisualization();
        __statusMessage = "Maze map generated";
    }

    void App::__GenerateCityPreset() {
        __map.ClearAll();
        std::mt19937 rng(20260706u);
        std::uniform_int_distribution<int> heightDist(0, 4);

        for (int r = 0; r < __map.Rows(); ++r) {
            for (int c = 0; c < __map.Cols(); ++c) {
                int idx = __map.Index(r, c);
                __map.SetHeight(idx, heightDist(rng));
                if (r % 6 == 2 || c % 9 == 4) {
                    __map.SetTerrain(idx, TerrainType::Road);
                    __map.SetObstacle(idx, false);
                    __map.SetHeight(idx, 1);
                }
                else if (((r / 3 + c / 4) % 5 == 0) && (r % 6 != 0) && (c % 9 != 0)) {
                    __map.SetObstacle(idx, true);
                    __map.SetTerrain(idx, TerrainType::Plain);
                    __map.SetHeight(idx, 2 + ((r + c) % 5));
                }
                else if ((r + c * 2) % 19 == 0) {
                    __map.SetTerrain(idx, TerrainType::Forest);
                }
            }
        }
        __map.SetStart(__map.Index(std::min(2, __map.Rows() - 1), std::min(2, __map.Cols() - 1)));
        __map.SetGoal(__map.Index(std::max(0, __map.Rows() - 3), std::max(0, __map.Cols() - 3)));
        __selectedCell = -1;
        __hoveredCell3D = -1;
        __ClearHistory();
        __showAgentComparison = false;
        __agentComparisonRows.clear();
        __ResetVisualization();
        __ResetCamera3D();
        __statusMessage = "City preset generated: roads, buildings and blocks";
    }

    void App::__GenerateRiverPreset() {
        __map.ClearAll();
        const int rows = __map.Rows();
        const int cols = __map.Cols();
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                int idx = __map.Index(r, c);
                __map.SetHeight(idx, std::clamp(4 + static_cast<int>(2.2 * std::sin(c * 0.23) + 1.4 * std::cos(r * 0.31)), 0, 9));
                if ((r + c) % 17 == 0) __map.SetTerrain(idx, TerrainType::Forest);
            }
        }
        for (int c = 0; c < cols; ++c) {
            int center = rows / 2 + static_cast<int>(std::sin(c * 0.22f) * rows * 0.13f);
            for (int dr = -2; dr <= 2; ++dr) {
                int r = center + dr;
                if (!__map.InBounds(r, c)) continue;
                int idx = __map.Index(r, c);
                __map.SetObstacle(idx, false);
                __map.SetTerrain(idx, TerrainType::Water);
                __map.SetHeight(idx, 0);
            }
        }
        const int bridges[] = { cols / 5, cols / 2, cols * 4 / 5 };
        for (int bridgeCol : bridges) {
            for (int c = std::max(0, bridgeCol - 1); c <= std::min(cols - 1, bridgeCol + 1); ++c) {
                for (int r = 0; r < rows; ++r) {
                    int idx = __map.Index(r, c);
                    __map.SetTerrain(idx, TerrainType::Road);
                    __map.SetObstacle(idx, false);
                    __map.SetHeight(idx, 1);
                }
            }
        }
        __map.SetStart(__map.Index(rows / 2, 1));
        __map.SetGoal(__map.Index(rows / 2, cols - 2));
        __selectedCell = -1;
        __hoveredCell3D = -1;
        __ClearHistory();
        __showAgentComparison = false;
        __agentComparisonRows.clear();
        __ResetVisualization();
        __ResetCamera3D();
        __statusMessage = "River crossing preset generated: bridges create route choices";
    }

    void App::__GenerateMountainPreset() {
        __map.ClearAll();
        const int rows = __map.Rows();
        const int cols = __map.Cols();
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                int idx = __map.Index(r, c);
                float ridge = std::abs(static_cast<float>(r) - rows * 0.5f - std::sin(c * 0.20f) * rows * 0.12f);
                int h = std::clamp(8 - static_cast<int>(ridge * 0.7f), 0, 9);
                __map.SetHeight(idx, h);
                if (h >= 5) __map.SetTerrain(idx, TerrainType::Mountain);
                if (h >= 8 && (c < cols / 3 || c > cols * 2 / 3)) __map.SetObstacle(idx, true);
                if ((r + c) % 21 == 0) __map.SetTerrain(idx, TerrainType::Forest);
            }
        }
        for (int passCol : { cols / 3, cols * 2 / 3 }) {
            for (int c = std::max(0, passCol - 2); c <= std::min(cols - 1, passCol + 2); ++c) {
                for (int r = rows / 2 - 4; r <= rows / 2 + 4; ++r) {
                    if (!__map.InBounds(r, c)) continue;
                    int idx = __map.Index(r, c);
                    __map.SetObstacle(idx, false);
                    __map.SetTerrain(idx, TerrainType::Road);
                    __map.SetHeight(idx, 2);
                }
            }
        }
        __map.SetStart(__map.Index(rows - 3, 2));
        __map.SetGoal(__map.Index(2, cols - 3));
        __selectedCell  = -1;
        __hoveredCell3D = -1;
        __ClearHistory();
        __showAgentComparison = false;
        __agentComparisonRows.clear();
        __ResetVisualization();
        __ResetCamera3D();
        __statusMessage = "Mountain pass preset generated: height cost matters";
    }

    void App::__RunDemoMode() {
        __GenerateRiverPreset();
        __view3D = true;
        __smoothTerrain3D = true;
        __movementProfile = MovementProfile::Pedestrian;
        __options.allowDiagonal = true;
        __heuristicWeightValue  = 1.25f;
        __terrainWeightValue    = 1.0f;
        __heightWeightValue     = 0.35f;
        __ResetCamera3D();
        __RunAlgorithm(AlgorithmKind::AStar);
        __frameIndex    = 0;
        __playing       = true;
        __statusMessage = "Demo mode: River preset + Diorama + A* animation";
    }

    void App::__ApplyMapSize() {
        int rows = static_cast<int>(std::round(__mapRowsValue));
        int cols = static_cast<int>(std::round(__mapColsValue));
        __PushUndoState("Resize map");
        __map.Resize(rows, cols);
        __selectedCell = -1;
        __hoveredCell3D = -1;
        __ResetVisualization();
        __ResetCamera3D();
        __mapPan2D = { 0.0f, 0.0f };
        __mapZoom2D = 1.0f;
        __statusMessage = "Map size changed to " + std::to_string(__map.Rows()) + " x " + std::to_string(__map.Cols());
    }

    void App::__RunAlgorithm(AlgorithmKind kind) {
        __SyncOptionsFromSliders();
        __result = Pathfinder::Run(__map, kind, __options);
        __hasResult = true;
        __frameIndex = 0;
        __playing = true;
        __accumulator = 0.0f;
        __statusMessage = "Running " + __result.algorithmName + " as " + __MovementProfileName();
    }

    void App::__RunBenchmark() {
        __SyncOptionsFromSliders();
        __benchmarkRows.clear();
        const std::array<AlgorithmKind, 5> algorithms{
            AlgorithmKind::BFS,
            AlgorithmKind::Dijkstra,
            AlgorithmKind::AStar,
            AlgorithmKind::WeightedAStar,
            AlgorithmKind::GreedyBestFirst
        };

        for (AlgorithmKind algorithm : algorithms) {
            SearchResult r = Pathfinder::Run(__map, algorithm, __options);
            __benchmarkRows.push_back({ r.algorithmName, r.found, r.pathLength, r.expandedNodes, r.generatedNodes, r.elapsedMs });
        }
        __statusMessage = "Benchmark finished for " + std::to_string(__benchmarkRows.size()) + " algorithms as " + __MovementProfileName();
    }

    void App::__CompareAgents() {
        __SyncOptionsFromSliders();
        __agentComparisonRows.clear();
        const std::array<MovementProfile, 4> profiles{
            MovementProfile::Pedestrian,
            MovementProfile::Car,
            MovementProfile::Offroad,
            MovementProfile::Drone
        };
        for (MovementProfile profile : profiles) {
            AlgorithmOptions opts = __options;
            opts.profile = profile;
            SearchResult r = Pathfinder::Run(__map, AlgorithmKind::AStar, opts);
            __agentComparisonRows.push_back({ profile, ProfileDisplayName(profile), r.found, r.pathLength, r.expandedNodes, r.elapsedMs, r.finalPath });
        }
        __showAgentComparison = true;
        __statusMessage = "Agent path comparison finished: same map, different cost models";
    }

    void App::__ExportScreenshot() {
        std::string path = EnsurePngExtension(NativePngFileDialog());
        if (path.empty()) {
            __statusMessage = "Screenshot export canceled";
            return;
        }
        try {
            std::filesystem::path p(path);
            if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
            TakeScreenshot(path.c_str());
            __statusMessage = "Screenshot exported: " + CompactDisplayPath(path);
        }
        catch (...) {
            __statusMessage = "Failed to export screenshot";
        }
    }

    void App::__SaveCurrentMap() {
        std::string path = EnsurePathMapExtension(NativeMapFileDialog(true));
        if (path.empty()) {
            __statusMessage = "Save canceled";
            return;
        }

        try {
            std::filesystem::path p(path);
            if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
        }
        catch (...) {
            __statusMessage = "Failed to prepare target folder";
            return;
        }

        if (__map.SaveToFile(path)) {
            __currentMapPath = path;
            __statusMessage = "Map saved: " + CompactDisplayPath(path);
        }
        else {
            __statusMessage = "Failed to save: " + CompactDisplayPath(path);
        }
    }

    void App::__LoadCurrentMap() {
        std::string path = NativeMapFileDialog(false);
        if (path.empty()) {
            __statusMessage = "Open canceled";
            return;
        }

        std::string error;
        if (__map.LoadFromFile(path, &error)) {
            __currentMapPath    = path;
            __mapRowsValue      = static_cast<float>(__map.Rows());
            __mapColsValue      = static_cast<float>(__map.Cols());
            __selectedCell      = -1;
            __hoveredCell3D     = -1;
            __ResetVisualization();
            __ResetCamera3D();
            __statusMessage     = "Map loaded: " + CompactDisplayPath(path);
        }
        else {
            __statusMessage = error.empty() ? ("Failed to open: " + CompactDisplayPath(path)) : error;
        }
    }

    void App::__ExportBenchmarkCsv() {
        try {
            std::filesystem::create_directories("exports");
            std::ofstream out("exports/benchmark.csv");
            if (!out) {
                __statusMessage = "Failed to create exports/benchmark.csv";
                return;
            }
            out << "algorithm,found,path_cost,expanded_nodes,generated_nodes,time_ms\n";
            for (const auto& row : __benchmarkRows) {
                out << row.algorithmName << ',' << (row.found ? "true" : "false") << ','
                    << row.pathLength << ',' << row.expandedNodes << ',' << row.generatedNodes << ',' << row.elapsedMs << '\n';
            }
            __statusMessage = "Benchmark exported to exports/benchmark.csv";
        }
        catch (...) {
            __statusMessage = "Failed to export benchmark CSV";
        }
    }

    void App::__PushUndoState(const std::string& label) {
        __undoStack.push_back(MapHistoryState{ __map, label });
        if (__undoStack.size() > 80) __undoStack.erase(__undoStack.begin());
        __redoStack.clear();
    }

    void App::__ClearHistory() {
        __undoStack.clear();
        __redoStack.clear();
        __lastEditedCell = -1;
    }

    void App::__UndoEdit() {
        if (__undoStack.empty()) {
            __statusMessage = "Nothing to undo";
            return;
        }
        __redoStack.push_back(MapHistoryState{ __map, "redo" });
        MapHistoryState state = __undoStack.back();
        __undoStack.pop_back();
        __map = state.map;
        __mapRowsValue  = static_cast<float>(__map.Rows());
        __mapColsValue  = static_cast<float>(__map.Cols());
        __selectedCell  = -1;
        __hoveredCell3D = -1;
        __showAgentComparison = false;
        __agentComparisonRows.clear();
        __ResetVisualization();
        __statusMessage = "Undo: " + state.label;
    }

    void App::__RedoEdit() {
        if (__redoStack.empty()) {
            __statusMessage = "Nothing to redo";
            return;
        }
        __undoStack.push_back(MapHistoryState{ __map, "undo" });
        MapHistoryState state = __redoStack.back();
        __redoStack.pop_back();
        __map = state.map;
        __mapRowsValue  = static_cast<float>(__map.Rows());
        __mapColsValue  = static_cast<float>(__map.Cols());
        __selectedCell  = -1;
        __hoveredCell3D = -1;
        __showAgentComparison = false;
        __agentComparisonRows.clear();
        __ResetVisualization();
        __statusMessage = "Redo";
    }

    void App::__ResetVisualization() {
        __hasResult             = false;
        __result                = SearchResult{};
        __frameIndex            = 0;
        __playing               = false;
        __accumulator           = 0.0f;
        __showAgentComparison   = false;
        __agentComparisonRows.clear();
    }

    void App::__SyncOptionsFromSliders() {
        __options.heuristicWeight   = static_cast<double>(__heuristicWeightValue);
        __options.terrainWeight     = static_cast<double>(__terrainWeightValue);
        __options.heightWeight      = static_cast<double>(__heightWeightValue);
        __options.profile           = __movementProfile;
    }

    void App::__LoadUIFont() {
        for (const char* path : FontCandidates()) {
            if (!FileExists(path)) continue;
            Font loaded = LoadFontEx(path, 192, nullptr, 0);
            if (loaded.texture.id != 0) {
                __uiFont = loaded;
                SetTextureFilter(__uiFont.texture, TEXTURE_FILTER_BILINEAR);
                __customFontLoaded = true;
                __fontStatus = path;
                return;
            }
        }
        __uiFont = GetFontDefault();
        SetTextureFilter(__uiFont.texture, TEXTURE_FILTER_BILINEAR);
        __customFontLoaded = false;
        __fontStatus = "default raylib font; add assets/fonts/ui.ttf to override";
    }

    void App::__UnloadUIFont() {
        if (__customFontLoaded) {
            UnloadFont(__uiFont);
            __customFontLoaded = false;
        }
    }

    bool App::__Button(Rectangle bounds, const char* text) {
        Vector2 mouse = GetMousePosition();
        bool hover = CheckCollisionPointRec(mouse, bounds);
        bool pressed = hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        Color fill = hover ? (__themeMode == ThemeMode::Dark ? ColorSet::ButtonHoverDark : ColorSet::ButtonHoverLight) : (__themeMode == ThemeMode::Dark ? ColorSet::ButtonIdleDark : ColorSet::ButtonIdleLight);
        if (pressed) fill = __AccentColor();

        DrawRectangleRounded(bounds, 0.18f, 8, fill);
        DrawRectangleRoundedLines(bounds, 0.18f, 8, 1.0f, __GridLineColor());

        float textWidth = __MeasureTextUI(text, 17);
        __DrawTextUIBold(text, bounds.x + (bounds.width - textWidth) / 2.0f, bounds.y + (bounds.height - __S(21.0f)) * 0.5f, 17, __TextColor());
        return pressed;
    }

    bool App::__ToggleButton(Rectangle bounds, const char* text, bool active) {
        Vector2 mouse = GetMousePosition();
        bool hover = CheckCollisionPointRec(mouse, bounds);
        bool pressed = hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        Color fill = active ? __AccentColor() : (hover ? (__themeMode == ThemeMode::Dark ? ColorSet::ButtonHoverDark : ColorSet::ButtonHoverLight) : (__themeMode == ThemeMode::Dark ? ColorSet::ButtonIdleDark : ColorSet::ButtonIdleLight));

        DrawRectangleRounded(bounds, 0.18f, 8, fill);
        DrawRectangleRoundedLines(bounds, 0.18f, 8, 1.0f, __GridLineColor());

        float textWidth = __MeasureTextUI(text, 17);
        __DrawTextUIBold(text, bounds.x + (bounds.width - textWidth) / 2.0f, bounds.y + (bounds.height - __S(21.0f)) * 0.5f, 17, active ? ColorSet::ActiveButtonText : __TextColor());
        return pressed;
    }

    bool App::__Slider(Rectangle bounds, const char* label, float* value, float minValue, float maxValue, bool integerDisplay) {
        const float labelW  = std::min(__S(132.0f), bounds.width * 0.34f);
        const float valueW  = __S(58.0f);
        const float gap     = __S(10.0f);
        const float barX    = bounds.x + labelW + gap;
        const float barW    = std::max(__S(70.0f), bounds.width - labelW - valueW - gap * 2.0f);
        const float barY    = bounds.y + bounds.height * 0.5f - __S(4.0f);

        __DrawTextUIBold(label, bounds.x, bounds.y + __S(3.0f), 15, __TextColor());

        Rectangle bar{ barX, barY, barW, __S(8.0f) };
        DrawRectangleRounded(bar, 0.4f, 8, __themeMode == ThemeMode::Dark ? ColorSet::ControlTrackDark : ColorSet::ControlTrackLight);

        float ratio = (*value - minValue) / (maxValue - minValue);
        ratio = std::clamp(ratio, 0.0f, 1.0f);
        Rectangle progress = bar;
        progress.width *= ratio;
        DrawRectangleRounded(progress, 0.4f, 8, __AccentColor());

        float knobX = bar.x + bar.width * ratio;
        DrawCircle(static_cast<int>(knobX), static_cast<int>(bar.y + bar.height / 2.0f), __S(8.0f), __themeMode == ThemeMode::Dark ? ColorSet::OverlayDark : ColorSet::OverlayLight);

        char buffer[64];
        if (std::string(label) == "Obstacle") {
            std::snprintf(buffer, sizeof(buffer), "%.0f%%", *value * 100.0f);
        }
        else if (integerDisplay) {
            std::snprintf(buffer, sizeof(buffer), "%.0f", *value);
        }
        else {
            std::snprintf(buffer, sizeof(buffer), "%.2f", *value);
        }
        __DrawTextUIBold(buffer, bar.x + bar.width + gap, bounds.y + __S(3.0f), 15, __MutedTextColor());

        bool changed = false;
        if (CheckCollisionPointRec(GetMousePosition(), { bar.x - __S(8.0f), bar.y - __S(14.0f), bar.width + __S(16.0f), bar.height + __S(28.0f) }) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float newRatio = (GetMouseX() - bar.x) / bar.width;
            newRatio = std::clamp(newRatio, 0.0f, 1.0f);
            *value = minValue + newRatio * (maxValue - minValue);
            changed = true;
        }
        return changed;
    }

    float App::__S(float value) const {
        return value * __uiScale;
    }

    void App::__DrawTextUI(const std::string& text, float x, float y, float fontSize, Color color) const {
        const float scaledSize = fontSize * __uiScale;
        const float spacing = 0.20f * __uiScale;
        DrawTextEx(__uiFont, text.c_str(), Vector2{ x, y }, scaledSize, spacing, color);
    }

    void App::__DrawTextUIBold(const std::string& text, float x, float y, float fontSize, Color color) const {
        const float scaledSize  = fontSize * __uiScale;
        const float spacing     = 0.20f * __uiScale;
        const float offset      = std::max(0.55f, 0.55f * __uiScale);
        DrawTextEx(__uiFont, text.c_str(), Vector2{ x, y }, scaledSize, spacing, color);
        DrawTextEx(__uiFont, text.c_str(), Vector2{ x + offset, y }, scaledSize, spacing, color);
        DrawTextEx(__uiFont, text.c_str(), Vector2{ x, y + offset * 0.35f }, scaledSize, spacing, color);
    }

    float App::__MeasureTextUI(const std::string& text, float fontSize) const {
        const float scaledSize = fontSize * __uiScale;
        return MeasureTextEx(__uiFont, text.c_str(), scaledSize, 0.20f * __uiScale).x;
    }

    int App::__FrameCount() const {
        return __hasResult ? static_cast<int>(__result.frames.size()) : 0;
    }

    const SearchFrame* App::__CurrentFrame() const {
        if (!__hasResult || __result.frames.empty()) return nullptr;
        int idx = std::clamp(__frameIndex, 0, __FrameCount() - 1);
        return &__result.frames[static_cast<size_t>(idx)];
    }

    int App::__MouseToCell(Vector2 mouse) const {
        const float padding         = __S(28.0f);
        const float usableW         = __canvas.width - 2.0f * padding;
        const float usableH         = __canvas.height - 2.0f * padding;
        const float baseCellSize    = std::floor(std::min(usableW / static_cast<float>(__map.Cols()), usableH / static_cast<float>(__map.Rows())));
        const float cellSize        = std::max(3.0f, baseCellSize * __mapZoom2D);
        if (cellSize <= 0.0f) return -1;

        const float gridW = cellSize * static_cast<float>(__map.Cols());
        const float gridH = cellSize * static_cast<float>(__map.Rows());
        const float startX = __canvas.x + (__canvas.width - gridW) / 2.0f + __mapPan2D.x;
        const float startY = __canvas.y + (__canvas.height - gridH) / 2.0f + __mapPan2D.y;

        if (mouse.x < startX || mouse.y < startY || mouse.x >= startX + gridW || mouse.y >= startY + gridH) {
            return -1;
        }

        int col = static_cast<int>((mouse.x - startX) / cellSize);
        int row = static_cast<int>((mouse.y - startY) / cellSize);
        if (!__map.InBounds(row, col)) return -1;
        return __map.Index(row, col);
    }

    int App::__PickCell3D(Vector2 mouse) const {
        if (!CheckCollisionPointRec(mouse, __canvas)) return -1;

        Ray ray = GetMouseRay(mouse, __camera);
        int bestCell = -1;
        float bestDistance = 1.0e30f;

        for (int cell = 0; cell < __map.Count(); ++cell) {
            BoundingBox box = __CellBoundingBox3D(cell);
            RayCollision hit = GetRayCollisionBox(ray, box);
            if (hit.hit && hit.distance < bestDistance) {
                bestDistance = hit.distance;
                bestCell = cell;
            }
        }

        return bestCell;
    }

    BoundingBox App::__CellBoundingBox3D(int cell) const {
        if (cell < 0 || cell >= __map.Count()) {
            return BoundingBox{ Vector3{ 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 0.0f } };
        }

        GridCoord g = __map.Coord(cell);
        const float x = static_cast<float>(g.col) - static_cast<float>(__map.Cols() - 1) * 0.5f;
        const float z = static_cast<float>(g.row) - static_cast<float>(__map.Rows() - 1) * 0.5f;
        const float half = 0.46f;
        const float h = __CellVisualHeight3D(cell);

        return BoundingBox{
            Vector3{ x - half, 0.0f, z - half },
            Vector3{ x + half, std::max(0.10f, h), z + half }
        };
    }

    float App::__SoftSurfaceHeightAt(int row, int col) const {
        row = std::clamp(row, 0, __map.Rows() - 1);
        col = std::clamp(col, 0, __map.Cols() - 1);

        float sum = 0.0f;
        float weightSum = 0.0f;
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                int rr  = std::clamp(row + dr, 0, __map.Rows() - 1);
                int cc  = std::clamp(col + dc, 0, __map.Cols() - 1);
                int idx = __map.Index(rr, cc);
                float h = static_cast<float>(__map.GetCell(idx).height) / 9.0f;
                if (__map.IsWater(idx)) h *= 0.12f;
                const float w = (dr == 0 && dc == 0) ? 3.0f : ((std::abs(dr) + std::abs(dc)) == 1 ? 1.25f : 0.55f);
                sum += std::clamp(h, 0.0f, 1.0f) * w;
                weightSum += w;
            }
        }

        const float h01 = weightSum > 0.0f ? sum / weightSum : 0.0f;
        const float scale = std::max(0.10f, __terrainHeightScale) * 0.85f;
        return 0.09f + h01 * scale;
    }

    float App::__CellVisualHeight3D(int cell) const {
        if (cell < 0 || cell >= __map.Count()) return 0.0f;
        if (!__smoothTerrain3D) return __CellSurfaceHeight(cell);
        GridCoord g = __map.Coord(cell);
        return __SoftSurfaceHeightAt(g.row, g.col);
    }

    Color App::__CellColor(int cell, const SearchFrame* frame) const {
        const Cell& mapCell = __map.GetCell(cell);
        const float hNorm = static_cast<float>(mapCell.height) / 10.0f;
        Color color = __TerrainVisualColor(mapCell.terrain, hNorm, false);

        if (__map.IsObstacle(cell) && mapCell.terrain != TerrainType::Water) {
            color = __themeMode == ThemeMode::Dark ? ColorSet::CellObstacleDark : ColorSet::CellObstacleLight;
        }

        if (frame != nullptr) {
            Color closed    = __themeMode == ThemeMode::Dark ? ColorSet::CellClosedDark  : ColorSet::CellClosedLight;
            Color open      = __themeMode == ThemeMode::Dark ? ColorSet::CellOpenDark    : ColorSet::CellOpenLight;
            Color path      = __themeMode == ThemeMode::Dark ? ColorSet::CellPathDark    : ColorSet::CellPathLight;
            Color current   = __themeMode == ThemeMode::Dark ? ColorSet::CellCurrentDark : ColorSet::AccentLight;
            if (__ContainsCell(frame->closed, cell))    color = Blend(color, closed, 0.68f);
            if (__ContainsCell(frame->open, cell))      color = Blend(color, open, 0.70f);
            if (__ContainsCell(frame->path, cell))      color = path;
            if (frame->current == cell)                 color = current;
        }

        if (cell == __map.Start()) return __SuccessColor();
        if (cell == __map.Goal())  return __DangerColor();
        return color;
    }

    float App::__CellSurfaceHeight(int cell) const {
        if (cell < 0 || cell >= __map.Count()) return 0.0f;
        const Cell& c = __map.GetCell(cell);
        float h = 0.12f + static_cast<float>(c.height) * __terrainHeightScale;
        if (__map.IsObstacle(cell) || __map.IsWater(cell)) h = std::max(0.65f, h + 0.55f);
        if (c.terrain == TerrainType::Water) h = std::max(0.18f, h * 0.32f);
        return h;
    }

    Vector3 App::__CellWorldPosition(int cell, float extraHeight) const {
        GridCoord g = __map.Coord(cell);
        const float x = static_cast<float>(g.col) - static_cast<float>(__map.Cols() - 1) * 0.5f;
        const float z = static_cast<float>(g.row) - static_cast<float>(__map.Rows() - 1) * 0.5f;
        return { x, __CellVisualHeight3D(cell) + extraHeight, z };
    }

    Vector3 App::__TerrainVertexWorld(int row, int col, float extraHeight) const {
        row = std::clamp(row, 0, __map.Rows() - 1);
        col = std::clamp(col, 0, __map.Cols() - 1);
        const float x = static_cast<float>(col) - static_cast<float>(__map.Cols() - 1) * 0.5f;
        const float z = static_cast<float>(row) - static_cast<float>(__map.Rows() - 1) * 0.5f;

        if (__smoothTerrain3D) {
            return { x, __SoftSurfaceHeightAt(row, col) + extraHeight, z };
        }

        float sum = 0.0f;
        int count = 0;
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                int rr = row + dr;
                int cc = col + dc;
                if (rr < 0 || rr >= __map.Rows() || cc < 0 || cc >= __map.Cols()) continue;
                sum += __CellSurfaceHeight(__map.Index(rr, cc));
                ++count;
            }
        }
        const float averagedHeight = count > 0 ? sum / static_cast<float>(count) : __CellSurfaceHeight(__map.Index(row, col));
        return { x, averagedHeight + extraHeight, z };
    }

    Vector3 App::__CatmullRom(Vector3 p0, Vector3 p1, Vector3 p2, Vector3 p3, float t) const {
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

    bool App::__ContainsCell(const std::vector<int>& values, int cell) const {
        return std::find(values.begin(), values.end(), cell) != values.end();
    }

    std::string App::__EditModeName() const {
        switch (__editMode) {
        case EditMode::Select:      return "Select";
        case EditMode::Wall:        return "Wall";
        case EditMode::Erase:       return "Erase";
        case EditMode::Start:       return "Start";
        case EditMode::Goal:        return "Goal";
        case EditMode::Raise:       return "Height+";
        case EditMode::Lower:       return "Height-";
        case EditMode::Plain:       return "Plain";
        case EditMode::Road:        return "Road";
        case EditMode::Forest:      return "Forest";
        case EditMode::Mountain:    return "Mountain";
        case EditMode::Water:       return "Water";
        default:                    return "Unknown";
        }
    }

    std::string App::__MovementProfileName() const {
        return ProfileDisplayName(__movementProfile);
    }

    std::string App::__ThemeName() const {
        return __themeMode == ThemeMode::Dark ? "Dark" : "Light";
    }

    std::string App::__SelectedMapPath() const {
        int slot = std::clamp(__selectedMapSlot, 1, 3);
        return "maps/slot" + std::to_string(slot) + ".pathmap";
    }

    std::string App::__CellInfo(int cell) const {
        if (cell < 0 || cell >= __map.Count()) return "Selected: none";

        GridCoord g = __map.Coord(cell);
        const Cell& c = __map.GetCell(cell);
        std::string type = __map.IsWalkable(cell, __movementProfile) ? "walkable for " + __MovementProfileName() : "blocked for " + __MovementProfileName();
        if (cell == __map.Start()) type = "start";
        if (cell == __map.Goal()) type = "goal";

        return "cell " + std::to_string(g.row) + "," + std::to_string(g.col)
            + " | " + TerrainName(c.terrain)
            + " | h=" + std::to_string(c.height)
            + " | " + type;
    }

}
