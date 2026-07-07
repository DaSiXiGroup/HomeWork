#include "App.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace pathlab {

namespace {
Color PanelColor() { return Color{ 34, 39, 46, 255 }; }
Color DarkPanelColor() { return Color{ 25, 29, 34, 255 }; }
Color AccentColor() { return Color{ 66, 135, 245, 255 }; }
Color TextColor() { return Color{ 235, 240, 247, 255 }; }
Color MutedTextColor() { return Color{ 160, 171, 186, 255 }; }
Color GridLineColor() { return Color{ 50, 56, 64, 255 }; }
Color DangerColor() { return Color{ 255, 100, 110, 255 }; }
Color SuccessColor() { return Color{ 94, 222, 128, 255 }; }

std::string FormatDouble(double value, int digits = 2) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f", digits, value);
    return std::string(buffer);
}

Color Blend(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    auto mix = [t](unsigned char x, unsigned char y) -> unsigned char {
        return static_cast<unsigned char>(static_cast<float>(x) + (static_cast<float>(y) - static_cast<float>(x)) * t);
    };
    return Color{ mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a) };
}

Color TerrainBaseColor(TerrainType terrain) {
    switch (terrain) {
        case TerrainType::Road: return Color{ 213, 199, 143, 255 };
        case TerrainType::Forest: return Color{ 113, 166, 114, 255 };
        case TerrainType::Water: return Color{ 59, 132, 204, 255 };
        case TerrainType::Mountain: return Color{ 158, 142, 130, 255 };
        case TerrainType::Plain:
        default: return Color{ 224, 229, 236, 255 };
    }
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
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial.ttf"
    };
}
} // namespace

App::App() : map_(35, 55) {
    GenerateRandomMap();
}

void App::Run() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(kWindowWidth, kWindowHeight, "PathLab Prototype - C++ raylib path planning visualization");
    SetTargetFPS(60);
    uiFont_ = GetFontDefault();
    LoadUIFont();

    while (!WindowShouldClose()) {
        Layout();
        Update();
        Draw();
    }

    UnloadUIFont();
    CloseWindow();
}

void App::Layout() {
    const float topH = 52.0f;
    const float bottomH = 98.0f;
    const float leftW = 290.0f;
    const float rightW = 330.0f;

    leftPanel_ = { 0, topH, leftW, static_cast<float>(GetScreenHeight()) - topH - bottomH };
    rightPanel_ = { static_cast<float>(GetScreenWidth()) - rightW, topH, rightW, static_cast<float>(GetScreenHeight()) - topH - bottomH };
    canvas_ = { leftW, topH, static_cast<float>(GetScreenWidth()) - leftW - rightW, static_cast<float>(GetScreenHeight()) - topH - bottomH };
    bottomPanel_ = { 0, static_cast<float>(GetScreenHeight()) - bottomH, static_cast<float>(GetScreenWidth()), bottomH };
}

void App::Update() {
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
    ClearBackground(Color{ 19, 22, 27, 255 });

    DrawTopBar();
    DrawLeftPanel();
    DrawRightPanel();
    DrawBottomPanel();
    DrawMapCanvas();

    EndDrawing();
}

void App::DrawTopBar() {
    DrawRectangle(0, 0, GetScreenWidth(), 52, DarkPanelColor());
    DrawTextUI("PathLab Prototype", 18, 12, 23, TextColor());
    DrawTextUI("interactive path planning visualization platform", 223, 18, 15, MutedTextColor());

    std::string fontInfo = "Font: " + fontStatus_;
    float w = MeasureTextUI(fontInfo, 13);
    DrawTextUI(fontInfo, static_cast<float>(GetScreenWidth()) - w - 18.0f, 19.0f, 13, MutedTextColor());
}

