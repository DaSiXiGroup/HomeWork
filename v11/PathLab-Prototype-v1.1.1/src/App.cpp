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

        // Format a floating point number for UI text, the original style was too ugly...
        // also good for my eyes :)
        std::string FormatDouble(double val, int digits = 2) {
            char buffer[64];
            std::snprintf(buffer, sizeof(buffer), "%.*f", digits, val);
            return std::string(buffer);
        }

        // code below is for 3D
        void DrawQuad3D(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color color) {
            DrawTriangle3D(a, b, c, color);
            DrawTriangle3D(a, c, d, color);
            DrawTriangle3D(c, b, a, color);
            DrawTriangle3D(d, c, a, color);
        }

        // Draw a solid cube with fake lighting, otherwise every face looks dead flat
        // you won't understand how diffcult it is unless you do it by yourself
        // I HATE YOU RAYLIB!
        void DrawCubeShade(Vector3 center, Vector3 size, Color base) {
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

            const Color top   = ScaleColor(base, 1.08f);
            const Color east  = ScaleColor(base, 0.92f);
            const Color west  = ScaleColor(base, 0.80f);
            const Color south = ScaleColor(base, 0.98f);
            const Color north = ScaleColor(base, 0.86f);

            DrawQuad3D(v010, v110, v111, v011, top);
            DrawQuad3D(v100, v101, v111, v110, east);
            DrawQuad3D(v000, v010, v011, v001, west);
            DrawQuad3D(v001, v011, v111, v101, south);
            DrawQuad3D(v000, v100, v110, v010, north);
        }

        // Draw a thin raised plate; used for roads, water patches , etc
        void DrawCubeFlat(Vector3 center, Vector3 size, Color base) {
            const Color top = ScaleColor(base, 1.06f);
            const Color side = ScaleColor(base, 0.94f);
            DrawCubeShade(center, size, side);

            const float x0 = center.x - size.x * 0.5f;
            const float x1 = center.x + size.x * 0.5f;
            const float y  = center.y + size.y * 0.5f + 0.002f;
            const float z0 = center.z - size.z * 0.5f;
            const float z1 = center.z + size.z * 0.5f;
            DrawQuad3D({ x0, y, z0 }, { x1, y, z0 }, { x1, y, z1 }, { x0, y, z1 }, top);
        }

        const std::array<const char*, 5> TERRAIN_NAMES{ "Plain", "Road", "Forest", "Water", "Mountain" };
        const std::array<const char*, 4> PROFILE_NAMES{ "Pedestrian", "Car", "Offroad", "Drone" };

        // Turn terrain enum into text for UI
        const char* TerrainName(Terrain terrain) {
            return TERRAIN_NAMES[static_cast<size_t>(terrain)];
        }

        // I also hate you WHITE CHARS!
        std::string TrimDialogPath(std::string val) {
            while (!val.empty() && (val.back() == '\n' || val.back() == '\r' || val.back() == ' ' || val.back() == '\t')) {
                val.pop_back();
            }
            while (!val.empty() && (val.front() == '\n' || val.front() == '\r' || val.front() == ' ' || val.front() == '\t')) {
                val.erase(val.begin());
            }
            return val;
        }

        // file system, I wrote these for different OS, such as Windows and MacOS
        // maybe Linux later?
        FILE* OpenPipe(const std::string& cmd) {
#ifdef _WIN32
            return _popen(cmd.c_str(), "r");
#else
            return popen(cmd.c_str(), "r");
#endif
        }

        // Close the cmd pipe with the matching OS call, boring but necessary
        int ClosePipe(FILE* pipe) {
#ifdef _WIN32
            return _pclose(pipe);
#else
            return pclose(pipe);
#endif
        }

        // Run a tiny native cmd and capture its out; mainly for file dialogs
        std::string CaptureCommand(const std::string& cmd) {
            std::string out;
            FILE* pipe = OpenPipe(cmd);
            if (pipe == nullptr) return out;
            char buffer[512];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                out += buffer;
            }
            ClosePipe(pipe);
            return TrimDialogPath(out);
        }

        // Ask the OS for a map path; this part is ugly because cross-platform GUI dialogs are ugly
        std::string MapDlg(bool save) {
#ifdef _WIN32
            std::string cmd = save
                ? "powershell -NoProfile -Sta -ExecutionPolicy Bypass -Command \"Add-Type -AssemblyName System.Windows.Forms; [Console]::OutputEncoding=[System.Text.Encoding]::UTF8; $d=New-Object System.Windows.Forms.SaveFileDialog; $d.Title='Save PathLab map'; $d.Filter='PathLab Map (*.pathmap)|*.pathmap|All Files (*.*)|*.*'; $d.DefaultExt='pathmap'; $d.FileName='map.pathmap'; if($d.ShowDialog() -eq 'OK'){ Write-Output $d.FileName }\""
                : "powershell -NoProfile -Sta -ExecutionPolicy Bypass -Command \"Add-Type -AssemblyName System.Windows.Forms; [Console]::OutputEncoding=[System.Text.Encoding]::UTF8; $d=New-Object System.Windows.Forms.OpenFileDialog; $d.Title='Open PathLab map'; $d.Filter='PathLab Map (*.pathmap)|*.pathmap|All Files (*.*)|*.*'; if($d.ShowDialog() -eq 'OK'){ Write-Output $d.FileName }\"";
            return CaptureCommand(cmd);
#elif defined(__APPLE__)
            std::string cmd = save
                ? "osascript -e 'POSIX path of (choose file name with prompt \"Save PathLab map\" default name \"map.pathmap\")' 2>/dev/null"
                : "osascript -e 'POSIX path of (choose file with prompt \"Open PathLab map\")' 2>/dev/null";
            return CaptureCommand(cmd);
#else
            std::string cmd = save
                ? "zenity --file-selection --save --confirm-overwrite --title='Save PathLab map' --filename='map.pathmap' 2>/dev/null"
                : "zenity --file-selection --title='Open PathLab map' 2>/dev/null";
            return CaptureCommand(cmd);
#endif
        }

        // Add .pathmap when the user forgets it, because of course someone will forget it
        std::string FixMapExt(std::string path) {
            if (path.empty()) return path;
            std::filesystem::path p(path);
            if (p.extension().empty()) p.replace_extension(".pathmap");
            return p.string();
        }

        // Ask the OS where to save the screenshot
        std::string PngDlg() {
#ifdef _WIN32
            std::string cmd = "powershell -NoProfile -Sta -ExecutionPolicy Bypass -Command \"Add-Type -AssemblyName System.Windows.Forms; [Console]::OutputEncoding=[System.Text.Encoding]::UTF8; $d=New-Object System.Windows.Forms.SaveFileDialog; $d.Title='Export PathLab screenshot'; $d.Filter='PNG Image (*.png)|*.png|All Files (*.*)|*.*'; $d.DefaultExt='png'; $d.FileName='pathlab_screenshot.png'; if($d.ShowDialog() -eq 'OK'){ Write-Output $d.FileName }\"";
            return CaptureCommand(cmd);
#elif defined(__APPLE__)
            std::string cmd = "osascript -e 'POSIX path of (choose file name with prompt \"Export PathLab screenshot\" default name \"pathlab_screenshot.png\")' 2>/dev/null";
            return CaptureCommand(cmd);
#else
            std::string cmd = "zenity --file-selection --save --confirm-overwrite --title='Export PathLab screenshot' --filename='pathlab_screenshot.png' 2>/dev/null";
            return CaptureCommand(cmd);
#endif
        }

        // Add .png if the dialog result has no extension
        std::string FixPngExt(std::string path) {
            if (path.empty()) return path;
            std::filesystem::path p(path);
            if (p.extension().empty()) p.replace_extension(".png");
            return p.string();
        }

        // Show only the filename in the panel; full paths are just visual pollution here
        std::string CompactPath(const std::string& path) {
            if (path.empty()) return "No file selected";
            std::filesystem::path p(path);
            std::string name = p.filename().string();
            if (name.empty()) name = path;
            return name.size() > 34 ? (name.substr(0, 31) + "...") : name;
        }

        // to be consist with raylib, I use Vec2 for every 2D vector
        // awful namestyle
        // I hate this
        float Vec2Distance(Vector2 a, Vector2 b) {
            const float dx = a.x - b.x;
            const float dy = a.y - b.y;
            return std::sqrt(dx * dx + dy * dy);
        }

        // OK so I forgot how to use friend
        // but it doesn't matter (?
        Vector2 Vec2Add(Vector2 a, Vector2 b) { return Vector2{ a.x + b.x, a.y + b.y }; }
        Vector2 Vec2Sub(Vector2 a, Vector2 b) { return Vector2{ a.x - b.x, a.y - b.y }; }
        Vector2 Vec2Scale(Vector2 v, float s) { return Vector2{ v.x * s, v.y * s }; }

        // Length of a 2D vector.
        float Vec2Length(Vector2 v) {
            return std::sqrt(v.x * v.x + v.y * v.y);
        }

        // that means, to make a vector's length 1
        Vector2 Vec2Normalize(Vector2 v) {
            const float len = Vec2Length(v);
            if (len <= 0.0001f) return Vector2{ 0.0f, 0.0f };
            return Vector2{ v.x / len, v.y / len };
        }

        // Distance between two 3D points, used by path tubes and picking helpers
        float Vec3Distance(Vector3 a, Vector3 b) {
            const float dx = a.x - b.x;
            const float dy = a.y - b.y;
            const float dz = a.z - b.z;
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }

        Vector3 Vec3Add(Vector3 a, Vector3 b) { return Vector3{ a.x + b.x, a.y + b.y, a.z + b.z }; }
        Vector3 Vec3Sub(Vector3 a, Vector3 b) { return Vector3{ a.x - b.x, a.y - b.y, a.z - b.z }; }
        Vector3 Vec3Scale(Vector3 v, float s) { return Vector3{ v.x * s, v.y * s, v.z * s }; }

        // Length of a 3D vector
        float Vec3Length(Vector3 v) {
            return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        }

        // that means, to make a vector's length 1
        Vector3 Vec3Normalize(Vector3 v) {
            const float len = Vec3Length(v);
            if (len <= 0.0001f) return Vector3{ 0.0f, 0.0f, 0.0f };
            return Vector3{ v.x / len, v.y / len, v.z / len };
        }

        // a trick for your dad SDLTF
        // so these two function are used to make the path more smooth
        // I don't think it's neccessary but it's good for my eyes
        Vector2 QuadBez2(Vector2 a, Vector2 control, Vector2 b, float t) {
            const float u = 1.0f - t;
            return Vector2{
                u * u * a.x + 2.0f * u * t * control.x + t * t * b.x,
                u * u * a.y + 2.0f * u * t * control.y + t * t * b.y
            };
        }

        // 3D quadratic Bezier point; same trick, but for the tube path
        Vector3 QuadBez3(Vector3 a, Vector3 control, Vector3 b, float t) {
            const float u = 1.0f - t;
            return Vector3{
                u * u * a.x + 2.0f * u * t * control.x + t * t * b.x,
                u * u * a.y + 2.0f * u * t * control.y + t * t * b.y,
                u * u * a.z + 2.0f * u * t * control.z + t * t * b.z
            };
        }


        // OK so these two function is used to simply the path
        // for example, think about there are three points A, B, C, which are all in a line
        // so you don't have to draw point B, we just care avbout the begin and the end
        std::vector<Vector2> ZipLine2(const std::vector<Vector2>& points) {
            if (points.size() <= 2) return points; // corner case
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

        // Remove useless middle points on straight 3D segments; otherwise smoothing overworks itself
        std::vector<Vector3> ZipLine3(const std::vector<Vector3>& points) {
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

        // these two function is for turning the hard path into smooth path
		// use these function after ZipLine2 or ZipLine3
        std::vector<Vector2> RoundLine2(const std::vector<Vector2>& points, float radius, int curveSteps) {
            std::vector<Vector2> p = ZipLine2(points);
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
                    out.push_back(QuadBez2(a, corner, b, static_cast<float>(s) / static_cast<float>(curveSteps)));
                }
            }
            if (Vec2Distance(out.back(), p.back()) > 0.35f) out.push_back(p.back());
            return out;
        }

        std::vector<Vector3> RoundLine3(const std::vector<Vector3>& points, float radius, int curveSteps) {
            std::vector<Vector3> p = ZipLine3(points);
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
                    out.push_back(QuadBez3(a, corner, b, static_cast<float>(s) / static_cast<float>(curveSteps)));
                }
            }
            if (Vec3Distance(out.back(), p.back()) > 0.02f) out.push_back(p.back());
            return out;
        }

        // So we can draw line now!
        void DrawPath2D(const std::vector<Vector2>& points, float width, Color outline, Color core) {
            if (points.size() < 2) return;
            for (size_t i = 1; i < points.size(); ++i) DrawLineEx(points[i - 1], points[i], width + 3.0f, outline);
            for (size_t i = 1; i < points.size(); ++i) DrawLineEx(points[i - 1], points[i], width, core);
        }

        void DrawTube3D(const std::vector<Vector3>& points, float radius, Color outline, Color core) {
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

        // this function is for make choose the color
        Color ProfileColor(MoveProfile prof, Theme theme) {
            const int id = static_cast<int>(prof);
            if (id < 0 || id >= static_cast<int>(Pal::ProfD.size())) {
                return PickColor(theme, Pal::ProfFallbackD, Pal::ProfFallbackL);
            }
            const size_t index = static_cast<size_t>(id);
            return PickColor(theme, Pal::ProfD[index], Pal::ProfL[index]);
        }

        // Turn movement prof enum into UI text
        const char* ProfNameOf(MoveProfile prof) {
            return PROFILE_NAMES[static_cast<size_t>(prof)];
        }

        // this function is for choose the font
        // I haven't tried other font anyway
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

    // Construct the app and give it a map immediately
    // we design this to avoid a temp window
    App::App() : __map(35, 55) {
        __GenRandom();
    }

    // Main application loop: layout, update, draw, repeat until the window closes
    void App::Run() {
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_WINDOW_MAXIMIZED);
        InitWindow(__WIN_W, __WIN_H, "PathLab Prototype v1.1.1, from DaSiXi Group");
        SetWindowMinSize(1280, 760);
        MaximizeWindow();
        SetTargetFPS(60); // Liushi Zhen ? Liu Shizhen!
        // bbgyfddd
        __font = GetFontDefault();
        __LoadFont();
        __ResetCam();

        while (!WindowShouldClose()) {
            __Layout();
            __Update();
            __Draw();
        }

        __UnloadFont();
        CloseWindow();
    }

    // Recalculate panel rectangles after resize
    // UI coordinates all start from here
    void App::__Layout() {
        __UpdateScale();

        const float topH    = __S(64.0f);
        const float bottomH = __S(96.0f);
        const float leftW   = __S(360.0f);
        const float rightW  = __S(410.0f);

        __lPanel     = { 0, topH, leftW, static_cast<float>(GetScreenHeight()) - topH - bottomH };
        __rPanel    = { static_cast<float>(GetScreenWidth()) - rightW, topH, rightW, static_cast<float>(GetScreenHeight()) - topH - bottomH };
        __canvas        = { leftW, topH, static_cast<float>(GetScreenWidth()) - leftW - rightW, static_cast<float>(GetScreenHeight()) - topH - bottomH };
        __bPanel   = { 0, static_cast<float>(GetScreenHeight()) - bottomH, static_cast<float>(GetScreenWidth()), bottomH };
    }

    // Handle keyboard, mouse, scrolling, camera and animation state each frm
    void App::__Update() {
        // so there comes the update by keyboard
        if (IsKeyPressed(KEY_F11)) {
            // F11
            __ToggleFull();
        }
        if (IsKeyPressed(KEY_F10) && !__full) {
            // F10
            MaximizeWindow();
        }
        if (IsKeyPressed(KEY_F5)) {
            // F5
            __ToggleView();
        }
        if (IsKeyPressed(KEY_F6)) {
            // F6
            __ToggleTheme();
        }
        if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_Z)) {
            // Ctrl + Z
            __UndoEdit();
        }
        if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_Y)) {
            // Ctrl + Y
            __RedoEdit();
        }
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) || IsMouseButtonReleased(MOUSE_RIGHT_BUTTON)) {
            // left button add, right button remove
            __lastCell = -1;
        }
        const Vector2 mouse = GetMousePosition();
        const float wheel = GetMouseWheelMove();
        if (std::abs(wheel) > 0.001f && CheckCollisionPointRec(mouse, __lPanel)) {
            __lScroll = std::clamp(__lScroll + wheel * __S(54.0f), -__S(760.0f), 0.0f);
        }
        else if (std::abs(wheel) > 0.001f && CheckCollisionPointRec(mouse, __rPanel)) {
            __rScroll = std::clamp(__rScroll + wheel * __S(54.0f), -__S(660.0f), 0.0f);
        }
        else if (__is3D) {
            __UpdateCam();
        }
        else if (CheckCollisionPointRec(mouse, __canvas)) {
            // ok so if you don't use abs
            // you'll fail
            if (std::abs(wheel) > 0.001f) {
                const float oldZoom     =   __zoom2D;
                __zoom2D                *=  std::pow(1.12f, wheel);
                __zoom2D                =   std::clamp(__zoom2D, 0.55f, 5.0f);
                const float zoomRatio   =   __zoom2D / oldZoom;
                Vector2 center{ __canvas.x + __canvas.width * 0.5f, __canvas.y + __canvas.height * 0.5f };
                __pan2D.x = mouse.x - center.x - (mouse.x - center.x - __pan2D.x) * zoomRatio;
                __pan2D.y = mouse.y - center.y - (mouse.y - center.y - __pan2D.y) * zoomRatio;
                __status  = "2D zoom: " + FormatDouble(__zoom2D, 2) + "x";
            }
            // xuanzhuan, tiaoyue, wo bi zhe yan ~
            if (IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
                Vector2 delta = GetMouseDelta();
                __pan2D.x += delta.x;
                __pan2D.y += delta.y;
            }
        }

        __HandleEdit();

        if (__play && __hasRes && __FrameCnt() > 0) {
            __acc += GetFrameTime();
            const float interval = 1.0f / std::max(1.0f, __animSpd);
            while (__acc >= interval) {
                __acc -= interval;
                __frmIdx++;
                if (__frmIdx >= __FrameCnt() - 1) {
                    __frmIdx = __FrameCnt() - 1;
                    __play   = false;
                    __status = "Animation finished";
                    break;
                }
            }
        }
    }

    // Draw the whole frm in the correct order
    // background first, panels later, map in between
    void App::__Draw() {
        BeginDrawing();
        ClearBackground(__CanvasColor());

        __DrawTopBar();
        __DrawLeft();
        __DrawRight();
        __DrawBottom();
        if (__is3D) {
            __Draw3D();
        }
        else {
            __DrawMap();
        }

        EndDrawing();
    }

    // Scale UI from cur window size
    void App::__UpdateScale() {
        const float widthScale = static_cast<float>(GetScreenWidth()) / 1440.0f;
        const float heightScale = static_cast<float>(GetScreenHeight()) / 900.0f;
        __scale = std::clamp(std::min(widthScale, heightScale), 0.95f, 1.35f);
    }

    // Switch between fullscreen and windowed mode while remembering old window size
    // I HATE WINDOW SIZE!
    void App::__ToggleFull() {
        if (!__full) {
            __winW = GetScreenWidth();
            __winH = GetScreenHeight();
            const int monitor   = GetCurrentMonitor();
            SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
            ToggleFullscreen();
            __full    = true;
            __status = "Fullscreen enabled; press F11 to return";
        }
        else {
            ToggleFullscreen();
            SetWindowSize(__winW, __winH);
            __full   = false;
            __status = "Windowed mode restored";
        }
    }

    // Toggle 2D editor and 3D terrain view
    void App::__ToggleView() {
        __is3D = !__is3D;
        if (__is3D) {
            __ResetCam();
            __status = "3D terrain view enabled. Left click selects/edits; right or middle drag rotates.";
        }
        else {
            __status = "2D map view enabled. Editing tools are available.";
        }
    }

    // Toggle light and dark theme
    // From where I stand I prefer dark model
    // but not every one share the same taste with me
    // Oh my god~ ni xia dao wo le ~
    void App::__ToggleTheme() {
        __theme = (__theme == Theme::Light) ? Theme::Dark : Theme::Light;
        __status = "Theme switched to " + __ThemeName() + ".";
    }

    // OK so the most awful part comes~
    // what if I don't use two seperate theme ...
    
    // Theme-aware normal panel color
    Color App::__PanelColor() const {
        return __theme == Theme::Dark ? Pal::PanelD : Pal::PanelL;
    }

    // Theme-aware darker panel color for bars and timeline
    Color App::__PanelDeep() const {
        return __theme == Theme::Dark ? Pal::PanelDeepD : Pal::PanelDeepL;
    }

    // Main accent color used by selected buttons and highlights
    Color App::__AccentColor() const {
        return __theme == Theme::Dark ? Pal::AccentD : Pal::AccentL;
    }

    // Main readable text color for cur theme
    Color App::__TextColor() const {
        return __theme == Theme::Dark ? Pal::TextD : Pal::TextL;
    }

    // Secondary text color
    Color App::__MutedColor() const {
        return __theme == Theme::Dark ? Pal::TextMuteD : Pal::TextMuteL;
    }

    // Grid and panel border color for cur theme
    Color App::__GridColor() const {
        return __theme == Theme::Dark ? Pal::GridD : Pal::GridL;
    }

    // Color for goal, failed result and warning text
    Color App::__DangerColor() const {
        return __theme == Theme::Dark ? Pal::DangerD : Pal::DangerL;
    }

    // Color for start point and successful result
    Color App::__SuccessColor() const {
        return __theme == Theme::Dark ? Pal::SuccessD : Pal::SuccessL;
    }

    // Background color of the map canvas
    Color App::__CanvasColor() const {
        return __theme == Theme::Dark ? Pal::CanvasD : Pal::CanvasL;
    }

    // Overlay color for hover and selection markers
    Color App::__OverlayColor() const {
        return __theme == Theme::Dark ? Pal::OverlayD : Pal::OverlayL;
    }

    // Convert terrain plus height into a visual color, including side-face shading
    // you know in geography, we use darker color to present higher height
    // so I think this is a good idea, and I use this
    Color App::__TerrainColor(Terrain terrain, float h01, bool side) const {
        h01 = std::clamp(h01, 0.0f, 1.0f);

        int id = static_cast<int>(terrain);
        if (id < 0 || id >= static_cast<int>(Pal::TerrD.size())) id = 0;

        const auto& ramp = (__theme == Theme::Dark ? Pal::TerrD : Pal::TerrL)[static_cast<size_t>(id)];
        Color color = h01 < 0.55f
            ? Blend(ramp.low, ramp.mid, h01 / 0.55f)
            : Blend(ramp.mid, ramp.high, (h01 - 0.55f) / 0.45f);

        if (side) {
            const Color shade = PickColor(
                __theme,
                WithAlpha(Pal::TerrShadeD, color.a),
                WithAlpha(Pal::TerrShadeL, color.a));
            color = Blend(color, shade, __theme == Theme::Dark ? 0.22f : 0.18f);
        }
        return color;
    }

    // Draw the soft 3D background gradient
    // caz pure solid color looked too cheap
    void App::__DrawGradBg(Rectangle box) const {
        Color top       = __theme == Theme::Dark ? Pal::GradTopD    : Pal::GradTopL;
        Color mid       = __theme == Theme::Dark ? Pal::GradMidD    : Pal::GradMidL;
        Color bottom    = __theme == Theme::Dark ? Pal::GradBotD : Pal::GradBotL;

        const int strips = 96;
        for (int i = 0; i < strips; ++i) {
            float t0    = static_cast<float>(i) / static_cast<float>(strips);
            float t1    = static_cast<float>(i + 1) / static_cast<float>(strips);
            float midT  = (t0 + t1) * 0.5f;
            Color c     = midT < 0.52f ? Blend(top, mid, midT / 0.52f) : Blend(mid, bottom, (midT - 0.52f) / 0.48f);
            Rectangle strip{ box.x, box.y + box.height * t0, box.width, box.height * (t1 - t0) + 1.0f };
            DrawRectangleRec(strip, c);
        }
    }

    // Reset the 3D camera to a sane angle for the cur map size
    void App::__ResetCam() {
        const float mapSize = static_cast<float>(std::max(__map.Rows(), __map.Cols()));
        __camTar   = { 0.0f, __smooth3D ? 0.55f : 0.0f, 0.0f };
        __camDist  = std::max(__smooth3D ? 24.0f : 28.0f, mapSize * (__smooth3D ? 0.82f : 0.95f));
        __camYaw   = 0.78f;
        __camPitch = __smooth3D ? 0.70f : 0.58f;

        __camera.up         = { 0.0f, 1.0f, 0.0f };
        __camera.fovy       = __smooth3D ? 38.0f : 45.0f;
        __camera.projection = CAMERA_PERSPECTIVE;
        __UpdateCam();
    }

    // Update 3D orbit camera and keyboard panning
    void App::__UpdateCam() {
        if (IsKeyPressed(KEY_R) && CheckCollisionPointRec(GetMousePosition(), __canvas)) {
            __ResetCam();
        }

        if (CheckCollisionPointRec(GetMousePosition(), __canvas)) {
            Vector2 delta = GetMouseDelta();
            if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON) || IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
                __camYaw   -= delta.x * 0.008f;
                __camPitch += delta.y * 0.008f; // ok so why 0.008f? because I like it
                                                // you don't have to do so
                __camPitch =  std::clamp(__camPitch, 0.18f, 1.34f);
            }

            float wheel = GetMouseWheelMove();
            if (std::abs(wheel) > 0.001f) {
                __camDist *= (1.0f - wheel * 0.10f);
                __camDist =  std::clamp(__camDist, 12.0f, 180.0f);
            }
        }

        float move = GetFrameTime() * __camDist * 0.55f;
        Vector3 forward{ static_cast<float>(std::sin(__camYaw)), 0.0f, static_cast<float>(std::cos(__camYaw)) };
        Vector3 right{ static_cast<float>(std::cos(__camYaw)), 0.0f, static_cast<float>(-std::sin(__camYaw)) };
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) move *= 2.0f;

        if (IsKeyDown(KEY_W)) {
            __camTar.x += forward.x * move;
            __camTar.z += forward.z * move;
        }
        if (IsKeyDown(KEY_S)) {
            __camTar.x -= forward.x * move;
            __camTar.z -= forward.z * move;
        }
        if (IsKeyDown(KEY_D)) {
            __camTar.x += right.x * move;
            __camTar.z += right.z * move;
        }
        if (IsKeyDown(KEY_A)) {
            __camTar.x -= right.x * move;
            __camTar.z -= right.z * move;
        }
        if (IsKeyDown(KEY_E)) __camTar.y += move * 0.35f;
        if (IsKeyDown(KEY_Q)) __camTar.y -= move * 0.35f;

        const float cp = static_cast<float>(std::cos(__camPitch));
        __camera.target = __camTar;
        __camera.position = {
            __camTar.x + __camDist * static_cast<float>(std::sin(__camYaw)) * cp,
            __camTar.y + __camDist * static_cast<float>(std::sin(__camPitch)),
            __camTar.z + __camDist * static_cast<float>(std::cos(__camYaw)) * cp
        };
    }

    // OK LET'S DRAW!
    // I think maybe QT is a good choice for GUI in this situation
    // but I don't know how to use QT :(
    void App::__DrawTopBar() {
        DrawRectangle(0, 0, GetScreenWidth(), static_cast<int>(__S(64.0f)), __PanelDeep());
        __TextBold("PathLab", __S(20), __S(15), 27, __TextColor());
        __Text("v1.1.1", __S(128), __S(24), 15, __MutedColor());

        const float btnW = __S(132.0f);
        const float btnH = __S(36.0f);
        const float gap  = __S(12.0f);
        float cx = static_cast<float>(GetScreenWidth()) * 0.5f - (btnW * 3.0f + gap * 2.0f) * 0.5f;
        if (__Button({ cx, __S(14.0f), btnW, btnH }, __is3D ? "View: 3D" : "View: 2D")) {
            __ToggleView();
        }
        if (__Button({ cx + btnW + gap, __S(14.0f), btnW, btnH }, __theme == Theme::Dark ? "Theme: Dark" : "Theme: Light")) {
            __ToggleTheme();
        }
        if (__Button({ cx + (btnW + gap) * 2.0f, __S(14.0f), btnW, btnH }, __is3D ? "Reset 3D" : "Reset 2D")) {
            if (__is3D) {
                __ResetCam();
                __status = "3D camera reset";
            }
            else {
                __zoom2D = 1.0f;
                __pan2D  = { 0.0f, 0.0f };
                __status = "2D view reset";
            }
        }

        const char* shortcuts = __full ? "F11 exit | F5 view | F6 theme | wheel zoom" : "F11 fullscreen | F10 maximize | F5 view | F6 theme | wheel zoom";
        __Text(shortcuts, static_cast<float>(GetScreenWidth()) - __S(610.0f), __S(23.0f), 14, __MutedColor());
    }

    // Draw map generation, alg and cost controls on the left
    void App::__DrawLeft() {
        DrawRectangleRec(__lPanel, __PanelColor());
        DrawRectangleLinesEx(__lPanel, 1.0f, __GridColor());
        BeginScissorMode(static_cast<int>(__lPanel.x), static_cast<int>(__lPanel.y), static_cast<int>(__lPanel.width), static_cast<int>(__lPanel.height));

        float       x   = __lPanel.x + __S(22.0f);
        float       y   = __lPanel.y + __S(18.0f) + __lScroll;
        const float cw  = __lPanel.width - __S(40.0f);
        const float gap = __S(10.0f);
        const float bw  = (cw - gap) * 0.5f;

        auto section = [&](const char* title) {
            __TextBold(title, x, y, 20, __TextColor());
            DrawRectangleRec({ x, y + __S(29.0f), cw, 1.0f }, __GridColor());
            y += __S(42.0f);
            };

        section("MAP");
        if (__Button({ x, y, bw, __S(34) }, "Random")) __GenRandom();
        if (__Button({ x + bw + gap, y, bw, __S(34) }, "Terrain")) __GenTerrain();
        y += __S(40);
        if (__Button({ x, y, bw, __S(34) }, "Maze")) __GenMaze();
        if (__Button({ x + bw + gap, y, bw, __S(34) }, "Clear")) {
            __PushUndo("Clear map");
            __map.ClearAll();
            __ResetVis();
            __status = "Map cleared";
        }
        y += __S(40);
        if (__Button({ x, y, bw, __S(34) }, "City preset")) __GenCity();
        if (__Button({ x + bw + gap, y, bw, __S(34) }, "River preset")) __GenRiver();
        y += __S(40);
        if (__Button({ x, y, bw, __S(34) }, "Mountain pass")) __GenMountain();
        if (__Button({ x + bw + gap, y, bw, __S(34) }, "Demo A*")) __RunDemo();
        y += __S(40);
        __Slider({ x, y, cw, __S(24) }, "Rows", &__rowVal, 10.0f, 80.0f, true);
        y += __S(38);
        __Slider({ x, y, cw, __S(24) }, "Columns", &__colVal, 10.0f, 120.0f, true);
        y += __S(38);
        __Slider({ x, y, cw, __S(24) }, "Obstacle", &__obsProb, 0.0f, 0.65f);
        y += __S(40);
        if (__Button({ x, y, cw, __S(34) }, "Apply map size")) __ApplyMapSize();
        y += __S(46);

        section("ALGORITHMS");
        if (__Button({ x, y, bw, __S(34) }, "BFS")) __RunAlg(AlgKind::BFS);
        if (__Button({ x + bw + gap, y, bw, __S(34) }, "Dijkstra")) __RunAlg(AlgKind::Dijkstra);
        y += __S(40);
        if (__Button({ x, y, bw, __S(34) }, "A*")) __RunAlg(AlgKind::AStar);
        if (__Button({ x + bw + gap, y, bw, __S(34) }, "Weighted A*")) __RunAlg(AlgKind::WAStar);
        y += __S(40);
        if (__Button({ x, y, bw, __S(34) }, "Greedy")) __RunAlg(AlgKind::Greedy);
        if (__Button({ x + bw + gap, y, bw, __S(34) }, "Benchmark")) __RunBench();
        y += __S(40);
        if (__TogBtn({ x, y, cw, __S(34) }, __showAgents ? "Agent paths: ON" : "Agent paths: OFF", __showAgents)) {
            if (!__showAgents) __CmpAgents();
            else { __showAgents = false; __agentRows.clear(); __status = "Agent comparison hidden"; }
        }
        y += __S(46);

        section("AGENT & COST");
        if (__TogBtn({ x, y, cw, __S(34) }, __opt.diag ? "Diagonal movement: ON" : "Diagonal movement: OFF", __opt.diag)) {
            __opt.diag = !__opt.diag;
            __status = __opt.diag ? "Diagonal movement enabled" : "Diagonal movement disabled";
        }
        y += __S(40);
        if (__TogBtn({ x, y, bw, __S(34) }, "Pedestrian", __prof == MoveProfile::Pedestrian)) __prof = MoveProfile::Pedestrian;
        if (__TogBtn({ x + bw + gap, y, bw, __S(34) }, "Car", __prof == MoveProfile::Car)) __prof = MoveProfile::Car;
        y += __S(40);
        if (__TogBtn({ x, y, bw, __S(34) }, "Offroad", __prof == MoveProfile::Offroad)) __prof = MoveProfile::Offroad;
        if (__TogBtn({ x + bw + gap, y, bw, __S(34) }, "Drone", __prof == MoveProfile::Drone)) __prof = MoveProfile::Drone;
        y += __S(38);
        __Slider({ x, y, cw, __S(24) }, "Heuristic", &__heurVal, 1.0f, 5.0f);
        y += __S(36);
        __Slider({ x, y, cw, __S(24) }, "Terrain cost", &__terrVal, 0.0f, 5.0f);
        y += __S(36);
        __Slider({ x, y, cw, __S(24) }, "Height cost", &__hgtVal, 0.0f, 2.0f);
        y += __S(40);

        section("VIEW");
        __Slider({ x, y, cw, __S(24) }, "2D zoom", &__zoom2D, 0.55f, 5.0f);
        y += __S(36);
        __Slider({ x, y, cw, __S(24) }, "3D height", &__hgtScale, 0.08f, 0.90f);
        y += __S(40);
        if (__TogBtn({ x, y, bw, __S(34) }, __smooth3D ? "Style: diorama" : "Style: editor", __smooth3D)) {
            __smooth3D = !__smooth3D;
            if (__is3D) __ResetCam();
            __status = __smooth3D ? "Diorama mode: presentation sandbox view" : "Editor mode: precise solid-grid editing";
        }
        if (__TogBtn({ x + bw + gap, y, bw, __S(34) }, __smoothPath ? "Path: curve" : "Path: polyline", __smoothPath)) __smoothPath = !__smoothPath;
        y += __S(40);
        if (__TogBtn({ x, y, cw, __S(34) }, __showGraph ? "Algorithm graph: ON" : "Algorithm graph: OFF", __showGraph)) __showGraph = !__showGraph;
        EndScissorMode();
    }

    // Draw editing tools, playback controls, out buttons and statistics.
    void App::__DrawRight() {
        DrawRectangleRec(__rPanel, __PanelColor());
        DrawRectangleLinesEx(__rPanel, 1.0f, __GridColor());
        BeginScissorMode(static_cast<int>(__rPanel.x), static_cast<int>(__rPanel.y), static_cast<int>(__rPanel.width), static_cast<int>(__rPanel.height));

        float       x     = __rPanel.x + __S(22.0f);
        float       y     = __rPanel.y + __S(18.0f) + __rScroll;
        const float cw    = __rPanel.width - __S(40.0f);
        const float gap   = __S(8.0f);
        const float bw    = (cw - gap * 2.0f) / 3.0f;
        const float halfW = (cw - gap) * 0.5f;
        const float gh    = __S(34.0f);

        auto section = [&](const char* title) {
            __TextBold(title, x, y, 20, __TextColor());
            DrawRectangleRec({ x, y + __S(29.0f), cw, 1.0f }, __GridColor());
            y += __S(42.0f);
            };

        section("TOOLS");
        auto tool = [&](Edit mode, const char* label, int col, int row) {
            if (__TogBtn({ x + col * (bw + gap), y + row * (gh + gap), bw, gh }, label, __edit == mode)) {
                __edit = mode;
            }
            };
        // 0, 1, 2, 3 means different position 
        tool(Edit::Select,      "Select",   0, 0);
        tool(Edit::Wall,        "Wall",     1, 0);
        tool(Edit::Erase,       "Erase",    2, 0);
        tool(Edit::Start,       "Start",    0, 1);
        tool(Edit::Goal,        "Goal",     1, 1);
        tool(Edit::Road,        "Road",     2, 1);
        tool(Edit::Forest,      "Forest",   0, 2);
        tool(Edit::Mountain,    "Mountain", 1, 2);
        tool(Edit::Water,       "Water",    2, 2);
        tool(Edit::Plain,       "Plain",    0, 3);
        tool(Edit::Raise,       "Height+",  1, 3);
        tool(Edit::Lower,       "Height-",  2, 3);
        y += 4 * (gh + gap) + __S(12);
        if (__Button({ x, y, halfW, gh }, "Undo")) __UndoEdit();
        if (__Button({ x + halfW + gap, y, halfW, gh }, "Redo")) __RedoEdit();
        y += __S(42);

        __Text("Mode: " + __EditName(), x, y, 14, __MutedColor());
        y += __S(24);
        __Text(__CellInfo(__selCell), x, y, 14, __selCell >= 0 ? __TextColor() : __MutedColor());
        y += __S(38);

        section("PLAYBACK");
        if (__Button({ x, y, bw, gh }, __play ? "Pause" : "Play")) {
            if (__hasRes) __play = !__play;
        }
        if (__Button({ x + bw + gap, y, bw, gh }, "Reset")) {
            __frmIdx    = 0;
            __play       = false;
        }
        if (__Button({ x + (bw + gap) * 2.0f, y, bw, gh }, "End")) {
            if (__FrameCnt() > 0) __frmIdx = __FrameCnt() - 1;
            __play = false;
        }
        y += __S(40);
        if (__Button({ x, y, bw, gh }, "Prev")) {
            __frmIdx = std::max(0, __frmIdx - 1);
            __play = false;
        }
        if (__Button({ x + bw + gap, y, bw, gh }, "Next")) {
            __frmIdx = std::min(__FrameCnt() - 1, __frmIdx + 1);
            __play = false;
        }
        if (__Button({ x + (bw + gap) * 2.0f, y, bw, gh }, "Replay")) {
            if (__hasRes) {
                __frmIdx    = 0;
                __play       = true;
            }
        }
        y += __S(40);
        __Slider({ x, y, cw, __S(24) }, "Speed", &__animSpd, 1.0f, 160.0f, true);
        y += __S(46);

        section("OUTPUT");
        if (__Button({ x, y, halfW, gh }, "Save as...")) __SaveMap();
        if (__Button({ x + halfW + gap, y, halfW, gh }, "Open...")) __LoadMap();
        y += __S(40);
        if (__Button({ x, y, halfW, gh }, "Screenshot")) __ExportPng();
        if (__Button({ x + halfW + gap, y, halfW, gh }, "CSV")) __ExportCsv();
        y += __S(38);
        __Text("Map file: " + CompactPath(__curMapPath), x, y, 14, __MutedColor());
        y += __S(34);

        section("LEGEND");
        __DrawLegend(x, y, cw);
        y += __S(10);

        section("STATS");
        std::string alg = __hasRes ? __result.algName : "None";
        __Text("__Algorithm: " + alg, x, y, 14, __TextColor());
        y += __S(24);
        __Text("Agent: " + __ProfName(), x, y, 14, __TextColor());
        y += __S(24);
        __Text("Frame: " + std::to_string(__FrameCnt() == 0 ? 0 : __frmIdx + 1) + " / " + std::to_string(__FrameCnt()), x, y, 14, __TextColor());
        y += __S(24);

        if (__hasRes) {
            __Text("Found: " + std::string(__result.found ? "Yes" : "No") + " | Cost: " + FormatDouble(__result.pathLen, 2), x, y, 14, __result.found ? __SuccessColor() : __DangerColor());
            y += __S(24);
            __Text("Expanded: " + std::to_string(__result.expNodes) + " | Generated: " + std::to_string(__result.genNodes), x, y, 14, __TextColor());
            y += __S(24);
            __Text("Time: " + FormatDouble(__result.timeMs, 3) + " ms", x, y, 14, __TextColor());
            y += __S(23);
        }
        else {
            __Text("Run an alg to see data.", x, y, 14, __MutedColor());
            y += __S(26);
        }

        const SearchFrm* frm = __CurFrame();
        if (frm != nullptr) {
            __Text("Open: " + std::to_string(frm->open.size()) + " | Closed: " + std::to_string(frm->closed.size()) + " | Current: " + std::to_string(frm->cur), x, y, 14, __TextColor());
            y += __S(28);
        }

        if (__showAgents && !__agentRows.empty()) {
            __Text("AGENT PATHS", x, y, 16, __TextColor());
            y += __S(24);
            for (const auto& row : __agentRows) {
                Color c = ProfileColor(row.prof, __theme);
                DrawRectangleRec({ x, y + __S(4), __S(11), __S(11) }, c);
                __Text(row.profName + (row.found ? (" | " + FormatDouble(row.pathLen, 1)) : " | no path"), x + __S(18), y, 13, row.found ? __TextColor() : __DangerColor());
                y += __S(20);
            }
            y += __S(8);
        }

        __Text("BENCHMARK", x, y, 16, __TextColor());
        y += __S(24);
        if (__benchRows.empty()) {
            __Text("Click Benchmark to compare algorithms.", x, y, 13, __MutedColor());
        }
        else {
            __Text("Algorithm        Cost    Expanded   Time", x, y, 12, __MutedColor());
            y += __S(18);
            for (const auto& row : __benchRows) {
                char buffer[256];
                std::snprintf(buffer, sizeof(buffer), "%-13s %7.2f %8d %7.3f", row.algName.c_str(), row.pathLen, row.expNodes, row.timeMs);
                __Text(buffer, x, y, 12, row.found ? __TextColor() : __DangerColor());
                y += __S(17);
            }
        }
        EndScissorMode();
    }

    // Draw terrain legend and cur agent movement rule
    void App::__DrawLegend(float x, float& y, float cw) {
        struct LegendItem { Terrain terrain; const char* name; const char* rule; };
        const LegendItem items[] = {
            { Terrain::Plain,       "Plain",    "normal" },
            { Terrain::Road,        "Road",     "fast" },
            { Terrain::Forest,      "Forest",   "slow" },
            { Terrain::Mountain,    "Mountain", "steep" },
            { Terrain::Water,       "Water",    "blocked" }
        };
        const float sw = __S(13.0f);
        for (const auto& item : items) {
            Color color = __TerrainColor(item.terrain, 0.45f, false);
            DrawRectangleRounded({ x, y + __S(3), sw, sw }, 0.18f, 6, color);
            DrawRectangleRoundedLines({ x, y + __S(3), sw, sw }, 0.18f, 6, 1.0f, __GridColor());
            __TextBold(item.name, x + __S(20), y, 13, __TextColor());
            __Text(item.rule, x + cw - __S(96), y, 13, __MutedColor());
            y += __S(20);
        }
        y += __S(4);
        __Text("Agent rules: " + __ProfName(), x, y, 13, __MutedColor());
        y += __S(20);
        std::string rule;
        switch (__prof) {
        case MoveProfile::Pedestrian:   rule = "walks most terrain; water blocked"; break;
        case MoveProfile::Car:          rule = "prefers roads; rough terrain costly"; break;
        case MoveProfile::Offroad:      rule = "handles forest/mountain better"; break;
        case MoveProfile::Drone:        rule = "nearly direct; terrain mostly ignored"; break;
        default:                        rule = "custom movement prof"; break;
        }
        __Text(rule, x, y, 13, __MutedColor());
        y += __S(24);
    }

    // Draw animation timeline and status/help text
    void App::__DrawBottom() {
        DrawRectangleRec(__bPanel, __PanelDeep());
        DrawRectangleLinesEx(__bPanel, 1.0f, __GridColor());

        float x          = __lPanel.width + __S(24.0f);
        float y          = __bPanel.y     + __S(15.0f);
        float rightLimit = __rPanel.x     - __S(24.0f);
        __Text("Timeline", __S(20), y, 20, __TextColor());

        Rectangle bar{ x, __bPanel.y + __S(22), std::max(__S(160.0f), rightLimit - x), __S(10) };
        DrawRectangleRounded(bar, 0.4f, 8, __theme == Theme::Dark ? Pal::TrackD : Pal::TrackL);

        float t = 0.0f;
        if (__FrameCnt() > 1) {
            t = static_cast<float>(__frmIdx) / static_cast<float>(__FrameCnt() - 1);
        }
        Rectangle progress = bar;
        progress.width *= t;
        DrawRectangleRounded(progress, 0.4f, 8, __AccentColor());

        float knobX = bar.x + bar.width * t;
        DrawCircle(static_cast<int>(knobX), static_cast<int>(bar.y + bar.height / 2.0f), 8.0f, __theme == Theme::Dark ? Pal::OverlayD : Pal::OverlayL);

        if (CheckCollisionPointRec(GetMousePosition(), { bar.x - 8, bar.y - 14, bar.width + 16, bar.height + 28 }) && IsMouseButtonDown(MOUSE_LEFT_BUTTON) && __FrameCnt() > 1) {
            float ratio = (GetMouseX() - bar.x) / bar.width;
            ratio       = std::clamp(ratio, 0.0f, 1.0f);
            __frmIdx    = static_cast<int>(ratio * static_cast<float>(__FrameCnt() - 1));
            __play      = false;
        }

        std::string info = __hasRes
            ? (__result.algName + " | step " + std::to_string(__frmIdx + 1) + " / " + std::to_string(__FrameCnt()))
            : "Generate a map, choose a tool, then run an alg.";
        __Text(info, x, __bPanel.y + __S(47.0f), 14, __MutedColor());
        __Text("Status: " + __status, __S(20), __bPanel.y + __S(55.0f), 14, __MutedColor());

        const char* help = __is3D
            ? "3D: left select/edit | right/middle rotate | wheel zoom | WASD pan | R reset"
            : "2D: wheel zoom | middle drag pan | left paint | right erase | Shift start | Ctrl goal";
        float helpW = __MeasureText(help, 13);
        __Text(help, static_cast<float>(GetScreenWidth()) - helpW - __S(20.0f), __bPanel.y + __S(55.0f), 13, __MutedColor());
    }

    // Draw the 2D grid map, cur alg frm and path
    void App::__DrawMap() {
        DrawRectangleRec(__canvas, __CanvasColor());
        DrawRectangleLinesEx(__canvas, 1.0f, __GridColor());

        const float padding = __S(28.0f);
        const float usableW = __canvas.width - 2.0f * padding;
        const float usableH = __canvas.height - 2.0f * padding;
        const float baseCellSize = std::floor(std::min(usableW / static_cast<float>(__map.Cols()), usableH / static_cast<float>(__map.Rows())));
        const float cellSize = std::max(3.0f, baseCellSize * __zoom2D);
        if (cellSize <= 0.0f) return;

        const float gridW = cellSize * static_cast<float>(__map.Cols());
        const float gridH = cellSize * static_cast<float>(__map.Rows());
        const float startX = __canvas.x + (__canvas.width - gridW) / 2.0f + __pan2D.x;
        const float startY = __canvas.y + (__canvas.height - gridH) / 2.0f + __pan2D.y;

        const SearchFrm* frm = __CurFrame();

        BeginScissorMode(static_cast<int>(__canvas.x), static_cast<int>(__canvas.y), static_cast<int>(__canvas.width), static_cast<int>(__canvas.height));
        for (int r = 0; r < __map.Rows(); ++r) {
            for (int c = 0; c < __map.Cols(); ++c) {
                int idx = __map.Index(r, c);
                Rectangle rect{ startX + static_cast<float>(c) * cellSize, startY + static_cast<float>(r) * cellSize, cellSize, cellSize };
                if (rect.x + rect.width < __canvas.x || rect.y + rect.height < __canvas.y || rect.x > __canvas.x + __canvas.width || rect.y > __canvas.y + __canvas.height) continue;
                DrawRectangleRec(rect, __CellColor(idx, frm));
                if (cellSize >= 8.0f) DrawRectangleLinesEx(rect, 1.0f, __GridColor());

                const Cell& cell = __map.GetCell(idx);
                if (cellSize >= 24.0f && !__map.IsObstacle(idx)) {
                    const std::string label = std::to_string(cell.height);
                    const float labelSize = std::clamp((cellSize * 0.34f) / __scale, 13.0f, 26.0f);
                    const float tw = __MeasureText(label, labelSize);
                    __TextBold(label, rect.x + (cellSize - tw) * 0.5f, rect.y + cellSize * 0.32f, labelSize, __MutedColor());
                }
            }
        }

        if (frm != nullptr && frm->path.size() >= 2) {
            auto centerOf = [&](int idx) {
                GPos gc = __map.Coord(idx);
                return Vector2{ startX + (static_cast<float>(gc.col) + 0.5f) * cellSize,
                                startY + (static_cast<float>(gc.row) + 0.5f) * cellSize };
                };
            std::vector<Vector2> points;
            points.reserve(frm->path.size());
            for (int cell : frm->path) points.push_back(centerOf(cell));

            const float width = std::max(3.0f, cellSize * 0.16f);
            const Color core = __theme == Theme::Dark ? Pal::Path2CoreD : Pal::Path2CoreL;
            const Color outline = __theme == Theme::Dark ? Pal::Path2OutD : Pal::Path2OutL;
            if (__smoothPath && frm->path.size() >= 3) {
                points = RoundLine2(points, std::clamp(cellSize * 0.46f, 7.0f, 28.0f), 10);
            }
            DrawPath2D(points, width, outline, core);
        }

        __DrawAgent2D(startX, startY, cellSize);

        const int hovered = __MouseCell(GetMousePosition());
        if (hovered >= 0) {
            GPos hc = __map.Coord(hovered);
            Rectangle rect{ startX + static_cast<float>(hc.col) * cellSize, startY + static_cast<float>(hc.row) * cellSize, cellSize, cellSize };
            DrawRectangleLinesEx(rect, 2.5f, __OverlayColor());
        }
        EndScissorMode();

        const int hoverCell = __MouseCell(GetMousePosition());
        if (hoverCell >= 0) {
            GPos hc = __map.Coord(hoverCell);
            const Cell& cell = __map.GetCell(hoverCell);
            std::string tip = "cell " + std::to_string(hc.row) + "," + std::to_string(hc.col)
                + " | " + TerrainName(cell.terrain) + " | h=" + std::to_string(cell.height)
                + " | zoom=" + FormatDouble(__zoom2D, 2) + "x";
            __Text(tip, __canvas.x + __S(22.0f), __canvas.y + __S(22.0f), 17, __TextColor());
        }
        else {
            __Text("2D map editor | wheel zoom, middle drag pan", __canvas.x + __S(22.0f), __canvas.y + __S(22.0f), 18, __TextColor());
        }
    }

    // Draw all agent comparison paths in 2D
    void App::__DrawAgent2D(float startX, float startY, float cellSize) const {
        if (!__showAgents || __agentRows.empty()) return;
        auto centerOf = [&](int idx) {
            GPos gc = __map.Coord(idx);
            return Vector2{ startX + (static_cast<float>(gc.col) + 0.5f) * cellSize,
                            startY + (static_cast<float>(gc.row) + 0.5f) * cellSize };
            };
        const float width = std::max(2.5f, cellSize * 0.10f);
        const Color outline = __theme == Theme::Dark ? Pal::Agent2OutD : Pal::Agent2OutL;
        for (const auto& row : __agentRows) {
            if (!row.found || row.path.size() < 2) continue;
            std::vector<Vector2> points;
            points.reserve(row.path.size());
            for (int cell : row.path) points.push_back(centerOf(cell));
            if (__smoothPath && points.size() >= 3) points = RoundLine2(points, std::clamp(cellSize * 0.40f, 6.0f, 24.0f), 8);
            DrawPath2D(points, width, outline, ProfileColor(row.prof, __theme));
        }
    }

    // Draw all agent comparison paths in 3D
    void App::__DrawAgent3D() const {
        if (!__showAgents || __agentRows.empty()) return;
        for (const auto& row : __agentRows) {
            if (!row.found || row.path.size() < 2) continue;
            __DrawPath3DC(row.path, ProfileColor(row.prof, __theme), 0.046f);
        }
    }

    // Draw the full 3D scene
    // order matters, or the scene becomes abstract
    // AWFUL 3D!
    void App::__Draw3D() {
        BeginScissorMode(static_cast<int>(__canvas.x), static_cast<int>(__canvas.y), static_cast<int>(__canvas.width), static_cast<int>(__canvas.height));
        __DrawGradBg(__canvas);

        BeginMode3D(__camera);

        const float worldW = static_cast<float>(__map.Cols());
        const float worldH = static_cast<float>(__map.Rows());
        Color floorColor = __theme == Theme::Dark ? Pal::FloorD : Pal::FloorL;
        DrawPlane({ 0.0f, -0.035f, 0.0f }, { worldW + 3.0f, worldH + 3.0f }, floorColor);

        const SearchFrm* frm = __CurFrame();
        if (__smooth3D) {
            __DrawSmooth3D(frm);
        }
        else {
            __DrawBlock3D(frm);
        }

        if (!__smooth3D && __map.Count() <= 9000) {
            for (int idx = 0; idx < __map.Count(); ++idx) {
                __DrawFeat3D(idx);
            }
        }

        if (__showGraph) {
            __DrawGraph3D();
        }

        if (frm != nullptr && frm->path.size() >= 2) {
            __DrawPath3D(frm->path);
        }
        __DrawAgent3D();

        DrawSphere(__CellPos(__map.Start(), 0.65f), 0.32f, __SuccessColor());
        DrawSphere(__CellPos(__map.Goal(), 0.65f), 0.32f, __DangerColor());

        if (frm != nullptr && frm->cur >= 0) {
            DrawSphere(__CellPos(frm->cur, 0.92f), 0.26f, __AccentColor());
        }

        if (__selCell >= 0) {
            __DrawSel3D(__selCell, __AccentColor());
        }
        if (__hov3D >= 0 && __hov3D != __selCell) {
            __DrawSel3D(__hov3D, __OverlayColor());
        }

        if (!__smooth3D) {
            DrawGrid(std::max(__map.Rows(), __map.Cols()) + 4, 1.0f);
        }
        EndMode3D();
        EndScissorMode();

        DrawRectangleLinesEx(__canvas, 1.0f, __GridColor());
        __Text(__smooth3D ? "3D diorama presentation" : "3D editor mode", __canvas.x + __S(22.0f), __canvas.y + __S(22.0f), 19, __TextColor());
        __Text(__smooth3D ? "Diorama hides the dense grid and turns the map into a clean terrain sandbox." : "Editor mode keeps every cell visible for precise editing and picking.", __canvas.x + __S(22.0f), __canvas.y + __S(49.0f), 14, __MutedColor());
        __Text("Agent: " + __ProfName() + " | " + std::string(__showGraph ? "graph overlay ON" : "graph overlay OFF"), __canvas.x + __S(22.0f), __canvas.y + __S(75.0f), 15, __MutedColor());

        if (__hov3D >= 0) {
            __Text("Hover: " + __CellInfo(__hov3D), __canvas.x + __S(22.0f), __canvas.y + __S(101.0f), 15, __TextColor());
        }
        if (__selCell >= 0) {
            __Text("Selected: " + __CellInfo(__selCell), __canvas.x + __S(22.0f), __canvas.y + __S(126.0f), 15, __AccentColor());
        }
    }

    // Draw the precise cell-block terrain for editor mode
    void App::__DrawBlock3D(const SearchFrm* frm) const {
        const float cellScale = 1.0f;
        for (int r = 0; r < __map.Rows(); ++r) {
            for (int c = 0; c < __map.Cols(); ++c) {
                const int idx       = __map.Index(r, c);
                const Cell& cell    = __map.GetCell(idx);
                const bool blocked  = __map.IsObstacle(idx) || __map.IsWater(idx);

                const float h       = __CellHgt(idx);
                Vector3 center      = __CellPos(idx, 0.0f);
                center.y            = h * 0.5f;
                Vector3 size{ cellScale * 0.92f, h, cellScale * 0.92f };

                Color color = __CellColor(idx, frm);
                if (cell.terrain == Terrain::Water) color = Blend(color, __theme == Theme::Dark ? Pal::BlockWaterD : Pal::BlockWaterL, 0.25f);
                DrawCubeShade(center, size, color);

                if (blocked || idx == __map.Start() || idx == __map.Goal()) {
                    DrawCubeWiresV(center, size, __theme == Theme::Dark ? Pal::BlockWireD : Pal::BlockWireL);
                }
            }
        }
    }

    // Draw the cleaner diorama terrain
    void App::__DrawSmooth3D(const SearchFrm* frm) const {

        const float worldW    = static_cast<float>(__map.Cols());
        const float worldH    = static_cast<float>(__map.Rows());
        const Color baseTop   = __theme == Theme::Dark ? Pal::DioBaseTopD : Pal::DioBaseTopL;
        const Color baseSide  = __theme == Theme::Dark ? Pal::DioBaseSideD : Pal::DioBaseSideL;
        const Color boardTrim = __theme == Theme::Dark ? Pal::DioTrimD : Pal::DioTrimL;

        DrawCubeShade({ 0.0f, -0.18f, 0.0f }, { worldW + 1.2f, 0.30f, worldH + 1.2f }, baseSide);
        DrawCubeFlat({ 0.0f, 0.015f, 0.0f }, { worldW + 0.95f, 0.035f, worldH + 0.95f }, baseTop);
        DrawCubeWiresV({ 0.0f, 0.015f, 0.0f }, { worldW + 0.95f, 0.040f, worldH + 0.95f }, boardTrim);

        auto pFor = [&](int idx, float lift = 0.06f) {
            return __CellPos(idx, lift);
            };

        auto isFrameCell = [&](int idx, const std::vector<int>& list) {
            return frm != nullptr && __HasCell(list, idx);
            };

        auto overlayFor = [&](int idx) -> Color {
            if (frm == nullptr)              return Pal::Transparent;
            if (__HasCell(frm->path, idx))   return __theme == Theme::Dark ? Pal::DioPathD : Pal::DioPathL;
            if (frm->cur == idx)             return __theme == Theme::Dark ? Pal::DioCurD : Pal::DioCurL;
            if (__HasCell(frm->open, idx))   return __theme == Theme::Dark ? Pal::DioOpenD : Pal::DioOpenL;
            if (__HasCell(frm->closed, idx)) return __theme == Theme::Dark ? Pal::DioClosedD : Pal::DioClosedL;
            return Pal::Transparent;
            };

        for (int idx = 0; idx < __map.Count(); ++idx) {
            const Cell& cell = __map.GetCell(idx);
            Vector3 p = pFor(idx, 0.052f);

            if (cell.terrain == Terrain::Water) {
                Color water = __theme == Theme::Dark ? Pal::DioWaterD : Pal::DioWaterL;
                DrawCubeFlat({ p.x, 0.075f, p.z }, { 0.96f, 0.030f, 0.96f }, water);
                continue;
            }

            if (cell.terrain == Terrain::Road) {
                Color road = __theme == Theme::Dark ? Pal::DioRoadD : Pal::DioRoadL;
                Color stripe = __theme == Theme::Dark ? Pal::DioStripeD : Pal::DioStripeL;
                DrawCubeFlat({ p.x, 0.125f, p.z }, { 0.94f, 0.045f, 0.94f }, road);

                GPos g = __map.Coord(idx);
                bool horizontal = false;
                bool vertical = false;
                if (g.col > 0 && __map.GetCell(__map.Index(g.row, g.col - 1)).terrain == Terrain::Road)                 horizontal = true;
                if (g.col + 1 < __map.Cols() && __map.GetCell(__map.Index(g.row, g.col + 1)).terrain == Terrain::Road)  horizontal = true;
                if (g.row > 0 && __map.GetCell(__map.Index(g.row - 1, g.col)).terrain == Terrain::Road)                 vertical = true;
                if (g.row + 1 < __map.Rows() && __map.GetCell(__map.Index(g.row + 1, g.col)).terrain == Terrain::Road)  vertical = true;
                if      (horizontal && !vertical)   DrawCubeFlat({ p.x, 0.154f, p.z }, { 0.52f, 0.012f, 0.055f }, stripe);
                else if (vertical && !horizontal)   DrawCubeFlat({ p.x, 0.154f, p.z }, { 0.055f, 0.012f, 0.52f }, stripe);
                else                                DrawCubeFlat({ p.x, 0.154f, p.z }, { 0.20f, 0.012f, 0.20f }, stripe);
                continue;
            }

            if (cell.terrain == Terrain::Forest) {
                GPos g = __map.Coord(idx);
                if (((idx * 31 + g.row * 17 + g.col * 13) % 11) == 0) {
                    Color trunk  = __theme == Theme::Dark ? Pal::DioTrunkD  : Pal::DioTrunkL;
                    Color canopy = __theme == Theme::Dark ? Pal::DioCanopyD : Pal::DioCanopyL;
                    DrawCylinder({ p.x, 0.205f, p.z }, 0.045f, 0.045f, 0.23f, 7, trunk);
                    DrawCylinder({ p.x, 0.435f, p.z }, 0.02f, 0.21f, 0.38f, 8, canopy);
                }
                continue;
            }

            if (cell.terrain == Terrain::Mountain) {
                GPos g = __map.Coord(idx);
                if (((idx * 23 + g.row * 11 + g.col * 19) % 13) == 0) {
                    Color rock  = __theme == Theme::Dark ? Pal::DioRockD : Pal::DioRockL;
                    Color cap   = __theme == Theme::Dark ? Pal::DioSnowD : Pal::DioSnowL;
                    float rockH = 0.42f + static_cast<float>(cell.height) * 0.035f;
                    DrawCylinder({ p.x, 0.18f + rockH * 0.50f, p.z }, 0.28f, 0.04f, rockH, 6, rock);
                    DrawSphere({ p.x + 0.08f, 0.18f + rockH * 0.80f, p.z - 0.07f }, 0.09f, cap);
                }
                continue;
            }
        }

        for (int idx = 0; idx < __map.Count(); ++idx) {
            if (!__map.IsObstacle(idx) || __map.IsWater(idx)) continue;
            const Cell& cell     = __map.GetCell(idx);
            GPos        g        = __map.Coord(idx);
            Vector3     p        = pFor(idx, 0.0f);
            float       h        = 0.58f + 0.055f * static_cast<float>((cell.height + g.row + g.col) % 5);
            Color       building = __theme == Theme::Dark ? Pal::DioBuildD : Pal::DioBuildL;
            Color       roof     = __theme == Theme::Dark ? Pal::DioRoofD  : Pal::DioRoofL;
            DrawCubeShade({ p.x, h * 0.5f + 0.08f, p.z }, { 0.70f, h, 0.70f }, building);
            DrawCubeFlat({ p.x, h + 0.106f, p.z }, { 0.74f, 0.035f, 0.74f }, roof);
        }

        if (frm != nullptr) {
            const int stride = std::max(1, __map.Count() / 2400);
            for (int idx = 0; idx < __map.Count(); idx += stride) {
                Color ov = overlayFor(idx);
                if (ov.a == 0) continue;
                Vector3 p = pFor(idx, 0.16f);
                DrawCubeFlat({ p.x, p.y, p.z }, { 0.82f, 0.026f, 0.82f }, ov);
            }
        }
    }

    // Draw the active final path in 3D using the default path color
    void App::__DrawPath3D(const std::vector<int>& path) const {
        const Color core = __theme == Theme::Dark ? Pal::Path3CoreD : Pal::Path3CoreL;
        __DrawPath3DC(path, core, 0.065f);
    }

    // Draw one 3D path with a chosen color
    // used by normal path and agent comparison
    void App::__DrawPath3DC(const std::vector<int>& path, Color core, float radius) const {
        if (path.size() < 2) return;

        std::vector<Vector3> points;
        points.reserve(path.size());
        for (int cell : path) points.push_back(__CellPos(cell, 0.66f));

        if (__smoothPath && path.size() >= 3) {
            points = RoundLine3(points, 0.46f, 10);
        }

        const Color outline = __theme == Theme::Dark ? Pal::Path3OutD : Pal::Path3OutL;
        DrawTube3D(points, radius, outline, core);
    }

    // Draw a sparse graph overlay
    // so the search space is visible but not too noisy
    void App::__DrawGraph3D() const {
        const int stride = std::max(1, std::max(__map.Rows(), __map.Cols()) / 45);
        Color edgeColor = __theme == Theme::Dark ? Pal::GraphEdgeD : Pal::GraphEdgeL;
        Color nodeColor = __theme == Theme::Dark ? Pal::GraphNodeD : Pal::GraphNodeL;
        for (int r = 0; r < __map.Rows(); r += stride) {
            for (int c = 0; c < __map.Cols(); c += stride) {
                int idx = __map.Index(r, c);
                if (!__map.IsWalkable(idx, __prof)) continue;
                Vector3 p = __CellPos(idx, 0.10f);
                DrawSphere(p, 0.045f, nodeColor);
                if (c + stride < __map.Cols()) {
                    int right = __map.Index(r, c + stride);
                    if (__map.IsWalkable(right, __prof)) DrawLine3D(p, __CellPos(right, 0.10f), edgeColor);
                }
                if (r + stride < __map.Rows()) {
                    int down = __map.Index(r + stride, c);
                    if (__map.IsWalkable(down, __prof)) DrawLine3D(p, __CellPos(down, 0.10f), edgeColor);
                }
            }
        }
    }

    // Draw a floating outline above one selected or hovered 3D cell
    void App::__DrawSel3D(int cell, Color color) const {
        if (cell < 0 || cell >= __map.Count()) return;
        BoundingBox box = __CellBox3D(cell);
        const float lift = 0.035f;
        box.min.x -= 0.035f;
        box.min.z -= 0.035f;
        box.max.x += 0.035f;
        box.max.z += 0.035f;
        box.min.y =  box.max.y + lift;
        box.max.y =  box.min.y + 0.045f;
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
        DrawCubeFlat(center, { size.x, 0.012f, size.z }, WithAlpha(color, Pal::SelFillA));
    }

    // Draw small road/tree/rock/water details on block terrain
    // yeah that's very cute~ >3<
    // please look at [NOI 2016] The lemon tree in the moonlight
    void App::__DrawFeat3D(int cell) const {
        if (cell < 0 || cell >= __map.Count()) return;
        const Cell& c = __map.GetCell(cell);
        if (__map.IsObstacle(cell) && c.terrain != Terrain::Water) return;

        GPos gc = __map.Coord(cell);
        const int featureHash = (gc.row * 73856093) ^ (gc.col * 19349663);
        if (c.terrain == Terrain::Forest    && (std::abs(featureHash) % (__smooth3D ? 11 : 7)) != 0) return;
        if (c.terrain == Terrain::Mountain  && (std::abs(featureHash) % (__smooth3D ? 13 : 8)) != 0) return;
        if (c.terrain == Terrain::Road      && (std::abs(featureHash) % (__smooth3D ? 6 : 3))  != 0) return;

        Vector3 p = __CellPos(cell, 0.02f);

        switch (c.terrain) {
        case Terrain::Road: {
            Color slab      = __theme == Theme::Dark ? Pal::FeatRoadD   : Pal::FeatRoadL;
            Color stripe    = __theme == Theme::Dark ? Pal::FeatStripeD : Pal::FeatStripeL;
            DrawCubeFlat({ p.x, p.y + 0.010f, p.z }, { 0.62f, 0.028f, 0.62f },  slab);
            DrawCubeFlat({ p.x, p.y + 0.026f, p.z }, { 0.065f, 0.014f, 0.46f }, stripe);
            break;
        }
        case Terrain::Forest: {
            Color trunk     = __theme == Theme::Dark ? Pal::FeatTrunkD  : Pal::FeatTrunkL;
            Color canopy    = __theme == Theme::Dark ? Pal::FeatCanopyD : Pal::FeatCanopyL;
            DrawCylinder({ p.x, p.y + 0.085f, p.z }, 0.045f, 0.045f, 0.18f, 7, trunk);
            DrawCylinder({ p.x, p.y + 0.30f, p.z }, 0.0f, 0.18f, 0.34f, 8, canopy);
            break;
        }
        case Terrain::Mountain: {
            Color peak  = __theme == Theme::Dark ? Pal::FeatPeakD  : Pal::FeatPeakL;
            Color stone = __theme == Theme::Dark ? Pal::FeatStoneD : Pal::FeatStoneL;
            DrawCylinder({ p.x, p.y + 0.18f, p.z }, 0.0f, 0.23f, 0.38f, 6, peak);
            DrawSphere({ p.x + 0.08f, p.y + 0.12f, p.z - 0.08f }, 0.075f, stone);
            break;
        }
        case Terrain::Water: {
            Color water = __theme == Theme::Dark ? Pal::FeatWaterD : Pal::FeatWaterL;
            DrawCubeFlat({ p.x, p.y + 0.028f, p.z }, { 0.82f, 0.045f, 0.82f }, water);
            break;
        }
        case Terrain::Plain:
        default:
            break;
        }
    }

    // Convert mouse actions into map edits
    // this is where most editing bugs like to hide
    // FUCK YOU!
    void App::__HandleEdit() {
        Vector2 mouse = GetMousePosition();

        if (__is3D) {
            __hov3D = CheckCollisionPointRec(mouse, __canvas) ? __Pick3D(mouse) : -1;
            if (__hov3D < 0) return;

            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                __selCell = __hov3D;
                if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
                    if (__lastCell != __hov3D) __PushUndo("Set start");
                    __lastCell = __hov3D;
                    __map.SetStart(__hov3D);
                    __ResetVis();
                    return;
                }
                if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
                    if (__lastCell != __hov3D) __PushUndo("Set goal");
                    __lastCell = __hov3D;
                    __map.SetGoal(__hov3D);
                    __ResetVis();
                    return;
                }
                if (__lastCell != __hov3D) {
                    __PushUndo("Edit cell");
                    __lastCell = __hov3D;
                    __EditCell(__hov3D);
                }
            }
            return;
        }

        __hov3D = -1;
        if (!CheckCollisionPointRec(mouse, __canvas)) return;

        int cell = __MouseCell(mouse);
        if (cell < 0) return;

        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            __selCell = cell;
            if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
                if (__lastCell != cell) __PushUndo("Set start");
                __lastCell = cell;
                __map.SetStart(cell);
                __ResetVis();
                return;
            }
            if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
                if (__lastCell != cell) __PushUndo("Set goal");
                __lastCell = cell;
                __map.SetGoal(cell);
                __ResetVis();
                return;
            }
            if (__lastCell != cell) {
                __PushUndo("Edit cell");
                __lastCell = cell;
                __EditCell(cell);
            }
        }

        if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
            __selCell = cell;
            if (__lastCell != cell) {
                __PushUndo("Erase cell");
                __lastCell = cell;
                __map.SetObstacle(cell, false);
                if (__map.GetCell(cell).terrain == Terrain::Water) __map.SetTerrain(cell, Terrain::Plain);
                __ResetVis();
            }
        }
    }

    // Apply the cur edit tool to one cell
    void App::__EditCell(int cell) {
        if (cell < 0 || cell >= __map.Count()) return;

        switch (__edit) {
        case Edit::Select:
            __status = "Selected " + __CellInfo(cell);
            return;
        case Edit::Wall:
            __map.SetObstacle(cell, true);
            break;
        case Edit::Erase:
            __map.SetObstacle(cell, false);
            if (__map.GetCell(cell).terrain == Terrain::Water) __map.SetTerrain(cell, Terrain::Plain);
            break;
        case Edit::Start:
            __map.SetStart(cell);
            break;
        case Edit::Goal:
            __map.SetGoal(cell);
            break;
        case Edit::Raise:
            __map.AddHeight(cell, 1);
            break;
        case Edit::Lower:
            __map.AddHeight(cell, -1);
            break;
        case Edit::Plain:
            __map.SetObstacle(cell, false);
            __map.SetTerrain(cell, Terrain::Plain);
            break;
        case Edit::Road:
            __map.SetObstacle(cell, false);
            __map.SetTerrain(cell, Terrain::Road);
            break;
        case Edit::Forest:
            __map.SetObstacle(cell, false);
            __map.SetTerrain(cell, Terrain::Forest);
            break;
        case Edit::Mountain:
            __map.SetObstacle(cell, false);
            __map.SetTerrain(cell, Terrain::Mountain);
            break;
        case Edit::Water:
            __map.SetObstacle(cell, false);
            __map.SetTerrain(cell, Terrain::Water);
            break;
        }
        __ResetVis();
    }

    // Generate a random mixed map and reset all visualization state
    void App::__GenRandom() {
        __map.GenerateRandom(__obsProb);
        __selCell       = -1;
        __hov3D         = -1;
        __ClearHist();
        __showAgents    = false;
        __agentRows.clear();
        __ResetVis();
        __status        = "Random map generated";
    }

    // Generate a procedural terrain map with roads, water, forest and mountains
    void App::__GenTerrain() {
        __map.GenerateTerrain();
        __selCell       = -1;
        __hov3D         = -1;
        __ClearHist();
        __showAgents    = false;
        __agentRows.clear();
        __ResetVis();
        __status        = "Terrain map generated: hills, road, river, forest and mountains";
    }

    // Generate a maze map
    // good for showing BFS and A* differences
    void App::__GenMaze() {
        __map.GenerateMaze();
        __selCell       = -1;
        __hov3D         = -1;
        __ClearHist();
        __showAgents    = false;
        __agentRows.clear();
        __ResetVis();
        __status        = "Maze map generated";
    }

    // Build a city-style preset with roads, blocks and random building heights.
    void App::__GenCity() {
        __map.ClearAll();
        std::mt19937 rng(time(NULL));
        std::uniform_int_distribution<int> heightDist(0, 4);

        for (int r = 0; r < __map.Rows(); ++r) {
            for (int c = 0; c < __map.Cols(); ++c) {
                int idx = __map.Index(r, c);
                __map.SetHeight(idx, heightDist(rng));
                if (r % 6 == 2 || c % 9 == 4) {
                    __map.SetTerrain(idx, Terrain::Road);
                    __map.SetObstacle(idx, false);
                    __map.SetHeight(idx, 1);
                }
                else if (((r / 3 + c / 4) % 5 == 0) && (r % 6 != 0) && (c % 9 != 0)) {
                    __map.SetObstacle(idx, true);
                    __map.SetTerrain(idx, Terrain::Plain);
                    __map.SetHeight(idx, 2 + ((r + c) % 5));
                }
                else if ((r + c * 2) % 19 == 0) {
                    __map.SetTerrain(idx, Terrain::Forest);
                }
            }
        }
        __map.SetStart(__map.Index(std::min(2, __map.Rows() - 1), std::min(2, __map.Cols() - 1)));
        __map.SetGoal (__map.Index(std::max(0, __map.Rows() - 3), std::max(0, __map.Cols() - 3)));
        __selCell       = -1;
        __hov3D         = -1;
        __ClearHist();
        __showAgents    = false;
        __agentRows.clear();
        __ResetVis();
        __ResetCam();
        __status        = "City preset generated: roads, buildings and blocks";
    }

    // Build a river-crossing preset with bridges and route choices
    void App::__GenRiver() {
        __map.ClearAll();
        const int rows = __map.Rows();
        const int cols = __map.Cols();
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                int idx = __map.Index(r, c);
                __map.SetHeight(idx, std::clamp(4 + static_cast<int>(2.2 * std::sin(c * 0.23) + 1.4 * std::cos(r * 0.31)), 0, 9));
                if ((r + c) % 17 == 0) __map.SetTerrain(idx, Terrain::Forest);
            }
        }
        for (int c = 0; c < cols; ++c) {
            int center = rows / 2 + static_cast<int>(std::sin(c * 0.22f) * rows * 0.13f);
            for (int dr = -2; dr <= 2; ++dr) {
                int r = center + dr;
                if (!__map.InBounds(r, c)) continue;
                int idx = __map.Index(r, c);
                __map.SetObstacle(idx, false);
                __map.SetTerrain(idx, Terrain::Water);
                __map.SetHeight(idx, 0);
            }
        }
        const int bridges[] = { cols / 5, cols / 2, cols * 4 / 5 };
        for (int bridgeCol : bridges) {
            for (int c = std::max(0, bridgeCol - 1); c <= std::min(cols - 1, bridgeCol + 1); ++c) {
                for (int r = 0; r < rows; ++r) {
                    int idx = __map.Index(r, c);
                    __map.SetTerrain(idx, Terrain::Road);
                    __map.SetObstacle(idx, false);
                    __map.SetHeight(idx, 1);
                }
            }
        }
        __map.SetStart(__map.Index(rows / 2, 1));
        __map.SetGoal(__map.Index(rows / 2, cols - 2));
        __selCell       = -1;
        __hov3D         = -1;
        __ClearHist();
        __showAgents    = false;
        __agentRows.clear();
        __ResetVis();
        __ResetCam();
        __status        = "River crossing preset generated: bridges create route choices";
    }

    // Build a mountain-pass preset where height cost finally matters
    void App::__GenMountain() {
        __map.ClearAll();
        const int rows = __map.Rows();
        const int cols = __map.Cols();
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                int idx = __map.Index(r, c);
                float ridge = std::abs(static_cast<float>(r) - rows * 0.5f - std::sin(c * 0.20f) * rows * 0.12f);
                int h = std::clamp(8 - static_cast<int>(ridge * 0.7f), 0, 9);
                __map.SetHeight(idx, h);
                if (h >= 5) __map.SetTerrain(idx, Terrain::Mountain);
                if (h >= 8 && (c < cols / 3 || c > cols * 2 / 3)) __map.SetObstacle(idx, true);
                if ((r + c) % 21 == 0) __map.SetTerrain(idx, Terrain::Forest);
            }
        }
        for (int passCol : { cols / 3, cols * 2 / 3 }) {
            for (int c = std::max(0, passCol - 2); c <= std::min(cols - 1, passCol + 2); ++c) {
                for (int r = rows / 2 - 4; r <= rows / 2 + 4; ++r) {
                    if (!__map.InBounds(r, c)) continue;
                    int idx = __map.Index(r, c);
                    __map.SetObstacle(idx, false);
                    __map.SetTerrain(idx, Terrain::Road);
                    __map.SetHeight(idx, 2);
                }
            }
        }
        __map.SetStart(__map.Index(rows - 3, 2));
        __map.SetGoal(__map.Index(2, cols - 3));
        __selCell       = -1;
        __hov3D         = -1;
        __ClearHist();
        __showAgents    = false;
        __agentRows.clear();
        __ResetVis();
        __ResetCam();
        __status        = "Mountain pass preset generated: height cost matters";
    }

    // Set up a pretty demo scene and run A* automatically
    // nyeah it's a constant demo caz I don't want to add more things
    void App::__RunDemo() {
        __GenRiver();
        __is3D      = true;
        __smooth3D  = true;
        __prof      = MoveProfile::Pedestrian;
        __opt.diag  = true;
        __heurVal   = 1.25f;
        __terrVal   = 1.0f;
        __hgtVal    = 0.35f;
        __ResetCam();
        __RunAlg(AlgKind::AStar);
        __frmIdx    = 0;
        __play      = true;
        __status    = "Demo mode: River preset + Diorama + A* animation";
    }

    // Resize the map from sliders and reset camera/view state
    void App::__ApplyMapSize() {
        int rows = static_cast<int>(std::round(__rowVal));
        int cols = static_cast<int>(std::round(__colVal));
        __PushUndo("Resize map");
        __map.Resize(rows, cols);
        __selCell       = -1;
        __hov3D         = -1;
        __ResetVis();
        __ResetCam();
        __pan2D         = { 0.0f, 0.0f };
        __zoom2D        = 1.0f;
        __status        = "Map size changed to " + std::to_string(__map.Rows()) + " x " + std::to_string(__map.Cols());
    }

    // Run the selected pathfinding alg and start animation playback
    void App::__RunAlg(AlgKind Type) {
        __SyncOpt();
        __result = Pathfinder::Run(__map, Type, __opt);
        __hasRes = true;
        __frmIdx = 0;
        __play   = true;
        __acc    = 0.0f;
        __status = "Running " + __result.algName + " as " + __ProfName();
    }

    // Run all algorithms once and store their comparison data
    void App::__RunBench() {
        __SyncOpt();
        __benchRows.clear();
        const std::array<AlgKind, 5> algorithms{
            AlgKind::BFS,
            AlgKind::Dijkstra,
            AlgKind::AStar,
            AlgKind::WAStar,
            AlgKind::Greedy
        };

        for (AlgKind alg : algorithms) {
            SearchRes r = Pathfinder::Run(__map, alg, __opt);
            __benchRows.push_back({ r.algName, r.found, r.pathLen, r.expNodes, r.genNodes, r.timeMs });
        }
        __status = "Benchmark finished for " + std::to_string(__benchRows.size()) + " algorithms as " + __ProfName();
    }

    // Run A* for every movement prof on the same map
    void App::__CmpAgents() {
        __SyncOpt();
        __agentRows.clear();
        const std::array<MoveProfile, 4> profiles{
            MoveProfile::Pedestrian,
            MoveProfile::Car,
            MoveProfile::Offroad,
            MoveProfile::Drone
        };
        for (MoveProfile prof : profiles) {
            AlgOpt opts = __opt;
            opts.prof = prof;
            SearchRes r = Pathfinder::Run(__map, AlgKind::AStar, opts);
            __agentRows.push_back({ prof, ProfNameOf(prof), r.found, r.pathLen, r.expNodes, r.timeMs, r.finalPath });
        }
        __showAgents = true;
        __status = "Agent path comparison finished: same map, different cost models";
    }

    // Export the cur window as a PNG screenshot
    void App::__ExportPng() {
        std::string path = FixPngExt(PngDlg());
        if (path.empty()) {
            __status = "Screenshot export canceled";
            return;
        }
        try {
            std::filesystem::path p(path);
            if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
            TakeScreenshot(path.c_str());
            __status = "Screenshot exported: " + CompactPath(path);
        }
        catch (...) {
            __status = "Failed to export screenshot";
        }
    }

    // Save the cur map to a .pathmap file
    void App::__SaveMap() {
        std::string path = FixMapExt(MapDlg(true));
        if (path.empty()) {
            __status = "Save canceled";
            return;
        }

        try {
            std::filesystem::path p(path);
            if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
        }
        catch (...) {
            __status = "Failed to prepare target folder";
            return;
        }

        if (__map.SaveToFile(path)) {
            __curMapPath = path;
            __status = "Map saved: " + CompactPath(path);
        }
        else {
            __status = "Failed to save: " + CompactPath(path);
        }
    }

    // Load a .pathmap file and sync UI sliders with the loaded size
    void App::__LoadMap() {
        std::string path = MapDlg(false);
        if (path.empty()) {
            __status = "Open canceled";
            return;
        }

        std::string error;
        if (__map.LoadFromFile(path, &error)) {
            __curMapPath    = path;
            __rowVal        = static_cast<float>(__map.Rows());
            __colVal        = static_cast<float>(__map.Cols());
            __selCell       = -1;
            __hov3D         = -1;
            __ResetVis();
            __ResetCam();
            __status        = "Map loaded: " + CompactPath(path);
        }
        else {
            __status = error.empty() ? ("Failed to open: " + CompactPath(path)) : error;
        }
    }

    // Export benchmark rows as CSV
    void App::__ExportCsv() {
        try {
            std::filesystem::create_directories("exports");
            std::ofstream out("exports/benchmark.csv");
            if (!out) {
                __status = "Failed to create exports/benchmark.csv";
                return;
            }
            out << "alg,found,path_cost,expanded_nodes,generated_nodes,time_ms\n";
            for (const auto& row : __benchRows) {
                out << row.algName << ',' << (row.found ? "true" : "false") << ','
                    << row.pathLen << ',' << row.expNodes << ',' << row.genNodes << ',' << row.timeMs << '\n';
            }
            __status = "Benchmark exported to exports/benchmark.csv";
        }
        catch (...) {
            __status = "Failed to export benchmark CSV";
        }
    }

    // Push cur map into undo stack before an edit
    void App::__PushUndo(const std::string& label) {
        __undo.push_back(MapSnap{ __map, label });
        if (__undo.size() > 80) __undo.erase(__undo.begin());
        __redo.clear();
    }

    // Clear undo/redo stacks after generating or loading a new map
    void App::__ClearHist() {
        __undo.clear();
        __redo.clear();
        __lastCell = -1;
    }

    // Restore the last map state from undo stack
    void App::__UndoEdit() {
        if (__undo.empty()) {
            __status = "Nothing to undo";
            return;
        }
        __redo.push_back(MapSnap{ __map, "redo" });
        MapSnap state = __undo.back();
        __undo.pop_back();
        __map           = state.map;
        __rowVal        = static_cast<float>(__map.Rows());
        __colVal        = static_cast<float>(__map.Cols());
        __selCell       = -1;
        __hov3D         = -1;
        __showAgents    = false;
        __agentRows.clear();
        __ResetVis();
        __status        = "Undo: " + state.label;
    }

    // Restore a state from redo stack, because regret can have regret too
    void App::__RedoEdit() {
        if (__redo.empty()) {
            __status = "Nothing to redo";
            return;
        }
        __undo.push_back(MapSnap{ __map, "undo" });
        MapSnap state = __redo.back();
        __redo.pop_back();
        __map           = state.map;
        __rowVal        = static_cast<float>(__map.Rows());
        __colVal        = static_cast<float>(__map.Cols());
        __selCell       = -1;
        __hov3D         = -1;
        __showAgents    = false;
        __agentRows.clear();
        __ResetVis();
        __status        = "Redo";
    }

    // Clear alg result and animation state after map changes
    void App::__ResetVis() {
        __hasRes     = false;
        __result     = SearchRes{};
        __frmIdx     = 0;
        __play       = false;
        __acc        = 0.0f;
        __showAgents = false;
        __agentRows.clear();
    }

    // Copy UI slider values into alg opt
    void App::__SyncOpt() {
        __opt.hW    = static_cast<double>(__heurVal);
        __opt.terrW = static_cast<double>(__terrVal);
        __opt.hgtW  = static_cast<double>(__hgtVal);
        __opt.prof  = __prof;
    }

    // Load a better UI font if the file exists
    // otherwise use raylib default
    void App::__LoadFont() {
        for (const char* path : FontCandidates()) {
            if (!FileExists(path)) continue;
            Font loaded = LoadFontEx(path, 192, nullptr, 0);
            if (loaded.texture.id != 0) {
                __font = loaded;
                SetTextureFilter(__font.texture, TEXTURE_FILTER_BILINEAR);
                __fontLoaded = true;
                __fontStat = path;
                return;
            }
        }
        __font = GetFontDefault();
        SetTextureFilter(__font.texture, TEXTURE_FILTER_BILINEAR);
        __fontLoaded = false;
        __fontStat   = "default raylib font; add assets/fonts/ui.ttf to override";
    }

    // Unload custom font before closing the window
    void App::__UnloadFont() {
        if (__fontLoaded) {
            UnloadFont(__font);
            __fontLoaded = false;
        }
    }

    // Draw a normal button and return whether it was pressed
    bool App::__Button(Rectangle box, const char* text) {
        Vector2 mouse = GetMousePosition();
        bool hover   = CheckCollisionPointRec(mouse, box);
        bool pressed = hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        Color fill   = hover ? (__theme == Theme::Dark ? Pal::BtnHoverD : Pal::BtnHoverL) : (__theme == Theme::Dark ? Pal::BtnIdleD : Pal::BtnIdleL);
        if (pressed) fill = __AccentColor();

        DrawRectangleRounded(box, 0.18f, 8, fill);
        DrawRectangleRoundedLines(box, 0.18f, 8, 1.0f, __GridColor());

        float textWidth = __MeasureText(text, 17);
        __TextBold(text, box.x + (box.width - textWidth) / 2.0f, box.y + (box.height - __S(21.0f)) * 0.5f, 17, __TextColor());
        return pressed;
    }

    // Draw a toggle button and return whether the user clicked it
    bool App::__TogBtn(Rectangle box, const char* text, bool active) {
        Vector2 mouse = GetMousePosition();
        bool hover = CheckCollisionPointRec(mouse, box);
        bool pressed = hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        Color fill = active ? __AccentColor() : (hover ? (__theme == Theme::Dark ? Pal::BtnHoverD : Pal::BtnHoverL) : (__theme == Theme::Dark ? Pal::BtnIdleD : Pal::BtnIdleL));

        DrawRectangleRounded(box, 0.18f, 8, fill);
        DrawRectangleRoundedLines(box, 0.18f, 8, 1.0f, __GridColor());

        float textWidth = __MeasureText(text, 17);
        __TextBold(text, box.x + (box.width - textWidth) / 2.0f, box.y + (box.height - __S(21.0f)) * 0.5f, 17, active ? Pal::BtnTextOn : __TextColor());
        return pressed;
    }

    // Draw and update a slider
    // yes, this is a tiny handmade UI system
    // I'M THE GENIUS HA HA HA HA (*laughter by old money
    bool App::__Slider(Rectangle box, const char* label, float* val, float loVal, float hiVal, bool intDisp) {
        const float labelW  = std::min(__S(132.0f), box.width * 0.34f);
        const float valueW  = __S(58.0f);
        const float gap     = __S(10.0f);
        const float barX    = box.x + labelW + gap;
        const float barW    = std::max(__S(70.0f), box.width - labelW - valueW - gap * 2.0f);
        const float barY    = box.y + box.height * 0.5f - __S(4.0f);

        __TextBold(label, box.x, box.y + __S(3.0f), 15, __TextColor());

        Rectangle bar{ barX, barY, barW, __S(8.0f) };
        DrawRectangleRounded(bar, 0.4f, 8, __theme == Theme::Dark ? Pal::TrackD : Pal::TrackL);

        float ratio = (*val - loVal) / (hiVal - loVal);
        ratio = std::clamp(ratio, 0.0f, 1.0f);
        Rectangle progress = bar;
        progress.width *= ratio;
        DrawRectangleRounded(progress, 0.4f, 8, __AccentColor());

        float knobX = bar.x + bar.width * ratio;
        DrawCircle(static_cast<int>(knobX), static_cast<int>(bar.y + bar.height / 2.0f), __S(8.0f), __theme == Theme::Dark ? Pal::OverlayD : Pal::OverlayL);

        char buffer[64];
        if (std::string(label) == "Obstacle") {
            std::snprintf(buffer, sizeof(buffer), "%.0f%%", *val * 100.0f);
        }
        else if (intDisp) {
            std::snprintf(buffer, sizeof(buffer), "%.0f", *val);
        }
        else {
            std::snprintf(buffer, sizeof(buffer), "%.2f", *val);
        }
        __TextBold(buffer, bar.x + bar.width + gap, box.y + __S(3.0f), 15, __MutedColor());

        bool changed = false;
        if (CheckCollisionPointRec(GetMousePosition(), { bar.x - __S(8.0f), bar.y - __S(14.0f), bar.width + __S(16.0f), bar.height + __S(28.0f) }) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float newRatio = (GetMouseX() - bar.x) / bar.width;
            newRatio = std::clamp(newRatio, 0.0f, 1.0f);
            *val = loVal + newRatio * (hiVal - loVal);
            changed = true;
        }
        return changed;
    }

    // Scale a UI size by cur UI scale
    float App::__S(float val) const {
        return val * __scale;
    }

    // Draw normal UI text with the loaded UI font
    void App::__Text(const std::string& text, float x, float y, float fs, Color color) const {
        const float scaledSize = fs * __scale;
        const float spacing = 0.20f * __scale;
        DrawTextEx(__font, text.c_str(), Vector2{ x, y }, scaledSize, spacing, color);
    }

    // Fake bold text by drawing it a few times
    // this is the most stupid code in this project
    // dare u stare at it for 3 seconds?
    void App::__TextBold(const std::string& text, float x, float y, float fs, Color color) const {
        const float scaledSize  = fs * __scale;
        const float spacing     = 0.20f * __scale;
        const float offset      = std::max(0.55f, 0.55f * __scale);
        DrawTextEx(__font, text.c_str(), Vector2{ x, y }, scaledSize, spacing, color);
        DrawTextEx(__font, text.c_str(), Vector2{ x + offset, y }, scaledSize, spacing, color);
        DrawTextEx(__font, text.c_str(), Vector2{ x, y + offset * 0.35f }, scaledSize, spacing, color);
    }

    // Measure UI text width using the same font settings as drawing
    float App::__MeasureText(const std::string& text, float fs) const {
        const float scaledSize = fs * __scale;
        return MeasureTextEx(__font, text.c_str(), scaledSize, 0.20f * __scale).x;
    }

    // Return animation frm count for cur alg resul
    int App::__FrameCnt() const {
        return __hasRes ? static_cast<int>(__result.frames.size()) : 0;
    }

    // Return the currently selected animation frm
    // or nullptr when nothing exists (another corner case
    const SearchFrm* App::__CurFrame() const {
        if (!__hasRes || __result.frames.empty()) return nullptr;
        int idx = std::clamp(__frmIdx, 0, __FrameCnt() - 1);
        return &__result.frames[static_cast<size_t>(idx)];
    }

    // Convert 2D mouse position into grid cell index
    int App::__MouseCell(Vector2 mouse) const {
        const float padding         = __S(28.0f);
        const float usableW         = __canvas.width - 2.0f * padding;
        const float usableH         = __canvas.height - 2.0f * padding;
        const float baseCellSize    = std::floor(std::min(usableW / static_cast<float>(__map.Cols()), usableH / static_cast<float>(__map.Rows())));
        const float cellSize        = std::max(3.0f, baseCellSize * __zoom2D);
        if (cellSize <= 0.0f) return -1;

        const float gridW = cellSize * static_cast<float>(__map.Cols());
        const float gridH = cellSize * static_cast<float>(__map.Rows());
        const float startX = __canvas.x + (__canvas.width - gridW) / 2.0f + __pan2D.x;
        const float startY = __canvas.y + (__canvas.height - gridH) / 2.0f + __pan2D.y;

        if (mouse.x < startX || mouse.y < startY || mouse.x >= startX + gridW || mouse.y >= startY + gridH) {
            return -1;
        }

        int col = static_cast<int>((mouse.x - startX) / cellSize);
        int row = static_cast<int>((mouse.y - startY) / cellSize);
        if (!__map.InBounds(row, col)) return -1;
        return __map.Index(row, col);
    }

    // Ray-pick a 3D cell under the mouse
    int App::__Pick3D(Vector2 mouse) const {
        if (!CheckCollisionPointRec(mouse, __canvas)) return -1;

        Ray ray = GetMouseRay(mouse, __camera);
        int bestCell = -1;
        float bestDistance = 1.0e30f;

        for (int cell = 0; cell < __map.Count(); ++cell) {
            BoundingBox box  = __CellBox3D(cell);
            RayCollision hit = GetRayCollisionBox(ray, box);
            if (hit.hit && hit.distance < bestDistance) {
                bestDistance = hit.distance;
                bestCell     = cell;
            }
        }

        return bestCell;
    }

    // Build the 3D picking box for one map cell
    BoundingBox App::__CellBox3D(int cell) const {
        if (cell < 0 || cell >= __map.Count()) {
            return BoundingBox{ Vector3{ 0.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 0.0f } };
        }

        GPos g = __map.Coord(cell);
        const float x    = static_cast<float>(g.col) - static_cast<float>(__map.Cols() - 1) * 0.5f;
        const float z    = static_cast<float>(g.row) - static_cast<float>(__map.Rows() - 1) * 0.5f;
        const float half = 0.46f;
        const float h    = __CellHgt3D(cell);

        return BoundingBox{
            Vector3{ x - half, 0.0f, z - half },
            Vector3{ x + half, std::max(0.10f, h), z + half }
        };
    }

    // Smooth nearby cell heights to make diorama terrain less blocky
    float App::__SoftHgtAt(int row, int col) const {
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

        const float h01   = weightSum > 0.0f ? sum / weightSum : 0.0f;
        const float scale = std::max(0.10f, __hgtScale) * 0.85f;
        return 0.09f + h01 * scale;
    }

    // Return the height actually used for 3D rendering
    float App::__CellHgt3D(int cell) const {
        if (cell < 0 || cell >= __map.Count()) return 0.0f;
        if (!__smooth3D) return __CellHgt(cell);
        GPos g = __map.Coord(cell);
        return __SoftHgtAt(g.row, g.col);
    }

    // Compute final 2D cell color after terrain, search state and endpoints
    Color App::__CellColor(int cell, const SearchFrm* frm) const {
        const Cell& mapCell = __map.GetCell(cell);
        const float hNorm = static_cast<float>(mapCell.height) / 10.0f;
        Color color = __TerrainColor(mapCell.terrain, hNorm, false);

        if (__map.IsObstacle(cell) && mapCell.terrain != Terrain::Water) {
            color = __theme == Theme::Dark ? Pal::CellObsD : Pal::CellObsL;
        }

        if (frm != nullptr) {
            Color closed    = __theme == Theme::Dark ? Pal::CellClosedD  : Pal::CellClosedL;
            Color open      = __theme == Theme::Dark ? Pal::CellOpenD    : Pal::CellOpenL;
            Color path      = __theme == Theme::Dark ? Pal::CellPathD    : Pal::CellPathL;
            Color cur       = __theme == Theme::Dark ? Pal::CellCurD : Pal::AccentL;
            if (__HasCell(frm->closed, cell))    color = Blend(color, closed, 0.68f);
            if (__HasCell(frm->open, cell))      color = Blend(color, open, 0.70f);
            if (__HasCell(frm->path, cell))      color = path;
            if (frm->cur == cell)                color = cur;
        }

        if (cell == __map.Start()) return __SuccessColor();
        if (cell == __map.Goal())  return __DangerColor();
        return color;
    }

    // Convert cell terrain and height val into block height
    float App::__CellHgt(int cell) const {
        if (cell < 0 || cell >= __map.Count()) return 0.0f;
        const Cell& c = __map.GetCell(cell);
        float h = 0.12f + static_cast<float>(c.height) * __hgtScale;
        if (__map.IsObstacle(cell) || __map.IsWater(cell)) h = std::max(0.65f, h + 0.55f);
        if (c.terrain == Terrain::Water) h = std::max(0.18f, h * 0.32f);
        return h;
    }

    // Convert cell index into 3D world center position
    Vector3 App::__CellPos(int cell, float lift) const {
        GPos g = __map.Coord(cell);
        const float x = static_cast<float>(g.col) - static_cast<float>(__map.Cols() - 1) * 0.5f;
        const float z = static_cast<float>(g.row) - static_cast<float>(__map.Rows() - 1) * 0.5f;
        return { x, __CellHgt3D(cell) + lift, z };
    }

    // Convert terrain grid coordinate into 3D vertex position
    Vector3 App::__TerrVtx(int row, int col, float lift) const {
        row = std::clamp(row, 0, __map.Rows() - 1);
        col = std::clamp(col, 0, __map.Cols() - 1);
        const float x = static_cast<float>(col) - static_cast<float>(__map.Cols() - 1) * 0.5f;
        const float z = static_cast<float>(row) - static_cast<float>(__map.Rows() - 1) * 0.5f;

        if (__smooth3D) {
            return { x, __SoftHgtAt(row, col) + lift, z };
        }

        float sum = 0.0f;
        int count = 0;
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                int rr = row + dr;
                int cc = col + dc;
                if (rr < 0 || rr >= __map.Rows() || cc < 0 || cc >= __map.Cols()) continue;
                sum += __CellHgt(__map.Index(rr, cc));
                ++count;
            }
        }
        const float averagedHeight = count > 0 ? sum / static_cast<float>(count) : __CellHgt(__map.Index(row, col));
        return { x, averagedHeight + lift, z };
    }

    // Catmull-Rom interpolation helper
    // currently kept for smooth 3D experiments
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

    // Check whether a cell index is inside a small frm list
    bool App::__HasCell(const std::vector<int>& values, int cell) const {
        return std::find(values.begin(), values.end(), cell) != values.end();
    }

    // Convert edit mode enum into text for the tool panel
    std::string App::__EditName() const {
        switch (__edit) {
        case Edit::Select:      return "Select";
        case Edit::Wall:        return "Wall";
        case Edit::Erase:       return "Erase";
        case Edit::Start:       return "Start";
        case Edit::Goal:        return "Goal";
        case Edit::Raise:       return "Height+";
        case Edit::Lower:       return "Height-";
        case Edit::Plain:       return "Plain";
        case Edit::Road:        return "Road";
        case Edit::Forest:      return "Forest";
        case Edit::Mountain:    return "Mountain";
        case Edit::Water:       return "Water";
        default:                return "Unknown";
        }
    }

    // Return cur movement prof display name
    std::string App::__ProfName() const {
        return ProfNameOf(__prof);
    }

    // Return cur theme display name
    std::string App::__ThemeName() const {
        return __theme == Theme::Dark ? "Dark" : "Light";
    }

    // Build the path of the selected built-in map slot
    std::string App::__SelMapPath() const {
        int slot = std::clamp(__mapSlot, 1, 3);
        return "maps/slot" + std::to_string(slot) + ".pathmap";
    }

    // Build the small hover/selected cell information string
    std::string App::__CellInfo(int cell) const {
        if (cell < 0 || cell >= __map.Count()) return "Selected: none";

        GPos g = __map.Coord(cell);
        const Cell& c = __map.GetCell(cell);
        std::string type = __map.IsWalkable(cell, __prof) ? "walkable for " + __ProfName() : "blocked for " + __ProfName();
        if (cell == __map.Start()) type = "start";
        if (cell == __map.Goal()) type = "goal";

        return "cell " + std::to_string(g.row) + "," + std::to_string(g.col)
            + " | " + TerrainName(c.terrain)
            + " | h=" + std::to_string(c.height)
            + " | " + type;
    }

}
