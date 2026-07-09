#include "GridMap.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace pathlab{

namespace{

const int MIN_SIZE       = 5;
const int MAX_ROWS       = 120;
const int MAX_COLS       = 180;
const int MIN_HEIGHT     = 0;
const int MAX_HEIGHT     = 9;
const int TERRAIN_ROLL_N = 100;
const int ROAD_ROLL      = 10;
const int FOREST_ROLL    = 24;
const int MOUNTAIN_ROLL  = 31;
const int FAR_TRY_LIMIT  = 500;
const int PICK_TRY_LIMIT = 10000;
const int HILL_COUNT     = 9;
const int MAZE_STEP      = 2;
const int TERRAIN_N      = 5;

const std::array<GPos, 4> DXY4{{
    { -1,  0 }, {  1,  0 }, {  0, -1 }, {  0,  1 }
}};

const std::array<GPos, 8> DXY8{{
    { -1,  0 }, {  1,  0 }, {  0, -1 }, {  0,  1 },
    { -1, -1 }, { -1,  1 }, {  1, -1 }, {  1,  1 }
}};

const std::array<GPos, 4> MAZE_DXY{{
    { -MAZE_STEP, 0 }, { MAZE_STEP, 0 }, { 0, -MAZE_STEP }, { 0, MAZE_STEP }
}};

const double TAU                = 6.28318530718;
const double EPS                = 1.0e-9;
const double SQRT2              = 1.4142135623730950488;
const double BLOCKED_COST       = 1000000.0;
const double MIN_STEP_COST      = 0.05;
const double DRONE_COST_RATE    = 0.85;

const double ROAD_COST     = 0.0;
const double FOREST_COST   = 0.75;
const double MOUNTAIN_COST = 1.45;

const char* MAP_MAGIC = "PATHLAB_MAP_V1";

// One procedural hill used by terrain generation.
struct Hill{
    double x = 0.0;
    double y = 0.0;
    double radius = 0.25;
    double amplitude = 1.0;
};

// Movement cost parameters for one agent-terrain pair.
struct MoveRule{
    double move = 1.0;
    double slope = 1.0;
    double terrain = 0.0;
    bool blocked = false;
};

// Clamp height into the legal 0..9 range; maps should not grow skyscrapers by accident.
int ClampHeight(int val){
    return std::clamp(val, MIN_HEIGHT, MAX_HEIGHT);
}

// Convert terrain enum to array index.
const size_t TerrainIndex(Terrain terrain){
    return static_cast<size_t>(terrain);
}

// Convert cell type into the save-file integer.
int CellTypeId(CellKind type){
    return static_cast<int>(type == CellKind::Obstacle);
}

// Convert terrain enum into the save-file integer.
int TerrainId(Terrain terrain){
    return static_cast<int>(terrain);
}

// Convert saved terrain integer back to enum, with clamp for dirty files.
Terrain TerrainFromId(int val){
    return static_cast<Terrain>(std::clamp(val, 0, TERRAIN_N - 1));
}

const MoveRule BLOCKED_RULE{ 1.0, 1.0, BLOCKED_COST, true };

const std::array<double, TERRAIN_N> SIMPLE_TERRAIN_COST{
    ROAD_COST, ROAD_COST, FOREST_COST, BLOCKED_COST, MOUNTAIN_COST
};

const std::array<MoveRule, TERRAIN_N> PEDESTRIAN_RULE{
    MoveRule{ 1.00, 1.00, 0.0,           false },
    MoveRule{ 0.62, 0.35, 0.0,           false },
    MoveRule{ 1.18, 1.05, FOREST_COST,   false },
    BLOCKED_RULE,
    MoveRule{ 1.32, 1.85, MOUNTAIN_COST, false }
};

const std::array<MoveRule, TERRAIN_N> CAR_RULE{
    MoveRule{ 1.80, 2.25, 0.85, false },
    MoveRule{ 0.38, 0.25, 0.0,  false },
    MoveRule{ 7.50, 3.50, 6.0,  false },
    BLOCKED_RULE,
    MoveRule{ 9.50, 5.50, 8.0,  false }
};

const std::array<MoveRule, TERRAIN_N> OFFROAD_RULE{
    MoveRule{ 0.95, 1.00, 0.0,  false },
    MoveRule{ 0.70, 0.45, 0.0,  false },
    MoveRule{ 1.02, 0.90, 0.25, false },
    BLOCKED_RULE,
    MoveRule{ 1.18, 1.20, 0.55, false }
};

const std::array<MoveRule, TERRAIN_N> DRONE_RULE{
    MoveRule{ DRONE_COST_RATE, 0.0, 0.0, false },
    MoveRule{ DRONE_COST_RATE, 0.0, 0.0, false },
    MoveRule{ DRONE_COST_RATE, 0.0, 0.0, false },
    BLOCKED_RULE, // I ... I don't think our plan can dive into the water...
    MoveRule{ DRONE_COST_RATE, 0.0, 0.0, false }
};

// Basic terrain cost used by old/simple cost path.
double PlainTerrainCost(Terrain terrain){
    return SIMPLE_TERRAIN_COST[TerrainIndex(terrain)];
}

// Pick the movement rule table for a prof and terrain.
MoveRule RuleFor(MoveProfile prof, Terrain terrain){
    const size_t id = TerrainIndex(terrain);
    switch(prof){
        case MoveProfile::Car:     return CAR_RULE[id];
        case MoveProfile::Offroad: return OFFROAD_RULE[id];
        case MoveProfile::Drone:   return DRONE_RULE[id];
        case MoveProfile::Pedestrian:
        default:                       return PEDESTRIAN_RULE[id];
    }
}

}