void App::DrawLeftPanel() {
    DrawRectangleRec(leftPanel_, PanelColor());
    int x = static_cast<int>(leftPanel_.x + 18);
    int y = static_cast<int>(leftPanel_.y + 16);
    const int bw = 120;
    const int gap = 10;

    DrawTextUI("Map creator", static_cast<float>(x), static_cast<float>(y), 22, TextColor());
    y += 34;

    if (Button({ static_cast<float>(x), static_cast<float>(y), bw, 32 }, "Random")) GenerateRandomMap();
    if (Button({ static_cast<float>(x + bw + gap), static_cast<float>(y), bw, 32 }, "Terrain")) GenerateTerrainMap();
    y += 42;
    if (Button({ static_cast<float>(x), static_cast<float>(y), bw, 32 }, "Maze")) GenerateMazeMap();
    if (Button({ static_cast<float>(x + bw + gap), static_cast<float>(y), bw, 32 }, "Clear")) {
        map_.ClearAll();
        ResetVisualization();
        statusMessage_ = "Map cleared";
    }
    y += 48;

    Slider({ static_cast<float>(x), static_cast<float>(y), 250, 25 }, "Rows", &mapRowsValue_, 10.0f, 80.0f, true);
    y += 40;
    Slider({ static_cast<float>(x), static_cast<float>(y), 250, 25 }, "Columns", &mapColsValue_, 10.0f, 120.0f, true);
    y += 40;
    if (Button({ static_cast<float>(x), static_cast<float>(y), 250, 31 }, "Apply map size")) ApplyMapSize();
    y += 45;

    Slider({ static_cast<float>(x), static_cast<float>(y), 250, 25 }, "Obstacle", &obstacleProbability_, 0.0f, 0.65f);
    y += 44;

    DrawTextUI("Algorithms", static_cast<float>(x), static_cast<float>(y), 22, TextColor());
    y += 34;

    if (Button({ static_cast<float>(x), static_cast<float>(y), bw, 31 }, "BFS")) RunAlgorithm(AlgorithmKind::BFS);
    if (Button({ static_cast<float>(x + bw + gap), static_cast<float>(y), bw, 31 }, "Dijkstra")) RunAlgorithm(AlgorithmKind::Dijkstra);
    y += 39;
    if (Button({ static_cast<float>(x), static_cast<float>(y), bw, 31 }, "A*")) RunAlgorithm(AlgorithmKind::AStar);
    if (Button({ static_cast<float>(x + bw + gap), static_cast<float>(y), bw, 31 }, "Weighted A*")) RunAlgorithm(AlgorithmKind::WeightedAStar);
    y += 39;
    if (Button({ static_cast<float>(x), static_cast<float>(y), bw, 31 }, "Greedy")) RunAlgorithm(AlgorithmKind::GreedyBestFirst);
    if (Button({ static_cast<float>(x + bw + gap), static_cast<float>(y), bw, 31 }, "Benchmark")) RunBenchmark();
    y += 46;

    DrawTextUI("Options", static_cast<float>(x), static_cast<float>(y), 22, TextColor());
    y += 34;

    if (ToggleButton({ static_cast<float>(x), static_cast<float>(y), 250, 31 }, options_.allowDiagonal ? "Diagonal: ON" : "Diagonal: OFF", options_.allowDiagonal)) {
        options_.allowDiagonal = !options_.allowDiagonal;
        statusMessage_ = options_.allowDiagonal ? "Diagonal movement enabled" : "Diagonal movement disabled";
    }
    y += 42;
    Slider({ static_cast<float>(x), static_cast<float>(y), 250, 25 }, "Heuristic weight", &heuristicWeightValue_, 1.0f, 5.0f);
    y += 40;
    Slider({ static_cast<float>(x), static_cast<float>(y), 250, 25 }, "Terrain weight", &terrainWeightValue_, 0.0f, 5.0f);
    y += 40;
    Slider({ static_cast<float>(x), static_cast<float>(y), 250, 25 }, "Height weight", &heightWeightValue_, 0.0f, 2.0f);
}

