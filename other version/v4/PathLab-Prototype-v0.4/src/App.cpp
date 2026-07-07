#include "App.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>


namespace pathlab{
    namespace{
        Color PanelColor()          { return PANEL_COLOR;       }
        Color DarkPanelColor()      { return DARK_PANEL_COLOR;  }
        Color AccentColor()         { return ACCENT_COLOR;      }
        Color TextColor()           { return TEXT_COLOR;        }
        Color MutedTextColor()      { return MUTED_TEXT_COLOR;  }
        Color GridLineColor()       { return GRID_LINE_COLOR;   }
        Color DangerColor()         { return DANGER_COLOR;      }
        Color SuccessColor()        { return SUCCESS_COLOR;     }

        std::string FormatDouble(double value, int digits = 2){
            char buffer[64];
            std::snprintf(buffer, sizeof(buffer), "%.*f", digits, value);
            return std::string(buffer);
        }

        Color Blend(Color a, Color b, float t){
            t = std::clamp(t, 0.0f, 1.0f);
            auto mix = [t](unsigned char x, unsigned char y) -> unsigned char{ return static_cast<unsigned char>(x + (y - x) * t); };
            return Color{mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a)};
        }

        Color TerrainBaseColor(TerrainType terrain){
            switch(terrain){
                case TerrainType::Road:     return ROAD_COLOR;
                case TerrainType::Forest:   return FOREST_COLOR;
                case TerrainType::Water:    return WATER_COLOR;
                case TerrainType::Mountain: return MOUNTAIN_COLOR;
                case TerrainType::Plain:    return PLAIN_COLOR;
                default:                    return PLAIN_COLOR;
            }
        }

        const char* TerrainName(TerrainType terrain){
            return TerrainCatalog::Name(terrain);
        }

        std::vector<std::string> FontCandidates(){
            return{
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
    }

    App::App() : __MAP(35, 55){
        GenerateRandomMap();
    }

    // FINALLY LET'S DANCE!
    void App::Run(){
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_WINDOW_MAXIMIZED);
        InitWindow(kWindowWidth, kWindowHeight, "PathLab Prototype");
        SetWindowMinSize(1280, 760);
        MaximizeWindow();
        SetTargetFPS(60);
        __UI_FONT = GetFontDefault();
        LoadUIFont();

        while(!WindowShouldClose()){
            Layout();
            Update();
            Draw();
        }

        UnloadUIFont();
        CloseWindow();
    }

    void App::Layout(){
        UpdateUIScale();

        const float topH    = S(64.0f);
        const float bottomH = S(122.0f);
        const float leftW   = S(350.0f);
        const float rightW  = S(430.0f);

        __LEFT_PANEL    = { 0, 
                            topH,
                            leftW,
                            static_cast<float>(GetScreenHeight()) - topH - bottomH };
        __RIGHT_PANEL   = { static_cast<float>(GetScreenWidth()) - rightW,
                            topH,
                            rightW,
                            static_cast<float>(GetScreenHeight()) - topH - bottomH };
        __CANVAS        = { leftW,
                            topH,
                            static_cast<float>(GetScreenWidth()) - leftW - rightW,
                            static_cast<float>(GetScreenHeight()) - topH - bottomH };
        __BOTTOM_PANEL  = { 0,
                            static_cast<float>(GetScreenHeight()) - bottomH,
                            static_cast<float>(GetScreenWidth()),
                            bottomH };
    }

    void App::Update(){
        if(IsKeyPressed(KEY_F11)) ToggleFullscreenMode();
        if(IsKeyPressed(KEY_F10) && !__FULL_SCREEN) MaximizeWindow();

        HandleMapEditing();

		// Update the map if the user has changed the map size
        if(__PLAYING && __HAS_RESULT && FrameCount() > 0){
            __ACCUMULATOR += GetFrameTime();
            const float interval = 1.0f / std::max(1.0f, __ANIMATION_SPEED);
            while(__ACCUMULATOR >= interval){
                __ACCUMULATOR -= interval;
                ++ __FRAME_INDEX;
                if(__FRAME_INDEX >= FrameCount() - 1){
                    __FRAME_INDEX = FrameCount() - 1;
                    __PLAYING = false;
                    __STATUS_MSG = "Animation finished";
                    break;
                }
            }
        }
    }

	// Draw the application
    void App::Draw(){
        BeginDrawing();
        ClearBackground(Color{19, 22, 27, 255});

        DrawTopBar();
        DrawLeftPanel();
        DrawRightPanel();
        DrawBottomPanel();
        DrawMapCanvas();

        EndDrawing();
    }