// Create a map and immediately resize it to a legal grid.
GridMap::GridMap(int rows, int cols){
    Resize(rows, cols);
}

// Resize the map, rebuild cells and reset start/goal.
void GridMap::Resize(int rows, int cols){
    __row = std::clamp(rows, MIN_SIZE, MAX_ROWS);
    __col = std::clamp(cols, MIN_SIZE, MAX_COLS);
    __cell.assign(static_cast<size_t>(Count()), Cell{});
    __ResetEnd();
}

// Clear every cell to default plain empty state.
void GridMap::ClearAll(){
    std::fill(__cell.begin(), __cell.end(), Cell{});
    __ResetEnd();
}

// Remove obstacles but keep most terrain information.
void GridMap::ClearObstacles(){
    for(Cell& cell : __cell){
        cell.type = CellKind::Empty;
        if(cell.terrain == Terrain::Water) cell.terrain = Terrain::Plain;
    }
    __FixEnd();
}

// Reset terrain and height without changing map size.
void GridMap::ResetTerrain(){
    for(Cell& cell : __cell){
        cell.terrain = Terrain::Plain;
        cell.height = MIN_HEIGHT;
    }
    __FixEnd();
}

// Generate a noisy random map; good for quick testing, not for beauty.
void GridMap::GenerateRandom(float obsProb, unsigned seed){
    obsProb = std::clamp(obsProb, 0.0f, 0.75f);

    std::mt19937 rng(seed);
    std::bernoulli_distribution obstacleDist(obsProb); // nyeah, another trick
                                                                   // no one could understand why I use bernoulli distribution
                                                                   // in 100 years
                                                                   // including me
    std::uniform_int_distribution<int> terrainDist(0, TERRAIN_ROLL_N - 1);
    std::uniform_int_distribution<int> heightDist(MIN_HEIGHT, MAX_HEIGHT);

    for(Cell& cell : __cell){
        cell = Cell{};
        const int roll = terrainDist(rng);
        if(roll < ROAD_ROLL)            cell.terrain = Terrain::Road;
        else if(roll < FOREST_ROLL)     cell.terrain = Terrain::Forest;
        else if(roll < MOUNTAIN_ROLL)   cell.terrain = Terrain::Mountain;

        cell.height = heightDist(rng);
        cell.type   = obstacleDist(rng) ? CellKind::Obstacle : CellKind::Empty;
    }

    __start = __PickFree(rng);
    __goal  = __PickFree(rng);

    for(int attempt = 0; attempt < FAR_TRY_LIMIT; ++attempt){
        const int       candidate   = __PickFree(rng);
        const GPos s           = Coord(__start);
        const GPos g           = Coord(candidate);
        const int       dist        = std::abs(s.row - g.row) + std::abs(s.col - g.col);
        if(dist > (__row + __col) / 3){
            __goal = candidate;
            break;
        }
    }
    __FixEnd();
}