void App::DrawRightPanel() {
    DrawRectangleRec(rightPanel_, PanelColor());
    int x = static_cast<int>(rightPanel_.x + 18);
    int y = static_cast<int>(rightPanel_.y + 16);

    DrawTextUI("Editor", static_cast<float>(x), static_cast<float>(y), 22, TextColor());
    y += 34;

    const int bw = 92;
    const int gh = 29;
    const int gap = 8;
    auto tool = [&](EditMode mode, const char* label, int col, int row) {
        if (ToggleButton({ static_cast<float>(x + col * (bw + gap)), static_cast<float>(y + row * (gh + gap)), bw, gh }, label, editMode_ == mode)) {
            editMode_ = mode;
        }
    };
    tool(EditMode::Wall, "Wall", 0, 0);
    tool(EditMode::Erase, "Erase", 1, 0);
    tool(EditMode::Start, "Start", 2, 0);
    tool(EditMode::Goal, "Goal", 0, 1);
    tool(EditMode::Road, "Road", 1, 1);
    tool(EditMode::Forest, "Forest", 2, 1);
    tool(EditMode::Mountain, "Mountain", 0, 2);
    tool(EditMode::Water, "Water", 1, 2);
    tool(EditMode::Plain, "Plain", 2, 2);
    tool(EditMode::Raise, "Height+", 0, 3);
    tool(EditMode::Lower, "Height-", 1, 3);
    y += 4 * (gh + gap) + 12;

    DrawTextUI(("Mode: " + EditModeName()).c_str(), static_cast<float>(x), static_cast<float>(y), 15, MutedTextColor());
    y += 28;

    DrawTextUI("Animation", static_cast<float>(x), static_cast<float>(y), 22, TextColor());
    y += 34;
    if (Button({ static_cast<float>(x), static_cast<float>(y), 92, 31 }, playing_ ? "Pause" : "Play")) {
        if (hasResult_) playing_ = !playing_;
    }
    if (Button({ static_cast<float>(x + 100), static_cast<float>(y), 92, 31 }, "Reset")) {
        frameIndex_ = 0;
        playing_ = false;
    }
    if (Button({ static_cast<float>(x + 200), static_cast<float>(y), 92, 31 }, "End")) {
        if (FrameCount() > 0) frameIndex_ = FrameCount() - 1;
        playing_ = false;
    }
    y += 39;
    if (Button({ static_cast<float>(x), static_cast<float>(y), 92, 31 }, "Prev")) {
        frameIndex_ = std::max(0, frameIndex_ - 1);
        playing_ = false;
    }
    if (Button({ static_cast<float>(x + 100), static_cast<float>(y), 92, 31 }, "Next")) {
        frameIndex_ = std::min(FrameCount() - 1, frameIndex_ + 1);
        playing_ = false;
    }
    if (Button({ static_cast<float>(x + 200), static_cast<float>(y), 92, 31 }, "Replay")) {
        if (hasResult_) {
            frameIndex_ = 0;
            playing_ = true;
        }
    }
    y += 48;
    Slider({ static_cast<float>(x), static_cast<float>(y), 292, 25 }, "Speed", &animationSpeed_, 1.0f, 160.0f, true);
    y += 46;

    DrawTextUI("File / output", static_cast<float>(x), static_cast<float>(y), 22, TextColor());
    y += 34;
    if (Button({ static_cast<float>(x), static_cast<float>(y), 92, 31 }, "Save map")) SaveCurrentMap();
    if (Button({ static_cast<float>(x + 100), static_cast<float>(y), 92, 31 }, "Load map")) LoadCurrentMap();
    if (Button({ static_cast<float>(x + 200), static_cast<float>(y), 92, 31 }, "CSV")) ExportBenchmarkCsv();
    y += 46;

    DrawTextUI("Statistics", static_cast<float>(x), static_cast<float>(y), 22, TextColor());
    y += 34;

    std::string algorithm = hasResult_ ? result_.algorithmName : "None";
    DrawTextUI("Algorithm: " + algorithm, static_cast<float>(x), static_cast<float>(y), 15, TextColor());
    y += 24;
    DrawTextUI("Frame: " + std::to_string(FrameCount() == 0 ? 0 : frameIndex_ + 1) + " / " + std::to_string(FrameCount()), static_cast<float>(x), static_cast<float>(y), 15, TextColor());
    y += 24;

    if (hasResult_) {
        DrawTextUI("Found path: " + std::string(result_.found ? "Yes" : "No"), static_cast<float>(x), static_cast<float>(y), 15, result_.found ? SuccessColor() : DangerColor());
        y += 24;
        DrawTextUI("Path cost: " + FormatDouble(result_.pathLength, 2), static_cast<float>(x), static_cast<float>(y), 15, TextColor());
        y += 24;
        DrawTextUI("Expanded: " + std::to_string(result_.expandedNodes), static_cast<float>(x), static_cast<float>(y), 15, TextColor());
        y += 24;
        DrawTextUI("Generated: " + std::to_string(result_.generatedNodes), static_cast<float>(x), static_cast<float>(y), 15, TextColor());
        y += 24;
        DrawTextUI("Time: " + FormatDouble(result_.elapsedMs, 3) + " ms", static_cast<float>(x), static_cast<float>(y), 15, TextColor());
        y += 26;
    } else {
        DrawTextUI("Run an algorithm to see data.", static_cast<float>(x), static_cast<float>(y), 15, MutedTextColor());
        y += 28;
    }

    const SearchFrame* frame = CurrentFrame();
    if (frame != nullptr) {
        DrawTextUI("Open set: " + std::to_string(frame->open.size()), static_cast<float>(x), static_cast<float>(y), 15, TextColor());
        y += 23;
        DrawTextUI("Closed set: " + std::to_string(frame->closed.size()), static_cast<float>(x), static_cast<float>(y), 15, TextColor());
        y += 23;
        DrawTextUI("Current: " + std::to_string(frame->current), static_cast<float>(x), static_cast<float>(y), 15, TextColor());
        y += 28;
    }

    DrawTextUI("Benchmark", static_cast<float>(x), static_cast<float>(y), 22, TextColor());
    y += 30;
    if (benchmarkRows_.empty()) {
        DrawTextUI("Click Benchmark to compare algorithms.", static_cast<float>(x), static_cast<float>(y), 14, MutedTextColor());
    } else {
        DrawTextUI("Algorithm        Cost    Expanded   Time", static_cast<float>(x), static_cast<float>(y), 13, MutedTextColor());
        y += 21;
        for (const auto& row : benchmarkRows_) {
            char buffer[256];
            std::snprintf(buffer, sizeof(buffer), "%-13s %7.2f %8d %7.3f", row.algorithmName.c_str(), row.pathLength, row.expandedNodes, row.elapsedMs);
            DrawTextUI(buffer, static_cast<float>(x), static_cast<float>(y), 13, row.found ? TextColor() : DangerColor());
            y += 19;
        }
    }
}