    void App::UpdateUIScale(){
        const float widthScale  = static_cast<float>(GetScreenWidth()) / 1600.0f;
        const float heightScale = static_cast<float>(GetScreenHeight()) / 1000.0f;
        __UI_SCALE              = std::clamp(std::min(widthScale, heightScale), 1.0f, 1.22f);
    }

    void App::ToggleFullscreenMode(){
        if(!__FULL_SCREEN){
            __WINDOW_ED_WIDTH   = GetScreenWidth();
            __WINDOW_ED_HEIGHT  = GetScreenHeight();
            const int monitor   = GetCurrentMonitor();
            SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
            ToggleFullscreen();
            __FULL_SCREEN = true;
            __STATUS_MSG = "Fullscreen enabled; press F11 to return";
        }
        else{
            ToggleFullscreen();
            SetWindowSize(__WINDOW_ED_WIDTH, __WINDOW_ED_HEIGHT);
            __FULL_SCREEN = false;
            __STATUS_MSG  = "Windowed mode restored";
        }
    }

    void App::DrawTopBar(){
        DrawRectangle(0, 0, GetScreenWidth(), static_cast<int>(S(64.0f)), DarkPanelColor());
        DrawTextUI("PathLab Prototype", S(20), S(15), 26, TextColor());

        const char* displayMode = __FULL_SCREEN ? "F11: exit fullscreen" : "F11: fullscreen | F10: maximize";
        DrawTextUI(displayMode, S(640), S(22), 15, MutedTextColor());

        // Maybe I should not add these code to show the font style
        // but I did this ... I don't know why :)
        std::string fontInfo = "Font: " + __FONT_STATUS;
        float w = MeasureTextUI(fontInfo, 14);
        DrawTextUI(fontInfo, static_cast<float>(GetScreenWidth()) - w - S(20.0f), S(23.0f), 14, MutedTextColor());
    }

    void App::DrawLeftPanel(){
        DrawRectangleRec(__LEFT_PANEL, PanelColor());
        float x         = __LEFT_PANEL.x + S(22.0f);
        float y         = __LEFT_PANEL.y + S(20.0f);
        const float bw  = S(145.0f);
        const float gap = S(12.0f);


        // Let's begin. the most ugly code in this project
        // You'll never know how I get these constants
        DrawTextUI("Map creator", x, y, 24, TextColor());
        y += 34;

        if(Button({x, y, bw, 32}, "Random")) GenerateRandomMap();
        if(Button({static_cast<float>(x + bw + gap), y, bw, 32}, "Terrain")) GenerateTerrainMap();
        y += 42;
        if(Button({x, y, bw, 32}, "Maze")) GenerateMazeMap();
        if(Button({static_cast<float>(x + bw + gap), y, bw, 32}, "Clear")){
            __MAP.ClearAll();
            ResetVisualization();
            __STATUS_MSG = "Map cleared";
        }
        y += 48;

        Slider({x, y, S(300), S(25)}, "Rows", &__MAP_ROWS_VALUE, 10.0f, 80.0f, true);
        y += 40;
        Slider({x, y, S(300), S(25)}, "Columns", &__MAP_COLS_VALUE, 10.0f, 120.0f, true);
        y += 40;
        if(Button({x, y, S(300), S(34)}, "Apply map size")) ApplyMapSize();
        y += 45;

        Slider({x, y, S(300), S(25)}, "Obstacle", &__OBSTACLE_PROBABILITY, 0.0f, 0.65f);
        y += 44;

        DrawTextUI("Algorithms", x, y, 24, TextColor());
        y += 34;

        if(Button({x, y, bw, 31}, "BFS"))                                     RunAlgorithm(AlgorithmKind::BFS);
        if(Button({static_cast<float>(x + bw + gap), y, bw, 31}, "Dijkstra")) RunAlgorithm(AlgorithmKind::Dijkstra);
        y += 39;
        if(Button({x, y, bw, 31}, "A*"))                                            RunAlgorithm(AlgorithmKind::AStar);
        if(Button({static_cast<float>(x + bw + gap), y, bw, 31}, "Weighted A*"))    RunAlgorithm(AlgorithmKind::WeightedAStar);
        y += 39;
        if(Button({x, y, bw, 31}, "Greedy"))                                    RunAlgorithm(AlgorithmKind::GreedyBestFirst);
        if(Button({static_cast<float>(x + bw + gap), y, bw, 31}, "Benchmark"))  RunBenchmark();
        y += 46;

        DrawTextUI("Options", x, y, 24, TextColor());
        y += 34;

        if(ToggleButton({x, y, S(300), S(34)}, __OPTIONS.allowDiagonal ? "Diagonal: ON" : "Diagonal: OFF", __OPTIONS.allowDiagonal)){
            __OPTIONS.allowDiagonal = !__OPTIONS.allowDiagonal;
            __STATUS_MSG = __OPTIONS.allowDiagonal ? "Diagonal movement enabled" : "Diagonal movement disabled";
        }
        y += 42;
        Slider({x, y, S(300), S(25)}, "Heuristic weight", &__HEURISTTIC_WEIGHT_VALUE, 1.0f, 5.0f);
        y += 40;
        Slider({x, y, S(300), S(25)}, "Terrain weight", &__TERRAIN_WEIGHT_VALUE, 0.0f, 5.0f);
        y += 40;
        Slider({x, y, S(300), S(25)}, "Height weight", &__HEIGHT_WEIGHT_VALUE, 0.0f, 2.0f);
    }