// Generate a smoother procedural terrain map with roads and river.
void GridMap::GenerateTerrain(unsigned seed){
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> unit(0.0, 1.0);

    std::array<Hill, HILL_COUNT> hills{};
    for(Hill& h : hills){
        h.x         = unit(rng);
        h.y         = unit(rng);
        h.radius    = 0.12 + unit(rng) * 0.28;
        h.amplitude = (unit(rng) < 0.30 ? -1.0 : 1.0) * (0.65 + unit(rng) * 1.35);
    }

    auto normX = [&](int col){ return __col <= 1 ? 0.0 : static_cast<double>(col) / static_cast<double>(__col - 1); };
    auto normY = [&](int row){ return __row <= 1 ? 0.0 : static_cast<double>(row) / static_cast<double>(__row - 1); };

    std::vector<double> raw(static_cast<size_t>(Count()), 0.0);
    double loVal = std::numeric_limits<double>::infinity();
    double hiVal = -std::numeric_limits<double>::infinity();

    for(int r = 0; r < __row; ++r){
        for(int c = 0; c < __col; ++c){
            const double x = normX(c), y = normY(r);
            double val = 0.24 * std::sin(7.5 * x + 2.2 * std::sin(3.1 * y))
                         + 0.18 * std::cos(6.5 * y + 1.7 * std::sin(4.0 * x)); // guess! why I use these wield constants
            for(const Hill& h : hills){
                const double dx = x - h.x, dy = y - h.y;
                val += h.amplitude * std::exp(-(dx * dx + dy * dy) / (2.0 * h.radius * h.radius));
            }
            raw[static_cast<size_t>(Index(r, c))] = val;
            loVal = std::min(loVal, val);
            hiVal = std::max(hiVal, val);
        }
    }

    const double range      = std::max(EPS, hiVal - loVal);
    const double roadPhase  = unit(rng) * TAU;
    const double riverPhase = unit(rng) * TAU;
    std::vector<GPos> roadCells;

    std::fill(__cell.begin(), __cell.end(), Cell{});

    for(int r = 0; r < __row; ++r){
        for(int c = 0; c < __col; ++c){
            const int idx = Index(r, c);
            Cell& cell = __cell[static_cast<size_t>(idx)];
            const double x          = normX(c), y = normY(r);
            const double normalized = (raw[static_cast<size_t>(idx)] - loVal) / range;
            const double roadY      = 0.50 + 0.12 * std::sin(6.0 * x + roadPhase) + 0.04 * std::sin(17.0 * x);
            const double riverY     = 0.27 + 0.16 * std::sin(5.0 * x + riverPhase) + 0.05 * std::sin(13.0 * x + 1.5);
            // guess! why I use these wield constants

            cell.height = ClampHeight(static_cast<int>(std::round(normalized * MAX_HEIGHT)));
            cell.type = CellKind::Empty;
            cell.terrain = Terrain::Plain;

            if(std::abs(y - riverY) < 0.020 || normalized < 0.075){
                cell.terrain = Terrain::Water;
                cell.type = CellKind::Obstacle;
            }
            if(normalized > 0.70) cell.terrain = Terrain::Mountain;
            if(normalized > 0.33 && normalized < 0.66){
                const double signal = std::sin(11.0 * x + 4.0 * std::sin(5.0 * y))
                                    + std::cos(9.0 * y + 2.5 * std::sin(6.0 * x));
                if(signal > 0.72) cell.terrain = Terrain::Forest;
            }
            if(std::abs(y - roadY) < 0.025){
                cell.terrain = Terrain::Road;
                cell.type = CellKind::Empty;
                if(cell.height > MIN_HEIGHT) --cell.height;
                roadCells.push_back({ r, c });
            }
        }
    }

    for(const GPos& road : roadCells){
        for(int dr = -1; dr <= 1; ++dr){
            for(int dc = -1; dc <= 1; ++dc){
                const int rr = road.row + dr, cc = road.col + dc;
                if(!InBounds(rr, cc)) continue;
                Cell& cell = __cell[static_cast<size_t>(Index(rr, cc))];
                if(cell.terrain == Terrain::Water){
                    cell.terrain    = Terrain::Road;
                    cell.type       = CellKind::Empty;
                }
            }
        }
    }

    int startRow    = __row / 2, startCol = std::max(1, __col / 12);
    int goalRow     = __row / 2, goalCol = std::min(__col - 2, __col * 11 / 12);
    if(!roadCells.empty()){
        auto nearestRoad = [&](int targetCol){
            GPos   best        = roadCells.front();
            int         bestScore   = std::numeric_limits<int>::max();
            for(const GPos& cell : roadCells){
                const int score = std::abs(cell.col - targetCol) * 3 + std::abs(cell.row - __row / 2);
                if(score < bestScore) bestScore = score, best = cell;
            }
            return best;
        };
        const GPos s = nearestRoad(startCol), g = nearestRoad(goalCol);
        startRow    = s.row; startCol   = s.col;
        goalRow     = g.row; goalCol    = g.col;
    }
    __start = Index(startRow, startCol);
    __goal = Index(goalRow, goalCol);
    __FixEnd();
}

