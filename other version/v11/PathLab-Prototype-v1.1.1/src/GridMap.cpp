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

const std::array<GridCoord, 4> DXY4{{
    { -1,  0 }, {  1,  0 }, {  0, -1 }, {  0,  1 }
}};

const std::array<GridCoord, 8> DXY8{{
    { -1,  0 }, {  1,  0 }, {  0, -1 }, {  0,  1 },
    { -1, -1 }, { -1,  1 }, {  1, -1 }, {  1,  1 }
}};

const std::array<GridCoord, 4> MAZE_DXY{{
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

const const char* MAP_MAGIC = "PATHLAB_MAP_V1";

struct Hill{
    double x = 0.0;
    double y = 0.0;
    double radius = 0.25;
    double amplitude = 1.0;
};

struct MoveRule{
    double move = 1.0;
    double slope = 1.0;
    double terrain = 0.0;
    bool blocked = false;
};

int ClampHeight(int value){
    return std::clamp(value, MIN_HEIGHT, MAX_HEIGHT);
}

const size_t TerrainIndex(TerrainType terrain){
    return static_cast<size_t>(terrain);
}

int CellTypeId(CellType type){
    return static_cast<int>(type == CellType::Obstacle);
}

int TerrainId(TerrainType terrain){
    return static_cast<int>(terrain);
}

TerrainType TerrainFromId(int value){
    return static_cast<TerrainType>(std::clamp(value, 0, TERRAIN_N - 1));
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

double PlainTerrainCost(TerrainType terrain){
    return SIMPLE_TERRAIN_COST[TerrainIndex(terrain)];
}

MoveRule RuleFor(MovementProfile profile, TerrainType terrain){
    const size_t id = TerrainIndex(terrain);
    switch(profile){
        case MovementProfile::Car:     return CAR_RULE[id];
        case MovementProfile::Offroad: return OFFROAD_RULE[id];
        case MovementProfile::Drone:   return DRONE_RULE[id];
        case MovementProfile::Pedestrian:
        default:                       return PEDESTRIAN_RULE[id];
    }
}

}

GridMap::GridMap(int rows, int cols){
    Resize(rows, cols);
}

void GridMap::Resize(int rows, int cols){
    __rows = std::clamp(rows, MIN_SIZE, MAX_ROWS);
    __cols = std::clamp(cols, MIN_SIZE, MAX_COLS);
    __cells.assign(static_cast<size_t>(Count()), Cell{});
    __ResetEndpoints();
}

void GridMap::ClearAll(){
    std::fill(__cells.begin(), __cells.end(), Cell{});
    __ResetEndpoints();
}

void GridMap::ClearObstacles(){
    for(Cell& cell : __cells){
        cell.type = CellType::Empty;
        if(cell.terrain == TerrainType::Water) cell.terrain = TerrainType::Plain;
    }
    __EnsureEndpointsWalkable();
}

void GridMap::ResetTerrain(){
    for(Cell& cell : __cells){
        cell.terrain = TerrainType::Plain;
        cell.height = MIN_HEIGHT;
    }
    __EnsureEndpointsWalkable();
}

void GridMap::GenerateRandom(float obstacleProbability, unsigned seed){
    obstacleProbability = std::clamp(obstacleProbability, 0.0f, 0.75f);

    std::mt19937 rng(seed);
    std::bernoulli_distribution obstacleDist(obstacleProbability); // nyeah, another trick
                                                                   // no one could understand why I use bernoulli distribution
                                                                   // in 100 years
                                                                   // including me
    std::uniform_int_distribution<int> terrainDist(0, TERRAIN_ROLL_N - 1);
    std::uniform_int_distribution<int> heightDist(MIN_HEIGHT, MAX_HEIGHT);

    for(Cell& cell : __cells){
        cell = Cell{};
        const int roll = terrainDist(rng);
        if(roll < ROAD_ROLL)            cell.terrain = TerrainType::Road;
        else if(roll < FOREST_ROLL)     cell.terrain = TerrainType::Forest;
        else if(roll < MOUNTAIN_ROLL)   cell.terrain = TerrainType::Mountain;

        cell.height = heightDist(rng);
        cell.type   = obstacleDist(rng) ? CellType::Obstacle : CellType::Empty;
    }

    __start = __PickRandomFreeCell(rng);
    __goal  = __PickRandomFreeCell(rng);

    for(int attempt = 0; attempt < FAR_TRY_LIMIT; ++attempt){
        const int       candidate   = __PickRandomFreeCell(rng);
        const GridCoord s           = Coord(__start);
        const GridCoord g           = Coord(candidate);
        const int       dist        = std::abs(s.row - g.row) + std::abs(s.col - g.col);
        if(dist > (__rows + __cols) / 3){
            __goal = candidate;
            break;
        }
    }
    __EnsureEndpointsWalkable();
}

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

    auto normX = [&](int col){ return __cols <= 1 ? 0.0 : static_cast<double>(col) / static_cast<double>(__cols - 1); };
    auto normY = [&](int row){ return __rows <= 1 ? 0.0 : static_cast<double>(row) / static_cast<double>(__rows - 1); };

    std::vector<double> raw(static_cast<size_t>(Count()), 0.0);
    double minValue = std::numeric_limits<double>::infinity();
    double maxValue = -std::numeric_limits<double>::infinity();

    for(int r = 0; r < __rows; ++r){
        for(int c = 0; c < __cols; ++c){
            const double x = normX(c), y = normY(r);
            double value = 0.24 * std::sin(7.5 * x + 2.2 * std::sin(3.1 * y))
                         + 0.18 * std::cos(6.5 * y + 1.7 * std::sin(4.0 * x)); // guess! why I use these wield constants
            for(const Hill& h : hills){
                const double dx = x - h.x, dy = y - h.y;
                value += h.amplitude * std::exp(-(dx * dx + dy * dy) / (2.0 * h.radius * h.radius));
            }
            raw[static_cast<size_t>(Index(r, c))] = value;
            minValue = std::min(minValue, value);
            maxValue = std::max(maxValue, value);
        }
    }

    const double range      = std::max(EPS, maxValue - minValue);
    const double roadPhase  = unit(rng) * TAU;
    const double riverPhase = unit(rng) * TAU;
    std::vector<GridCoord> roadCells;

    std::fill(__cells.begin(), __cells.end(), Cell{});

    for(int r = 0; r < __rows; ++r){
        for(int c = 0; c < __cols; ++c){
            const int idx = Index(r, c);
            Cell& cell = __cells[static_cast<size_t>(idx)];
            const double x          = normX(c), y = normY(r);
            const double normalized = (raw[static_cast<size_t>(idx)] - minValue) / range;
            const double roadY      = 0.50 + 0.12 * std::sin(6.0 * x + roadPhase) + 0.04 * std::sin(17.0 * x);
            const double riverY     = 0.27 + 0.16 * std::sin(5.0 * x + riverPhase) + 0.05 * std::sin(13.0 * x + 1.5);
            // guess! why I use these wield constants

            cell.height = ClampHeight(static_cast<int>(std::round(normalized * MAX_HEIGHT)));
            cell.type = CellType::Empty;
            cell.terrain = TerrainType::Plain;

            if(std::abs(y - riverY) < 0.020 || normalized < 0.075){
                cell.terrain = TerrainType::Water;
                cell.type = CellType::Obstacle;
            }
            if(normalized > 0.70) cell.terrain = TerrainType::Mountain;
            if(normalized > 0.33 && normalized < 0.66){
                const double signal = std::sin(11.0 * x + 4.0 * std::sin(5.0 * y))
                                    + std::cos(9.0 * y + 2.5 * std::sin(6.0 * x));
                if(signal > 0.72) cell.terrain = TerrainType::Forest;
            }
            if(std::abs(y - roadY) < 0.025){
                cell.terrain = TerrainType::Road;
                cell.type = CellType::Empty;
                if(cell.height > MIN_HEIGHT) --cell.height;
                roadCells.push_back({ r, c });
            }
        }
    }

    for(const GridCoord& road : roadCells){
        for(int dr = -1; dr <= 1; ++dr){
            for(int dc = -1; dc <= 1; ++dc){
                const int rr = road.row + dr, cc = road.col + dc;
                if(!InBounds(rr, cc)) continue;
                Cell& cell = __cells[static_cast<size_t>(Index(rr, cc))];
                if(cell.terrain == TerrainType::Water){
                    cell.terrain    = TerrainType::Road;
                    cell.type       = CellType::Empty;
                }
            }
        }
    }

    int startRow    = __rows / 2, startCol = std::max(1, __cols / 12);
    int goalRow     = __rows / 2, goalCol = std::min(__cols - 2, __cols * 11 / 12);
    if(!roadCells.empty()){
        auto nearestRoad = [&](int targetCol){
            GridCoord   best        = roadCells.front();
            int         bestScore   = std::numeric_limits<int>::max();
            for(const GridCoord& cell : roadCells){
                const int score = std::abs(cell.col - targetCol) * 3 + std::abs(cell.row - __rows / 2);
                if(score < bestScore) bestScore = score, best = cell;
            }
            return best;
        };
        const GridCoord s = nearestRoad(startCol), g = nearestRoad(goalCol);
        startRow    = s.row; startCol   = s.col;
        goalRow     = g.row; goalCol    = g.col;
    }
    __start = Index(startRow, startCol);
    __goal = Index(goalRow, goalCol);
    __EnsureEndpointsWalkable();
}

void GridMap::GenerateMaze(unsigned seed){
    std::mt19937 rng(seed);
    for(Cell& cell : __cells){
        cell        = Cell{};
        cell.type   = CellType::Obstacle;
    }

    auto carve = [&](int row, int col){
        if(!InBounds(row, col)) return;
        Cell& cell      = __cells[static_cast<size_t>(Index(row, col))];
        cell.type       = CellType::Empty;
        cell.terrain    = TerrainType::Plain;
        cell.height     = MIN_HEIGHT;
    };

    std::vector<GridCoord> stack;
    stack.push_back({ 1, 1 });
    carve(1, 1);

    while(!stack.empty()){
        const GridCoord u = stack.back();
        std::vector<GridCoord> candidate;
        candidate.reserve(MAZE_DXY.size());
        for(const GridCoord& d : MAZE_DXY){
            const int nr = u.row + d.row, nc = u.col + d.col;
            if(InBounds(nr, nc) && IsObstacle(Index(nr, nc))) candidate.push_back({ nr, nc });
        }
        if(candidate.empty()){
            stack.pop_back();
            continue;
        }
        std::uniform_int_distribution<int> pick(0, static_cast<int>(candidate.size()) - 1);
        const GridCoord v = candidate[static_cast<size_t>(pick(rng))];
        carve((u.row + v.row) / 2, (u.col + v.col) / 2);
        carve(v.row, v.col);
        stack.push_back(v);
    }

    __start = Index(1, 1);
    const int goalRow = std::max(1, __rows % 2 == 0 ? __rows - 3 : __rows - 2);
    const int goalCol = std::max(1, __cols % 2 == 0 ? __cols - 3 : __cols - 2);
    __goal = Index(goalRow, goalCol);
    __EnsureEndpointsWalkable();
}

bool GridMap::SaveToFile(const std::string& path) const{
    try{
        const std::filesystem::path filePath(path);
        if(filePath.has_parent_path()) std::filesystem::create_directories(filePath.parent_path());

        std::ofstream out(path);
        if(!out) return false;
        out << MAP_MAGIC << '\n' << __rows << ' ' << __cols << '\n' << __start << ' ' << __goal << '\n';
        for(const Cell& cell : __cells) out << CellTypeId(cell.type) << ' ' << TerrainId(cell.terrain) << ' ' << cell.height << '\n';
        return true;
    }catch(...){
        return false;
    }
}

bool GridMap::LoadFromFile(const std::string& path, std::string* errorMessage){
    std::ifstream in(path);
    if(!in){
        if(errorMessage != nullptr) *errorMessage = "Cannot open file: " + path;
        return false;
    }

    std::string magic;
    std::getline(in, magic);
    if(magic != MAP_MAGIC){
        if(errorMessage != nullptr) *errorMessage = "Unsupported map file format.";
        return false;
    }

    int rows = 0, cols = 0, start = 0, goal = 0;
    if(!(in >> rows >> cols >> start >> goal)){
        if(errorMessage != nullptr) *errorMessage = "Corrupted map header.";
        return false;
    }

    rows = std::clamp(rows, MIN_SIZE, MAX_ROWS);
    cols = std::clamp(cols, MIN_SIZE, MAX_COLS);
    std::vector<Cell> loaded(static_cast<size_t>(rows * cols));

    for(int i = 0; i < rows * cols; ++i){
        int type = 0, terrain = 0, height = 0;
        if(!(in >> type >> terrain >> height)){
            if(errorMessage != nullptr) *errorMessage = "Corrupted cell data.";
            return false;
        }
        Cell& cell      = loaded[static_cast<size_t>(i)];
        cell.type       = type == 1 ? CellType::Obstacle : CellType::Empty;
        cell.terrain    = TerrainFromId(terrain);
        cell.height     = ClampHeight(height);
    }

    __rows  = rows;
    __cols  = cols;
    __cells = std::move(loaded);
    __start = std::clamp(start, 0, Count() - 1);
    __goal  = std::clamp(goal, 0, Count() - 1);
    __EnsureEndpointsWalkable();
    return true;
}

bool GridMap::InBounds(int row, int col) const{
    return row >= 0 && row < __rows && col >= 0 && col < __cols;
}

const Cell& GridMap::GetCell(int index) const{
    if(!__ValidIndex(index)) throw std::out_of_range("Cell index out of range");
    return __cells[static_cast<size_t>(index)];
}

Cell& GridMap::GetCell(int index){
    if(!__ValidIndex(index)) throw std::out_of_range("Cell index out of range");
    return __cells[static_cast<size_t>(index)];
}

bool GridMap::IsObstacle(int index) const{
    return !__ValidIndex(index) || __cells[static_cast<size_t>(index)].type == CellType::Obstacle;
}

bool GridMap::IsWater(int index) const{
    return __ValidIndex(index) && __cells[static_cast<size_t>(index)].terrain == TerrainType::Water;
}

bool GridMap::IsWalkable(int index) const{
    return IsWalkable(index, MovementProfile::Pedestrian);
}

bool GridMap::IsWalkable(int index, MovementProfile profile) const{
    if(!__ValidIndex(index))                return false;
    if(profile == MovementProfile::Drone)   return true;
    return !IsObstacle(index) && !IsWater(index);
}

void GridMap::SetObstacle(int index, bool obstacle){
    if(!__ValidIndex(index) || index == __start || index == __goal) return;
    Cell& cell = __cells[static_cast<size_t>(index)];
    cell.type = obstacle ? CellType::Obstacle : CellType::Empty;
    if(!obstacle && cell.terrain == TerrainType::Water) cell.terrain = TerrainType::Plain;
}

void GridMap::SetTerrain(int index, TerrainType terrain){
    if(!__ValidIndex(index)) return;
    Cell& cell = __cells[static_cast<size_t>(index)];
    cell.terrain = terrain;
    if(terrain == TerrainType::Water) cell.type = CellType::Obstacle;
    if(index == __start || index == __goal) __EnsureEndpointsWalkable();
}

void GridMap::SetHeight(int index, int height){
    if(__ValidIndex(index)) __cells[static_cast<size_t>(index)].height = ClampHeight(height);
}

void GridMap::AddHeight(int index, int delta){
    if(__ValidIndex(index)) SetHeight(index, __cells[static_cast<size_t>(index)].height + delta);
}

void GridMap::SetStart(int index){
    if(IsWalkable(index) && index != __goal) __start = index;
}

void GridMap::SetGoal(int index){
    if(IsWalkable(index) && index != __start) __goal = index;
}

std::vector<int> GridMap::Neighbors(int index, bool allowDiagonal) const{
    return Neighbors(index, allowDiagonal, MovementProfile::Pedestrian);
}

std::vector<int> GridMap::Neighbors(int index, bool allowDiagonal, MovementProfile profile) const{
    std::vector<int> result;
    if(!__ValidIndex(index)) return result;

    result.reserve(allowDiagonal ? DXY8.size() : DXY4.size());
    const GridCoord u   = Coord(index);
    const size_t    cnt = allowDiagonal ? DXY8.size() : DXY4.size();

    for(size_t k = 0; k < cnt; ++k){
        const GridCoord d   = DXY8[k];
        const int       nr  = u.row + d.row, nc = u.col + d.col;
        if(!InBounds(nr, nc)) continue;

        const int v = Index(nr, nc);
        if(!IsWalkable(v, profile)) continue;

        if(k >= DXY4.size() && profile != MovementProfile::Drone){
            const int sideA = Index(u.row, nc);
            const int sideB = Index(nr, u.col);
            if(!IsWalkable(sideA, profile) || !IsWalkable(sideB, profile)) continue;
        }
        result.push_back(v);
    }
    return result;
}

double GridMap::BaseMoveCost(int from, int to) const{
    const GridCoord a = Coord(from), b = Coord(to);
    const int dr = std::abs(a.row - b.row), dc = std::abs(a.col - b.col);
    return (dr == 1 && dc == 1) ? SQRT2 : 1.0;
}

double GridMap::TerrainCost(int index) const{
    if(!__ValidIndex(index)) return BLOCKED_COST;
    return PlainTerrainCost(__cells[static_cast<size_t>(index)].terrain);
}

double GridMap::StepCost(int from, int to, double terrainWeight, double heightWeight) const{
    return StepCost(from, to, terrainWeight, heightWeight, MovementProfile::Pedestrian);
}

double GridMap::StepCost(int from, int to, double terrainWeight, double heightWeight, MovementProfile profile) const{
    if(!IsWalkable(from, profile) || !IsWalkable(to, profile)) return BLOCKED_COST;

    const TerrainType   terrain = __cells[static_cast<size_t>(to)].terrain;
    const MoveRule      rule    = RuleFor(profile, terrain);
    if(rule.blocked) return BLOCKED_COST;
    if(profile == MovementProfile::Drone) return BaseMoveCost(from, to) * rule.move;

    const int       h1      = __cells[static_cast<size_t>(from)].height;
    const int       h2      = __cells[static_cast<size_t>(to)].height;
    const double    height  = std::abs(h1 - h2) * heightWeight * rule.slope;
    return std::max(MIN_STEP_COST, BaseMoveCost(from, to) * rule.move + terrainWeight * rule.terrain + height);
}

bool GridMap::__ValidIndex(int index) const{
    return index >= 0 && index < Count();
}

int GridMap::__DefaultStart() const{
    return Index(__rows / 2, __cols / 5);
}

int GridMap::__DefaultGoal() const{
    return Index(__rows / 2, __cols * 4 / 5);
}

int GridMap::__PickRandomFreeCell(std::mt19937& rng) const{
    std::uniform_int_distribution<int> dist(0, Count() - 1);
    for(int attempt = 0; attempt < PICK_TRY_LIMIT; ++attempt){
        const int candidate = dist(rng);
        if(IsWalkable(candidate)) return candidate;
    }
    for(int i = 0; i < Count(); ++i) if(IsWalkable(i)) return i;
    return 0;
}

void GridMap::__ResetEndpoints(){
    __start = __DefaultStart();
    __goal  = __DefaultGoal();
    __EnsureEndpointsWalkable();
}

void GridMap::__EnsureEndpointsWalkable(){
    if(__start < 0 || __start >= Count())   __start = 0;
    if(__goal < 0  || __goal >= Count())    __goal  = Count() - 1;

    auto makeWalkable = [&](int index){
        Cell& cell = __cells[static_cast<size_t>(index)];
        cell.type = CellType::Empty;
        if(cell.terrain == TerrainType::Water) cell.terrain = TerrainType::Plain;
    };

    makeWalkable(__start);
    makeWalkable(__goal);
    if(__start == __goal){
        __goal = std::clamp(__DefaultGoal(), 0, Count() - 1);
        makeWalkable(__goal);
    }
}

}