    void App::DrawRightPanel(){
        DrawRectangleRec(__RIGHT_PANEL, PanelColor());
        float x = __RIGHT_PANEL.x + S(22.0f);
        float y = __RIGHT_PANEL.y + S(20.0f);

        DrawTextUI("Editor", x, y, 24, TextColor());
        y += 34;

        const float bw  = S(118.0f);
        const float gh  = S(34.0f);
        const float gap = S(10.0f);
        auto tool = [&](EditMode mode, const char* label, int col, int row){
            if(ToggleButton({static_cast<float>(x + col * (bw + gap)), static_cast<float>(y + row * (gh + gap)), bw, gh}, label, __EDIT_MODE == mode)) __EDIT_MODE = mode;
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

        DrawTextUI(("Mode: " + EditModeName()).c_str(), x, y, 15, MutedTextColor());
        y += 28;

        DrawTextUI("Animation", x, y, 24, TextColor());
        y += 34;

        if(Button({x, y, S(118), S(34)}, __PLAYING ? "Pause" : "Play")) if(__HAS_RESULT) __PLAYING = !__PLAYING;

        if(Button({static_cast<float>(x + static_cast<int>(S(128))), y, S(118), S(34)}, "Reset")){
            __FRAME_INDEX   = 0;
            __PLAYING       = false;
        }

        if(Button({static_cast<float>(x + static_cast<int>(S(256))), y, S(118), S(34)}, "End")){
            if(FrameCount() > 0) __FRAME_INDEX = FrameCount() - 1;
            __PLAYING = false;
        }
        
        y += 39;

        if(Button({x, y, S(118), S(34)}, "Prev")){
            __FRAME_INDEX   = std::max(0, __FRAME_INDEX - 1);
            __PLAYING       = false;
        }

        if(Button({static_cast<float>(x + static_cast<int>(S(128))), y, S(118), S(34)}, "Next")){
            __FRAME_INDEX   = std::min(FrameCount() - 1, __FRAME_INDEX + 1);
            __PLAYING       = false;
        }
        
        if(Button({static_cast<float>(x + static_cast<int>(S(256))), y, S(118), S(34)}, "Replay")){
            if(__HAS_RESULT){
                __FRAME_INDEX   = 0;
                __PLAYING       = true;
            }
        }

        y += 48;
        Slider({x, y, S(374), S(25)}, "Speed", &__ANIMATION_SPEED, 1.0f, 160.0f, true);
        y += 46;

        DrawTextUI("File / output", x, y, 24, TextColor());
        y += 34;

        if(Button({x, y, S(118), S(34)}, "Save map"))                                                   SaveCurrentMap();
        if(Button({static_cast<float>(x + static_cast<int>(S(128))), y, S(118), S(34)}, "Load map"))    LoadCurrentMap();
        if(Button({static_cast<float>(x + static_cast<int>(S(256))), y, S(118), S(34)}, "CSV"))         ExportBenchmarkCsv();
        y += 46;

        DrawTextUI("Statistics", x, y, 24, TextColor());
        y += 34;

        std::string algorithm = __HAS_RESULT ? __RESULT.algorithmName : "None";
        DrawTextUI("Algorithm: " + algorithm, x, y, 15, TextColor());
        y += 24;

        DrawTextUI("Frame: " + std::to_string(FrameCount() == 0 ? 0 : __FRAME_INDEX + 1) + " / " + std::to_string(FrameCount()), x, y, 15, TextColor());
        y += 24;

        if(__HAS_RESULT){
            DrawTextUI("Found path: " + std::string(__RESULT.found ? "Yes" : "No"), x, y, 15, __RESULT.found ? SuccessColor() : DangerColor());
            y += 24;
            DrawTextUI("Path cost: " + FormatDouble(__RESULT.pathLength, 2), x, y, 15, TextColor());
            y += 24;
            DrawTextUI("Expanded: " + std::to_string(__RESULT.expandedNodes), x, y, 15, TextColor());
            y += 24;
            DrawTextUI("Generated: " + std::to_string(__RESULT.generatedNodes), x, y, 15, TextColor());
            y += 24;
            DrawTextUI("Time: " + FormatDouble(__RESULT.elapsedMs, 3) + " ms", x, y, 15, TextColor());
            y += 26;
        }
        else{
            DrawTextUI("Run an algorithm to see data.", x, y, 15, MutedTextColor());
            y += 28;
        }

        const SearchFrame* frame = CurrentFrame();
        if(frame != nullptr){
            DrawTextUI("Open set: " + std::to_string(frame->open.size()), x, y, 15, TextColor());
            y += 23;
            DrawTextUI("Closed set: " + std::to_string(frame->closed.size()), x, y, 15, TextColor());
            y += 23;
            DrawTextUI("Current: " + std::to_string(frame->current), x, y, 15, TextColor());
            y += 28;
        }

        DrawTextUI("Benchmark", x, y, 24, TextColor());
        y += 30;

        if(__BENCHMARK_ROWS.empty()){
            DrawTextUI("Click Benchmark to compare algorithms.", x, y, 14, MutedTextColor());
        }
        else{
            DrawTextUI("Algorithm        Cost    Expanded   Time", x, y, 13, MutedTextColor());
            y += 21;
            for(const auto& row : __BENCHMARK_ROWS){
                char buffer[256];
                std::snprintf(buffer, sizeof(buffer), "%-13s %7.2f %8d %7.3f", row.algorithmName.c_str(), row.pathLength, row.expandedNodes, row.elapsedMs);
                DrawTextUI(buffer, x, y, 13, row.found ? TextColor() : DangerColor());
                y += 19;
            }
        }
    }

    void App::DrawBottomPanel(){
        DrawRectangleRec(__BOTTOM_PANEL, DarkPanelColor());

        float x = S(360.0f);
        float y = __BOTTOM_PANEL.y + S(20.0f);
        DrawTextUI("Timeline", S(22), y, 24, TextColor());

        Rectangle bar{x, __BOTTOM_PANEL.y + S(38), static_cast<float>(GetScreenWidth()) - S(800), S(12)};
        DrawRectangleRounded(bar, 0.4f, 8, Color{72, 80, 92, 255});

        float t = 0.0f;
        if(FrameCount() > 1){
            t = static_cast<float>(__FRAME_INDEX) / static_cast<float>(FrameCount() - 1);
        }
        Rectangle progress   = bar;
        progress.width      *= t;
        DrawRectangleRounded(progress, 0.4f, 8, AccentColor());

        float knobX = bar.x + bar.width * t;
        DrawCircle(static_cast<int>(knobX), static_cast<int>(bar.y + bar.height / 2.0f), 9.0f, Color{245, 247, 250, 255});

        if(CheckCollisionPointRec(GetMousePosition(),{bar.x - 8, bar.y - 14, bar.width + 16, bar.height + 28}) && IsMouseButtonDown(MOUSE_LEFT_BUTTON) && FrameCount() > 1){
            float ratio = (GetMouseX() - bar.x) / bar.width;
            ratio       = std::clamp(ratio, 0.0f, 1.0f);
            __FRAME_INDEX   = static_cast<int>(ratio * static_cast<float>(FrameCount() - 1));
            __PLAYING       = false;
        }

        std::string info = __HAS_RESULT ?
                            (__RESULT.algorithmName + " | step " + std::to_string(__FRAME_INDEX + 1) + " / " + std::to_string(FrameCount()))
                          : "Generate a map, choose a tool, then run an algorithm.";
        DrawTextUI(info, x, __BOTTOM_PANEL.y + S(67.0f), 17, MutedTextColor());
        DrawTextUI("Status: " + __STATUS_MSG, S(22), __BOTTOM_PANEL.y + S(70.0f), 17, MutedTextColor());
        DrawTextUI("Left: paint | Right: erase | Shift+Left: start | Ctrl+Left: goal | F11: fullscreen",
        static_cast<float>(GetScreenWidth()) - S(760.0f), __BOTTOM_PANEL.y + S(70.0f), 16, MutedTextColor());
    }

    void App::DrawMapCanvas(){
        // Spend a few more floats here; tight grid borders look awful on a projector.
        DrawRectangleRec(__CANVAS, Color{20, 24, 29, 255});
        DrawRectangleLinesEx(__CANVAS, 1.0f, Color{50, 58, 70, 255});

        const float padding  = S(28.0f);
        const float usableW  = __CANVAS.width - 2.0f * padding;
        const float usableH  = __CANVAS.height - 2.0f * padding;
        const float cellSize = std::floor(std::min(usableW / static_cast<float>(__MAP.Cols()), usableH / static_cast<float>(__MAP.Rows())));
        
        if(cellSize <= 0.0f) return;

        const float gridW   = cellSize * static_cast<float>(__MAP.Cols());
        const float gridH   = cellSize * static_cast<float>(__MAP.Rows());
        const float startX  = __CANVAS.x + (__CANVAS.width - gridW) / 2.0f;
        const float startY  = __CANVAS.y + (__CANVAS.height - gridH) / 2.0f;

        const SearchFrame* frame = CurrentFrame();

        for(int r = 0; r < __MAP.Rows(); ++r){
            for(int c = 0; c < __MAP.Cols(); ++c){
                int idx = __MAP.Index(r, c);
                Rectangle rect{startX + static_cast<float>(c) * cellSize, startY + static_cast<float>(r) * cellSize, cellSize, cellSize};
                DrawRectangleRec(rect, CellColor(idx, frame));
                if(cellSize >= 9.0f) DrawRectangleLinesEx(rect, 1.0f, GridLineColor());

                const Cell& cell = __MAP.GetCell(idx);
                if(cellSize >= 22.0f && !__MAP.IsObstacle(idx)){
                    DrawTextUI(std::to_string(cell.height), rect.x + cellSize * 0.36f, rect.y + cellSize * 0.22f, 13, Color{45, 50, 60, 190});
                }
            }
        }

        const int hovered = MouseToCell(GetMousePosition());
        if(hovered >= 0){
            GridCoord hc = __MAP.Coord(hovered);
            Rectangle rect{startX + static_cast<float>(hc.col) * cellSize, startY + static_cast<float>(hc.row) * cellSize, cellSize, cellSize};
            DrawRectangleLinesEx(rect, 2.0f, Color{255, 255, 255, 210});
            const Cell& cell = __MAP.GetCell(hovered);
            std::string tip = "cell " + std::to_string(hc.row) + " , " + std::to_string(hc.col)
                               + " | " + TerrainName(cell.terrain) + " | h = " + std::to_string(cell.height);
            DrawTextUI(tip, __CANVAS.x + S(22.0f), __CANVAS.y + S(22.0f), 18, TextColor());
        }else{
            DrawTextUI("Map canvas", __CANVAS.x + S(22.0f), __CANVAS.y + S(22.0f), 20, TextColor());
        }
    }

    void App::HandleMapEditing(){
        // Mouse dragging fires every frame, so every edit below must be idempotent.
        Vector2 mouse = GetMousePosition();
        if(!CheckCollisionPointRec(mouse, __CANVAS)) return;

        int cell = MouseToCell(mouse);
        if(cell < 0) return;

        if(IsMouseButtonDown(MOUSE_LEFT_BUTTON)){
            if(IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)){
                __MAP.SetStart(cell);
                ResetVisualization();
                return;
            }
            if(IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)){
                __MAP.SetGoal(cell);
                ResetVisualization();
                return;
            }

            switch(__EDIT_MODE){
            case EditMode::Wall:
                __MAP.SetObstacle(cell, true);
                break;
            case EditMode::Erase:
                __MAP.SetObstacle(cell, false);
                break;
            case EditMode::Start:
                __MAP.SetStart(cell);
                break;
            case EditMode::Goal:
                __MAP.SetGoal(cell);
                break;
            case EditMode::Raise:
                __MAP.AddHeight(cell, 1);
                break;
            case EditMode::Lower:
                __MAP.AddHeight(cell, -1);
                break;
            case EditMode::Plain:
                __MAP.SetObstacle(cell, false);
                __MAP.SetTerrain(cell, TerrainType::Plain);
                break;
            case EditMode::Road:
                __MAP.SetObstacle(cell, false);
                __MAP.SetTerrain(cell, TerrainType::Road);
                break;
            case EditMode::Forest:
                __MAP.SetObstacle(cell, false);
                __MAP.SetTerrain(cell, TerrainType::Forest);
                break;
            case EditMode::Mountain:
                __MAP.SetObstacle(cell, false);
                __MAP.SetTerrain(cell, TerrainType::Mountain);
                break;
            case EditMode::Water:
                __MAP.SetTerrain(cell, TerrainType::Water);
                break;
            }
            ResetVisualization();
        }