// Generate a maze by digging two-cell steps through walls.
void GridMap::GenerateMaze(unsigned seed){
    std::mt19937 rng(seed);
    for(Cell& cell : __cell){
        cell        = Cell{};
        cell.type   = CellKind::Obstacle;
    }

    auto carve = [&](int row, int col){
        if(!InBounds(row, col)) return;
        Cell& cell      = __cell[static_cast<size_t>(Index(row, col))];
        cell.type       = CellKind::Empty;
        cell.terrain    = Terrain::Plain;
        cell.height     = MIN_HEIGHT;
    };

    std::vector<GPos> stack;
    stack.push_back({ 1, 1 });
    carve(1, 1);

    while(!stack.empty()){
        const GPos u = stack.back();
        std::vector<GPos> candidate;
        candidate.reserve(MAZE_DXY.size());
        for(const GPos& d : MAZE_DXY){
            const int nr = u.row + d.row, nc = u.col + d.col;
            if(InBounds(nr, nc) && IsObstacle(Index(nr, nc))) candidate.push_back({ nr, nc });
        }
        if(candidate.empty()){
            stack.pop_back();
            continue;
        }
        std::uniform_int_distribution<int> pick(0, static_cast<int>(candidate.size()) - 1);
        const GPos v = candidate[static_cast<size_t>(pick(rng))];
        carve((u.row + v.row) / 2, (u.col + v.col) / 2);
        carve(v.row, v.col);
        stack.push_back(v);
    }

    __start = Index(1, 1);
    const int goalRow = std::max(1, __row % 2 == 0 ? __row - 3 : __row - 2);
    const int goalCol = std::max(1, __col % 2 == 0 ? __col - 3 : __col - 2);
    __goal = Index(goalRow, goalCol);
    __FixEnd();
}

// Save map into the simple text format used by this project.
bool GridMap::SaveToFile(const std::string& path) const{
    try{
        const std::filesystem::path filePath(path);
        if(filePath.has_parent_path()) std::filesystem::create_directories(filePath.parent_path());

        std::ofstream out(path);
        if(!out) return false;
        out << MAP_MAGIC << '\n' << __row << ' ' << __col << '\n' << __start << ' ' << __goal << '\n';
        for(const Cell& cell : __cell) out << CellTypeId(cell.type) << ' ' << TerrainId(cell.terrain) << ' ' << cell.height << '\n';
        return true;
    }catch(...){
        return false;
    }
}

// Load map from text format and repair unsafe values while reading.
bool GridMap::LoadFromFile(const std::string& path, std::string* errMsg){
    std::ifstream in(path);
    if(!in){
        if(errMsg != nullptr) *errMsg = "Cannot open file: " + path;
        return false;
    }

    std::string magic;
    std::getline(in, magic);
    if(magic != MAP_MAGIC){
        if(errMsg != nullptr) *errMsg = "Unsupported map file format.";
        return false;
    }

    int rows = 0, cols = 0, start = 0, goal = 0;
    if(!(in >> rows >> cols >> start >> goal)){
        if(errMsg != nullptr) *errMsg = "Corrupted map header.";
        return false;
    }

    rows = std::clamp(rows, MIN_SIZE, MAX_ROWS);
    cols = std::clamp(cols, MIN_SIZE, MAX_COLS);
    std::vector<Cell> loaded(static_cast<size_t>(rows * cols));

    for(int i = 0; i < rows * cols; ++i){
        int type = 0, terrain = 0, height = 0;
        if(!(in >> type >> terrain >> height)){
            if(errMsg != nullptr) *errMsg = "Corrupted cell data.";
            return false;
        }
        Cell& cell      = loaded[static_cast<size_t>(i)];
        cell.type       = type == 1 ? CellKind::Obstacle : CellKind::Empty;
        cell.terrain    = TerrainFromId(terrain);
        cell.height     = ClampHeight(height);
    }

    __row  = rows;
    __col  = cols;
    __cell = std::move(loaded);
    __start = std::clamp(start, 0, Count() - 1);
    __goal  = std::clamp(goal, 0, Count() - 1);
    __FixEnd();
    return true;
}