void App::DrawBottomPanel() {
    DrawRectangleRec(bottomPanel_, DarkPanelColor());

    int x = 300;
    int y = static_cast<int>(bottomPanel_.y + 17);
    DrawTextUI("Timeline", 18, static_cast<float>(y), 22, TextColor());

    Rectangle bar{ static_cast<float>(x), bottomPanel_.y + 31, static_cast<float>(GetScreenWidth()) - 650, 10 };
    DrawRectangleRounded(bar, 0.4f, 8, Color{ 72, 80, 92, 255 });

    float t = 0.0f;
    if (FrameCount() > 1) {
        t = static_cast<float>(frameIndex_) / static_cast<float>(FrameCount() - 1);
    }
    Rectangle progress = bar;
    progress.width *= t;
    DrawRectangleRounded(progress, 0.4f, 8, AccentColor());

    float knobX = bar.x + bar.width * t;
    DrawCircle(static_cast<int>(knobX), static_cast<int>(bar.y + bar.height / 2.0f), 9.0f, Color{ 245, 247, 250, 255 });

    if (CheckCollisionPointRec(GetMousePosition(), { bar.x - 8, bar.y - 14, bar.width + 16, bar.height + 28 }) && IsMouseButtonDown(MOUSE_LEFT_BUTTON) && FrameCount() > 1) {
        float ratio = (GetMouseX() - bar.x) / bar.width;
        ratio = std::clamp(ratio, 0.0f, 1.0f);
        frameIndex_ = static_cast<int>(ratio * static_cast<float>(FrameCount() - 1));
        playing_ = false;
    }

    std::string info = hasResult_
        ? (result_.algorithmName + " | step " + std::to_string(frameIndex_ + 1) + " / " + std::to_string(FrameCount()))
        : "Generate a map, choose a tool, then run an algorithm.";
    DrawTextUI(info, static_cast<float>(x), bottomPanel_.y + 56.0f, 15, MutedTextColor());

    DrawTextUI("Status: " + statusMessage_, 18, bottomPanel_.y + 58.0f, 15, MutedTextColor());

    DrawTextUI("Left click paints selected tool | Right click erases walls | Shift+Left sets start | Ctrl+Left sets goal", 
               static_cast<float>(GetScreenWidth()) - 650.0f, bottomPanel_.y + 58.0f, 14, MutedTextColor());
}