        if(IsMouseButtonDown(MOUSE_RIGHT_BUTTON)){
            __MAP.SetObstacle(cell, false);
            if(__MAP.GetCell(cell).terrain == TerrainType::Water) __MAP.SetTerrain(cell, TerrainType::Plain);
            ResetVisualization();
        }
    }

    void App::GenerateRandomMap(){
        __MAP.GenerateRandom(__OBSTACLE_PROBABILITY);
        ResetVisualization();
        __STATUS_MSG = "Random map generated";
    }

    void App::GenerateTerrainMap(){
        __MAP.GenerateTerrain();
        ResetVisualization();
        __STATUS_MSG = "2.5D terrain map generated";
    }

    void App::GenerateMazeMap(){
        __MAP.GenerateMaze();
        ResetVisualization();
        __STATUS_MSG = "Maze map generated";
    }

    void App::ApplyMapSize(){
        int rows = static_cast<int>(std::round(__MAP_ROWS_VALUE));
        int cols = static_cast<int>(std::round(__MAP_COLS_VALUE));
        __MAP.Resize(rows, cols);
        ResetVisualization();
        __STATUS_MSG = "Map size changed to " + std::to_string(__MAP.Rows()) + " x " + std::to_string(__MAP.Cols());
    }

    void App::RunAlgorithm(AlgorithmKind kind){
        SyncOptionsFromSliders();
        __RESULT = Pathfinder::Run(__MAP, kind, __OPTIONS);
        __HAS_RESULT = true;
        __FRAME_INDEX = 0;
        __PLAYING = true;
        __ACCUMULATOR = 0.0f;
        __STATUS_MSG = "Running " + __RESULT.algorithmName;
    }

    void App::RunBenchmark(){
        // Sync sliders before benchmarking; UI state and algorithm state must not drift.
        SyncOptionsFromSliders();
        __BENCHMARK_ROWS.clear();
        const std::vector<AlgorithmKind> algorithms = {
            AlgorithmKind::BFS,
            AlgorithmKind::Dijkstra,
            AlgorithmKind::AStar,
            AlgorithmKind::WeightedAStar,
            AlgorithmKind::GreedyBestFirst
        };

        for(AlgorithmKind algorithm : algorithms){
            SearchResult r = Pathfinder::Run(__MAP, algorithm, __OPTIONS);
            __BENCHMARK_ROWS.push_back({r.algorithmName, r.found, r.pathLength, r.expandedNodes, r.generatedNodes, r.elapsedMs});
        }
        __STATUS_MSG = "Benchmark finished for " + std::to_string(__BENCHMARK_ROWS.size()) + " algorithms";
    }

    void App::SaveCurrentMap(){
		__STATUS_MSG = __MAP.SaveToFile("maps/current.pathmap") ? "Map saved to maps/current.pathmap" : "Failed to save map";
    }

    void App::LoadCurrentMap(){
        std::string error;
        if(__MAP.LoadFromFile("maps/current.pathmap", &error)){
            __MAP_ROWS_VALUE = static_cast<float>(__MAP.Rows());
            __MAP_COLS_VALUE = static_cast<float>(__MAP.Cols());
            ResetVisualization();
            __STATUS_MSG = "Map loaded from maps/current.pathmap";
        }
        else{
            __STATUS_MSG = error.empty() ? "Failed to load map" : error;
        }
    }

    void App::ExportBenchmarkCsv(){
        try{
            std::filesystem::create_directories("exports");
            std::ofstream out("exports/benchmark.csv");
            if(!out){
                __STATUS_MSG = "Failed to create exports/benchmark.csv";
                return;
            }
            out << "algorithm,found,path_cost,expanded_nodes,generated_nodes,time_ms\n";
            for(const auto& row : __BENCHMARK_ROWS){
                out << row.algorithmName    << ',' << (row.found ? "true" : "false")    << ','
                    << row.pathLength       << ',' << row.expandedNodes                 << ','
                    << row.generatedNodes   << ',' << row.elapsedMs << '\n';
            }
            __STATUS_MSG = "Benchmark exported to exports/benchmark.csv";
        }catch(...){
            __STATUS_MSG = "Failed to export benchmark CSV";
        }
    }

    void App::ResetVisualization(){
        __HAS_RESULT    = false;
        __RESULT        = SearchResult{};
        __FRAME_INDEX   = 0;
        __PLAYING       = false;
        __ACCUMULATOR   = 0.0f;
    }

    void App::SyncOptionsFromSliders(){
        __OPTIONS.heuristicWeight   = static_cast<double>(__HEURISTTIC_WEIGHT_VALUE);
        __OPTIONS.terrainWeight     = static_cast<double>(__TERRAIN_WEIGHT_VALUE);
        __OPTIONS.heightWeight      = static_cast<double>(__HEIGHT_WEIGHT_VALUE);
    }

    void App::LoadUIFont(){
        for(const auto& path : FontCandidates()){
            if(!FileExists(path.c_str())) continue;
            Font loaded = LoadFontEx(path.c_str(), 32, nullptr, 0);
            if(loaded.texture.id != 0){
                __UI_FONT               = loaded;
                __CUSTOM_FONT_LOADED    = true;
                __FONT_STATUS           = path;
                return;
            }
        }
        __UI_FONT               = GetFontDefault();
        __CUSTOM_FONT_LOADED    = false;
        __FONT_STATUS           = "default raylib font; add assets/fonts/ui.ttf to override";
    }

    void App::UnloadUIFont(){
        if(__CUSTOM_FONT_LOADED){
            UnloadFont(__UI_FONT);
            __CUSTOM_FONT_LOADED = false;
        }
    }

    bool App::Button(Rectangle bounds, const char* text){
        Vector2 mouse       = GetMousePosition();
        bool hover          = CheckCollisionPointRec(mouse, bounds);
        bool pressed        = hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        Color fill          = hover ? Color{82, 96, 118, 255} : Color{54, 62, 75, 255};
        if(pressed) fill    = AccentColor();

        DrawRectangleRounded(bounds, 0.18f, 8, fill);
        DrawRectangleRoundedLines(bounds, 0.18f, 8, 1.0f, Color{95, 106, 124, 255});

        float textWidth = MeasureTextUI(text, 16);
        DrawTextUI(text, bounds.x + (bounds.width - textWidth) / 2.0f, bounds.y + S(8.0f), 16, TextColor());
        return pressed;
    }

    bool App::ToggleButton(Rectangle bounds, const char* text, bool active){
        Vector2 mouse   = GetMousePosition();
        bool hover      = CheckCollisionPointRec(mouse, bounds);
        bool pressed    = hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        Color fill      = active ? AccentColor() : (hover ? Color{82, 96, 118, 255} : Color{54, 62, 75, 255});

        DrawRectangleRounded(bounds, 0.18f, 8, fill);
        DrawRectangleRoundedLines(bounds, 0.18f, 8, 1.0f, Color{95, 106, 124, 255});

        float textWidth = MeasureTextUI(text, 15);
        DrawTextUI(text, bounds.x + (bounds.width - textWidth) / 2.0f, bounds.y + S(7.0f), 15, TextColor());
        return pressed;
    }

    bool App::Slider(Rectangle bounds, const char* label, float* value, float minValue, float maxValue, bool integerDisplay){
        DrawTextUI(label, bounds.x, bounds.y - S(21.0f), 15, TextColor());

        Rectangle bar{bounds.x, bounds.y + 9.0f, bounds.width, 8.0f};
        DrawRectangleRounded(bar, 0.4f, 8, Color{72, 80, 92, 255});

        float ratio = (*value - minValue) / (maxValue - minValue);
        ratio               =  std::clamp(ratio, 0.0f, 1.0f);
        Rectangle progress  =  bar;
        progress.width      *= ratio;
        DrawRectangleRounded(progress, 0.4f, 8, AccentColor());

        float knobX = bar.x + bar.width * ratio;
        DrawCircle(static_cast<int>(knobX), static_cast<int>(bar.y + bar.height / 2.0f), 8.0f, Color{245, 247, 250, 255});

        char buffer[64];
        if(std::string(label) == "Obstacle"){
            std::snprintf(buffer, sizeof(buffer), "%.0f%%", *value * 100.0f);
        }
        else if(integerDisplay){
            std::snprintf(buffer, sizeof(buffer), "%.0f", *value);
        }
        else{
            std::snprintf(buffer, sizeof(buffer), "%.2f", *value);
        }
        DrawTextUI(buffer, bounds.x + bounds.width - S(64.0f), bounds.y - S(21.0f), 15, MutedTextColor());

        bool changed = false;
        if(CheckCollisionPointRec(GetMousePosition(),{bar.x - 8, bar.y - 12, bar.width + 16, bar.height + 24}) && IsMouseButtonDown(MOUSE_LEFT_BUTTON)){
            float newRatio  = (GetMouseX() - bar.x) / bar.width;
            newRatio        = std::clamp(newRatio, 0.0f, 1.0f);
            *value          = minValue + newRatio * (maxValue - minValue);
            changed         = true;
        }
        return changed;
    }

    float App::S(float value) const{
        return value * __UI_SCALE;
    }

    void App::DrawTextUI(const std::string& text, float x, float y, float fontSize, Color color) const{
        const float scaledSize = fontSize * __UI_SCALE;
        DrawTextEx(__UI_FONT, text.c_str(), Vector2{x, y}, scaledSize, 1.15f * __UI_SCALE, color);
    }

    float App::MeasureTextUI(const std::string& text, float fontSize) const{
        const float scaledSize = fontSize * __UI_SCALE;
        return MeasureTextEx(__UI_FONT, text.c_str(), scaledSize, 1.15f * __UI_SCALE).x;
    }

    int App::FrameCount() const{
        return __HAS_RESULT ? static_cast<int>(__RESULT.frames.size()) : 0;
    }

    const SearchFrame* App::CurrentFrame() const{
        if(!__HAS_RESULT || __RESULT.frames.empty()) return nullptr;
        int idx = std::clamp(__FRAME_INDEX, 0, FrameCount() - 1);
        return &__RESULT.frames[static_cast<size_t>(idx)];
    }

    int App::MouseToCell(Vector2 mouse) const{
        const float padding     = S(28.0f);
        const float usableW     = __CANVAS.width - 2.0f * padding;
        const float usableH     = __CANVAS.height - 2.0f * padding;
        const float cellSize    = std::floor(std::min(usableW / static_cast<float>(__MAP.Cols()), usableH / static_cast<float>(__MAP.Rows())));
        if(cellSize <= 0.0f)    return -1;

        const float gridW   = cellSize * static_cast<float>(__MAP.Cols());
        const float gridH   = cellSize * static_cast<float>(__MAP.Rows());
        const float startX  = __CANVAS.x + (__CANVAS.width - gridW) / 2.0f;
        const float startY  = __CANVAS.y + (__CANVAS.height - gridH) / 2.0f;

        if(mouse.x < startX || mouse.y < startY || mouse.x >= startX + gridW || mouse.y >= startY + gridH) return -1;

        int col = static_cast<int>((mouse.x - startX) / cellSize);
        int row = static_cast<int>((mouse.y - startY) / cellSize);
        if(!__MAP.InBounds(row, col)) return -1;
        return __MAP.Index(row, col);
    }

    Color App::CellColor(int cell, const SearchFrame* frame) const{
        // Later overlays win: terrain first, search state next, endpoints last.
        const Cell& mapCell = __MAP.GetCell(cell);
        Color color         = TerrainBaseColor(mapCell.terrain);
        if(__MAP.IsObstacle(cell)) color = mapCell.terrain == TerrainType::Water ? TerrainBaseColor(TerrainType::Water) : Color{58, 64, 73, 255};

        if(!__MAP.IsObstacle(cell)){
            color = Blend(color, Color{45, 50, 58, 255}, static_cast<float>(mapCell.height) * 0.035f);
        }


        if(frame != nullptr){
            if(ContainsCell(frame->closed, cell))   color = CLOSE_COLOR;
            if(ContainsCell(frame->open, cell))     color = OPEN_COLOR;
            if(ContainsCell(frame->path, cell))     color = PATH_COLOR;
            if(frame->current == cell)              color = CELL_COLOR;
        }

        if(cell == __MAP.Start()) return START_COLOR;
        if(cell == __MAP.Goal())  return GOAL_COLOR;
        return color;
    }

    bool App::ContainsCell(const std::vector<int>& values, int cell) const{
        return std::find(values.begin(), values.end(), cell) != values.end();
    }

    std::string App::EditModeName() const{
        switch(__EDIT_MODE){
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
}