// Check whether row and column are inside the grid.
bool GridMap::InBounds(int row, int col) const{
    return row >= 0 && row < __row && col >= 0 && col < __col;
}

// Access one cell by index; invalid index throws because silent bugs are worse.
const Cell& GridMap::GetCell(int index) const{
    if(!__ValidIndex(index)) throw std::out_of_range("Cell index out of range");
    return __cell[static_cast<size_t>(index)];
}

// Access one cell by index; invalid index throws because silent bugs are worse.
Cell& GridMap::GetCell(int index){
    if(!__ValidIndex(index)) throw std::out_of_range("Cell index out of range");
    return __cell[static_cast<size_t>(index)];
}

// Check whether a cell is an obstacle or just invalid.
bool GridMap::IsObstacle(int index) const{
    return !__ValidIndex(index) || __cell[static_cast<size_t>(index)].type == CellKind::Obstacle;
}

// Check whether a cell is water terrain.
bool GridMap::IsWater(int index) const{
    return __ValidIndex(index) && __cell[static_cast<size_t>(index)].terrain == Terrain::Water;
}

// Check whether a cell can be visited by the selected movement prof.
bool GridMap::IsWalkable(int index) const{
    return IsWalkable(index, MoveProfile::Pedestrian);
}

// Check whether a cell can be visited by the selected movement prof.
bool GridMap::IsWalkable(int index, MoveProfile prof) const{
    if(!__ValidIndex(index))                return false;
    if(prof == MoveProfile::Drone)   return true;
    return !IsObstacle(index) && !IsWater(index);
}

// Set obstacle state and fix water/plain conflict at the same time.
void GridMap::SetObstacle(int index, bool obstacle){
    if(!__ValidIndex(index) || index == __start || index == __goal) return;
    Cell& cell = __cell[static_cast<size_t>(index)];
    cell.type = obstacle ? CellKind::Obstacle : CellKind::Empty;
    if(!obstacle && cell.terrain == Terrain::Water) cell.terrain = Terrain::Plain;
}

// Set cell terrain and enforce water as blocked.
void GridMap::SetTerrain(int index, Terrain terrain){
    if(!__ValidIndex(index)) return;
    Cell& cell = __cell[static_cast<size_t>(index)];
    cell.terrain = terrain;
    if(terrain == Terrain::Water) cell.type = CellKind::Obstacle;
    if(index == __start || index == __goal) __FixEnd();
}

// Set cell height after clamping.
void GridMap::SetHeight(int index, int height){
    if(__ValidIndex(index)) __cell[static_cast<size_t>(index)].height = ClampHeight(height);
}

// Add height delta and clamp the result.
void GridMap::AddHeight(int index, int delta){
    if(__ValidIndex(index)) SetHeight(index, __cell[static_cast<size_t>(index)].height + delta);
}

// Move the start point to a valid cell.
void GridMap::SetStart(int index){
    if(IsWalkable(index) && index != __goal) __start = index;
}

// Move the goal point to a valid cell.
void GridMap::SetGoal(int index){
    if(IsWalkable(index) && index != __start) __goal = index;
}

// Return valid neighbor cells for the search graph.
std::vector<int> GridMap::Neighbors(int index, bool diag) const{
    return Neighbors(index, diag, MoveProfile::Pedestrian);
}