void App::DrawMapCanvas() {
    DrawRectangleRec(canvas_, Color{ 20, 24, 29, 255 });
    DrawRectangleLinesEx(canvas_, 1.0f, Color{ 50, 58, 70, 255 });

    const float padding = 24.0f;
    const float usableW = canvas_.width - 2.0f * padding;
    const float usableH = canvas_.height - 2.0f * padding;
    const float cellSize = std::floor(std::min(usableW / static_cast<float>(map_.Cols()), usableH / static_cast<float>(map_.Rows())));
    if (cellSize <= 0.0f) return;

    const float gridW = cellSize * static_cast<float>(map_.Cols());
    const float gridH = cellSize * static_cast<float>(map_.Rows());
    const float startX = canvas_.x + (canvas_.width - gridW) / 2.0f;
    const float startY = canvas_.y + (canvas_.height - gridH) / 2.0f;

    const SearchFrame* frame = CurrentFrame();

    for (int r = 0; r < map_.Rows(); ++r) {
        for (int c = 0; c < map_.Cols(); ++c) {
            int idx = map_.Index(r, c);
            Rectangle rect{ startX + static_cast<float>(c) * cellSize, startY + static_cast<float>(r) * cellSize, cellSize, cellSize };
            DrawRectangleRec(rect, CellColor(idx, frame));
            if (cellSize >= 9.0f) DrawRectangleLinesEx(rect, 1.0f, GridLineColor());

            const Cell& cell = map_.GetCell(idx);
            if (cellSize >= 22.0f && !map_.IsObstacle(idx)) {
                DrawTextUI(std::to_string(cell.height), rect.x + cellSize * 0.37f, rect.y + cellSize * 0.25f, 11, Color{ 45, 50, 60, 180 });
            }
        }
    }

    const int hovered = MouseToCell(GetMousePosition());
    if (hovered >= 0) {
        GridCoord hc = map_.Coord(hovered);
        Rectangle rect{ startX + static_cast<float>(hc.col) * cellSize, startY + static_cast<float>(hc.row) * cellSize, cellSize, cellSize };
        DrawRectangleLinesEx(rect, 2.0f, Color{ 255, 255, 255, 210 });
        const Cell& cell = map_.GetCell(hovered);
        std::string tip = "cell " + std::to_string(hc.row) + "," + std::to_string(hc.col)
            + " | " + TerrainName(cell.terrain) + " | h=" + std::to_string(cell.height);
        DrawTextUI(tip, canvas_.x + 18.0f, canvas_.y + 18.0f, 16, TextColor());
    } else {
        DrawTextUI("Map canvas", canvas_.x + 18.0f, canvas_.y + 18.0f, 18, TextColor());
    }
}

void App::HandleMapEditing() {
    Vector2 mouse = GetMousePosition();
    if (!CheckCollisionPointRec(mouse, canvas_)) return;

    int cell = MouseToCell(mouse);
    if (cell < 0) return;

    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
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

        switch (editMode_) {
            case EditMode::Wall:
                map_.SetObstacle(cell, true);
                break;
            case EditMode::Erase:
                map_.SetObstacle(cell, false);
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
                map_.SetTerrain(cell, TerrainType::Water);
                break;
        }
        ResetVisualization();
    }

    if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
        map_.SetObstacle(cell, false);
        if (map_.GetCell(cell).terrain == TerrainType::Water) map_.SetTerrain(cell, TerrainType::Plain);
        ResetVisualization();
    }
}

void App::GenerateRandomMap() {
    map_.GenerateRandom(obstacleProbability_);
    ResetVisualization();
    statusMessage_ = "Random map generated";
}

void App::GenerateTerrainMap() {
    map_.GenerateTerrain();
    ResetVisualization();
    statusMessage_ = "2.5D terrain map generated";
}

void App::GenerateMazeMap() {
    map_.GenerateMaze();
    ResetVisualization();
    statusMessage_ = "Maze map generated";
}

void App::ApplyMapSize() {
    int rows = static_cast<int>(std::round(mapRowsValue_));
    int cols = static_cast<int>(std::round(mapColsValue_));
    map_.Resize(rows, cols);
    ResetVisualization();
    statusMessage_ = "Map size changed to " + std::to_string(map_.Rows()) + " x " + std::to_string(map_.Cols());
}

void App::RunAlgorithm(AlgorithmKind kind) {
    SyncOptionsFromSliders();
    result_ = Pathfinder::Run(map_, kind, options_);
    hasResult_ = true;
    frameIndex_ = 0;
    playing_ = true;
    accumulator_ = 0.0f;
    statusMessage_ = "Running " + result_.algorithmName;
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
    statusMessage_ = "Benchmark finished for " + std::to_string(benchmarkRows_.size()) + " algorithms";
}

void App::SaveCurrentMap() {
    if (map_.SaveToFile("maps/current.pathmap")) {
        statusMessage_ = "Map saved to maps/current.pathmap";
    } else {
        statusMessage_ = "Failed to save map";
    }
}