// Return valid neighbor cells for the search graph.
std::vector<int> GridMap::Neighbors(int index, bool diag, MoveProfile prof) const{
    std::vector<int> result;
    if(!__ValidIndex(index)) return result;

    result.reserve(diag ? DXY8.size() : DXY4.size());
    const GPos u   = Coord(index);
    const size_t    cnt = diag ? DXY8.size() : DXY4.size();

    for(size_t k = 0; k < cnt; ++k){
        const GPos d   = DXY8[k];
        const int       nr  = u.row + d.row, nc = u.col + d.col;
        if(!InBounds(nr, nc)) continue;

        const int v = Index(nr, nc);
        if(!IsWalkable(v, prof)) continue;

        if(k >= DXY4.size() && prof != MoveProfile::Drone){
            const int sideA = Index(u.row, nc);
            const int sideB = Index(nr, u.col);
            if(!IsWalkable(sideA, prof) || !IsWalkable(sideB, prof)) continue;
        }
        result.push_back(v);
    }
    return result;
}

// Return geometric move cost, 1 for straight and sqrt(2) for diagonal.
std::vector<int> GridMap::Neighbors(int index, bool diag, MovementProfile prof) const{
    return Neighbors(index, diag, ToMoveProfile(prof));
}

double GridMap::BaseMoveCost(int from, int to) const{
    const GPos a = Coord(from), b = Coord(to);
    const int dr = std::abs(a.row - b.row), dc = std::abs(a.col - b.col);
    return (dr == 1 && dc == 1) ? SQRT2 : 1.0;
}

// Return simple terrain cost for one cell.
double GridMap::TerrainCost(int index) const{
    if(!__ValidIndex(index)) return BLOCKED_COST;
    return PlainTerrainCost(__cell[static_cast<size_t>(index)].terrain);
}

// Return full movement cost from one cell to another.
double GridMap::StepCost(int from, int to, double terrW, double hgtW) const{
    return StepCost(from, to, terrW, hgtW, MoveProfile::Pedestrian);
}

// Return full movement cost from one cell to another.
double GridMap::StepCost(int from, int to, double terrW, double hgtW, MoveProfile prof) const{
    if(!IsWalkable(from, prof) || !IsWalkable(to, prof)) return BLOCKED_COST;

    const Terrain   terrain = __cell[static_cast<size_t>(to)].terrain;
    const MoveRule      rule    = RuleFor(prof, terrain);
    if(rule.blocked) return BLOCKED_COST;
    if(prof == MoveProfile::Drone) return BaseMoveCost(from, to) * rule.move;

    const int       h1      = __cell[static_cast<size_t>(from)].height;
    const int       h2      = __cell[static_cast<size_t>(to)].height;
    const double    height  = std::abs(h1 - h2) * hgtW * rule.slope;
    return std::max(MIN_STEP_COST, BaseMoveCost(from, to) * rule.move + terrW * rule.terrain + height);
}

// Check cell index range.
bool GridMap::__ValidIndex(int index) const{
    return index >= 0 && index < Count();
}

// Default start cell near the upper-left corner.
int GridMap::__DefStart() const{
    return Index(__row / 2, __col / 5);
}

// Default goal cell near the lower-right corner.
int GridMap::__DefGoal() const{
    return Index(__row / 2, __col * 4 / 5);
}

// Pick a random walkable cell; fallback exists because random luck can be terrible.
int GridMap::__PickFree(std::mt19937& rng) const{
    std::uniform_int_distribution<int> dist(0, Count() - 1);
    for(int attempt = 0; attempt < PICK_TRY_LIMIT; ++attempt){
        const int candidate = dist(rng);
        if(IsWalkable(candidate)) return candidate;
    }
    for(int i = 0; i < Count(); ++i) if(IsWalkable(i)) return i;
    return 0;
}

// Reset start and goal after map structure changes.
void GridMap::__ResetEnd(){
    __start = __DefStart();
    __goal  = __DefGoal();
    __FixEnd();
}

// Force start and goal to stay usable after edits/generation.
void GridMap::__FixEnd(){
    if(__start < 0 || __start >= Count())   __start = 0;
    if(__goal < 0  || __goal >= Count())    __goal  = Count() - 1;

    auto makeWalkable = [&](int index){
        Cell& cell = __cell[static_cast<size_t>(index)];
        cell.type = CellKind::Empty;
        if(cell.terrain == Terrain::Water) cell.terrain = Terrain::Plain;
    };

    makeWalkable(__start);
    makeWalkable(__goal);
    if(__start == __goal){
        __goal = std::clamp(__DefGoal(), 0, Count() - 1);
        makeWalkable(__goal);
    }
}

}