void App::LoadCurrentMap() {
    std::string error;
    if (map_.LoadFromFile("maps/current.pathmap", &error)) {
        mapRowsValue_ = static_cast<float>(map_.Rows());
        mapColsValue_ = static_cast<float>(map_.Cols());
        ResetVisualization();
        statusMessage_ = "Map loaded from maps/current.pathmap";
    } else {
        statusMessage_ = error.empty() ? "Failed to load map" : error;
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
}

void App::LoadUIFont() {
    for (const auto& path : FontCandidates()) {
        if (!FileExists(path.c_str())) continue;
        Font loaded = LoadFontEx(path.c_str(), 32, nullptr, 0);
        if (loaded.texture.id != 0) {
            uiFont_ = loaded;
            customFontLoaded_ = true;
            fontStatus_ = path;
            return;
        }
    }
    uiFont_ = GetFontDefault();
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
    Color fill = hover ? Color{ 82, 96, 118, 255 } : Color{ 54, 62, 75, 255 };
    if (pressed) fill = AccentColor();

    DrawRectangleRounded(bounds, 0.18f, 8, fill);
    DrawRectangleRoundedLines(bounds, 0.18f, 8, 1.0f, Color{ 95, 106, 124, 255 });

    float textWidth = MeasureTextUI(text, 14);
    DrawTextUI(text, bounds.x + (bounds.width - textWidth) / 2.0f, bounds.y + 8.0f, 14, TextColor());
    return pressed;
}

bool App::ToggleButton(Rectangle bounds, const char* text, bool active) {
    Vector2 mouse = GetMousePosition();
    bool hover = CheckCollisionPointRec(mouse, bounds);
    bool pressed = hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    Color fill = active ? AccentColor() : (hover ? Color{ 82, 96, 118, 255 } : Color{ 54, 62, 75, 255 });

    DrawRectangleRounded(bounds, 0.18f, 8, fill);
    DrawRectangleRoundedLines(bounds, 0.18f, 8, 1.0f, Color{ 95, 106, 124, 255 });

    float textWidth = MeasureTextUI(text, 13);
    DrawTextUI(text, bounds.x + (bounds.width - textWidth) / 2.0f, bounds.y + 7.0f, 13, TextColor());
    return pressed;
}

bool App::Slider(Rectangle bounds, const char* label, float* value, float minValue, float maxValue, bool integerDisplay) {
    DrawTextUI(label, bounds.x, bounds.y - 18.0f, 13, TextColor());

    Rectangle bar{ bounds.x, bounds.y + 9.0f, bounds.width, 8.0f };
    DrawRectangleRounded(bar, 0.4f, 8, Color{ 72, 80, 92, 255 });

    float ratio = (*value - minValue) / (maxValue - minValue);
    ratio = std::clamp(ratio, 0.0f, 1.0f);
    Rectangle progress = bar;
    progress.width *= ratio;
    DrawRectangleRounded(progress, 0.4f, 8, AccentColor());

    float knobX = bar.x + bar.width * ratio;
    DrawCircle(static_cast<int>(knobX), static_cast<int>(bar.y + bar.height / 2.0f), 8.0f, Color{ 245, 247, 250, 255 });

    char buffer[64];
    if (std::string(label) == "Obstacle") {
        std::snprintf(buffer, sizeof(buffer), "%.0f%%", *value * 100.0f);
    } else if (integerDisplay) {
        std::snprintf(buffer, sizeof(buffer), "%.0f", *value);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%.2f", *value);
    }
    DrawTextUI(buffer, bounds.x + bounds.width - 54.0f, bounds.y - 18.0f, 13, MutedTextColor());

    bool changed = false;
    if (CheckCollisionPointRec(GetMousePosition(), { bar.x - 8, bar.y - 12, bar.width + 16, bar.height + 24 }) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        float newRatio = (GetMouseX() - bar.x) / bar.width;
        newRatio = std::clamp(newRatio, 0.0f, 1.0f);
        *value = minValue + newRatio * (maxValue - minValue);
        changed = true;
    }
    return changed;
}

void App::DrawTextUI(const std::string& text, float x, float y, float fontSize, Color color) const {
    DrawTextEx(uiFont_, text.c_str(), Vector2{ x, y }, fontSize, 1.0f, color);
}

float App::MeasureTextUI(const std::string& text, float fontSize) const {
    return MeasureTextEx(uiFont_, text.c_str(), fontSize, 1.0f).x;
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
    const float padding = 24.0f;
    const float usableW = canvas_.width - 2.0f * padding;
    const float usableH = canvas_.height - 2.0f * padding;
    const float cellSize = std::floor(std::min(usableW / static_cast<float>(map_.Cols()), usableH / static_cast<float>(map_.Rows())));
    if (cellSize <= 0.0f) return -1;

    const float gridW = cellSize * static_cast<float>(map_.Cols());
    const float gridH = cellSize * static_cast<float>(map_.Rows());
    const float startX = canvas_.x + (canvas_.width - gridW) / 2.0f;
    const float startY = canvas_.y + (canvas_.height - gridH) / 2.0f;

    if (mouse.x < startX || mouse.y < startY || mouse.x >= startX + gridW || mouse.y >= startY + gridH) {
        return -1;
    }

    int col = static_cast<int>((mouse.x - startX) / cellSize);
    int row = static_cast<int>((mouse.y - startY) / cellSize);
    if (!map_.InBounds(row, col)) return -1;
    return map_.Index(row, col);
}

Color App::CellColor(int cell, const SearchFrame* frame) const {
    const Cell& mapCell = map_.GetCell(cell);
    Color color = TerrainBaseColor(mapCell.terrain);
    if (map_.IsObstacle(cell)) color = mapCell.terrain == TerrainType::Water ? TerrainBaseColor(TerrainType::Water) : Color{ 58, 64, 73, 255 };

    if (!map_.IsObstacle(cell)) {
        color = Blend(color, Color{ 45, 50, 58, 255 }, static_cast<float>(mapCell.height) * 0.035f);
    }

    if (frame != nullptr) {
        if (ContainsCell(frame->closed, cell)) color = Color{ 116, 129, 150, 255 };
        if (ContainsCell(frame->open, cell)) color = Color{ 88, 180, 255, 255 };
        if (ContainsCell(frame->path, cell)) color = Color{ 255, 224, 102, 255 };
        if (frame->current == cell) color = Color{ 255, 177, 66, 255 };
    }

    if (cell == map_.Start()) return Color{ 80, 220, 120, 255 };
    if (cell == map_.Goal()) return Color{ 255, 89, 94, 255 };
    return color;
}

bool App::ContainsCell(const std::vector<int>& values, int cell) const {
    return std::find(values.begin(), values.end(), cell) != values.end();
}

std::string App::EditModeName() const {
    switch (editMode_) {
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

} // namespace pathlab
