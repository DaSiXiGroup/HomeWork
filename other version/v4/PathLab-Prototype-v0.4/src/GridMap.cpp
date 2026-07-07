#include "GridMap.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>

namespace pathlab{

    namespace{
        constexpr int MAP_MIN_SIZE                    = 5;
        constexpr int MAP_MAX_ROWS                    = 120;
        constexpr int MAP_MAX_COLS                    = 180;
        constexpr int CELL_TYPE_EMPTY_ID              = 0;
        constexpr int CELL_TYPE_OBSTACLE_ID           = 1;
        constexpr int TERRAIN_PLAIN_ID                = 0;
        constexpr int TERRAIN_ROAD_ID                 = 1;
        constexpr int TERRAIN_FOREST_ID               = 2;
        constexpr int TERRAIN_WATER_ID                = 3;
        constexpr int TERRAIN_MOUNTAIN_ID             = 4;
        constexpr int ROLL_MIN_VALUE                  = 0;
        constexpr int ROLL_MAX_VALUE                  = 99;
        constexpr int RANDOM_ROAD_LIMIT               = 10;
        constexpr int RANDOM_FOREST_LIMIT             = 24;
        constexpr int RANDOM_MOUNTAIN_LIMIT           = 31;
        constexpr int TERRAIN_ROAD_LIMIT              = 14;
        constexpr int TERRAIN_FOREST_LIMIT            = 36;
        constexpr int TERRAIN_MOUNTAIN_LIMIT          = 47;
        constexpr int TERRAIN_WATER_LIMIT             = 54;
        constexpr int OBSTACLE_PROBABILITY_MIN_X100   = 0;
        constexpr int OBSTACLE_PROBABILITY_MAX_X100   = 75;
        constexpr int CENTER_ROW_DIVISOR              = 2;
        constexpr int START_COL_DIVISOR               = 5;
        constexpr int GOAL_COL_NUMERATOR              = 4;
        constexpr int GOAL_COL_DIVISOR                = 5;
        constexpr int FAR_GOAL_RETRY_LIMIT            = 500;
        constexpr int FAR_GOAL_DISTANCE_DIVISOR       = 3;
        constexpr int FREE_CELL_RETRY_LIMIT           = 10000;
        constexpr int MAZE_FIRST_CELL                 = 1;
        constexpr int MAZE_STEP                       = 2;
        constexpr int MAZE_DIRECTION_COUNT            = 4;
        constexpr int ORTHOGONAL_DIRECTION_COUNT      = 4;
        constexpr int DIAGONAL_DIRECTION_COUNT        = 8;
        constexpr int MAZE_EVEN_BORDER_BACKOFF        = 3;
        constexpr int MAZE_ODD_BORDER_BACKOFF         = 2;
        constexpr int INVALID_INDEX                   = -1;
        constexpr double STRAIGHT_MOVE_COST           = 1.0;
        constexpr double DIAGONAL_MOVE_COST           = 1.4142135623730950488;
        constexpr double ROAD_COST_DELTA              = -0.25;
        constexpr double FOREST_COST_DELTA            = 0.8;
        constexpr double MOUNTAIN_COST_DELTA          = 1.7;
        constexpr double BLOCKED_COST                 = 1000000.0;
        constexpr double HEIGHT_COST_UNIT             = 1.0;
        constexpr const char* MAP_FILE_MAGIC          = "PATHLAB_MAP_V1";

        class PlainTerrain final : public Terrain{
        public:
            [[nodiscard]] TerrainType Kind()           const override { return TerrainType::Plain; }
            [[nodiscard]] const char* Name()           const override { return "Plain"; }
            [[nodiscard]] int         SaveId()         const override { return TERRAIN_PLAIN_ID; }
            [[nodiscard]] double      CostDelta()      const override { return DEFAULT_PLAIN_COST; }
            [[nodiscard]] bool        BlocksMovement() const override { return false; }
        };

        class RoadTerrain final : public Terrain{
        public:
            [[nodiscard]] TerrainType Kind()           const override { return TerrainType::Road; }
            [[nodiscard]] const char* Name()           const override { return "Road"; }
            [[nodiscard]] int         SaveId()         const override { return TERRAIN_ROAD_ID; }
            [[nodiscard]] double      CostDelta()      const override { return ROAD_COST_DELTA; }
            [[nodiscard]] bool        BlocksMovement() const override { return false; }
        };

        class ForestTerrain final : public Terrain{
        public:
            [[nodiscard]] TerrainType Kind()           const override { return TerrainType::Forest; }
            [[nodiscard]] const char* Name()           const override { return "Forest"; }
            [[nodiscard]] int         SaveId()         const override { return TERRAIN_FOREST_ID; }
            [[nodiscard]] double      CostDelta()      const override { return FOREST_COST_DELTA; }
            [[nodiscard]] bool        BlocksMovement() const override { return false; }
        };

        class WaterTerrain final : public Terrain{
        public:
            [[nodiscard]] TerrainType Kind()           const override { return TerrainType::Water; }
            [[nodiscard]] const char* Name()           const override { return "Water"; }
            [[nodiscard]] int         SaveId()         const override { return TERRAIN_WATER_ID; }
            [[nodiscard]] double      CostDelta()      const override { return BLOCKED_COST; }
            [[nodiscard]] bool        BlocksMovement() const override { return true; }
        };

        class MountainTerrain final : public Terrain{
        public:
            [[nodiscard]] TerrainType Kind()           const override { return TerrainType::Mountain; }
            [[nodiscard]] const char* Name()           const override { return "Mountain"; }
            [[nodiscard]] int         SaveId()         const override { return TERRAIN_MOUNTAIN_ID; }
            [[nodiscard]] double      CostDelta()      const override { return MOUNTAIN_COST_DELTA; }
            [[nodiscard]] bool        BlocksMovement() const override { return false; }
        };

        int ClampHeight(int value){ return std::clamp(value, MIN_CELL_HEIGHT, MAX_CELL_HEIGHT); }
        int ToInt(CellType type){ return type == CellType::Obstacle ? CELL_TYPE_OBSTACLE_ID : CELL_TYPE_EMPTY_ID; }

        TerrainType RandomMapTerrain(int roll){
            if(roll < RANDOM_ROAD_LIMIT)     return TerrainType::Road;
            if(roll < RANDOM_FOREST_LIMIT)   return TerrainType::Forest;
            if(roll < RANDOM_MOUNTAIN_LIMIT) return TerrainType::Mountain;
            return TerrainType::Plain;
        }

        TerrainType RichMapTerrain(int roll){
            if(roll < TERRAIN_ROAD_LIMIT)     return TerrainType::Road;
            if(roll < TERRAIN_FOREST_LIMIT)   return TerrainType::Forest;
            if(roll < TERRAIN_MOUNTAIN_LIMIT) return TerrainType::Mountain;
            if(roll < TERRAIN_WATER_LIMIT)    return TerrainType::Water;
            return TerrainType::Plain;
        }

        GridCoord MiddleStart(int rows, int cols){ return { rows / CENTER_ROW_DIVISOR, cols / START_COL_DIVISOR }; }
        GridCoord MiddleGoal (int rows, int cols){ return { rows / CENTER_ROW_DIVISOR, cols * GOAL_COL_NUMERATOR / GOAL_COL_DIVISOR }; }
    }

    void Terrain::Apply(Cell& cell) const{
        cell.terrain = Kind();
        if(BlocksMovement()) cell.type = CellType::Obstacle;
    }

    const Terrain& TerrainCatalog::Get(TerrainType terrain){
        static const PlainTerrain    plain;
        static const RoadTerrain     road;
        static const ForestTerrain   forest;
        static const WaterTerrain    water;
        static const MountainTerrain mountain;

        switch(terrain){
        case TerrainType::Road:     return road;
        case TerrainType::Forest:   return forest;
        case TerrainType::Water:    return water;
        case TerrainType::Mountain: return mountain;
        case TerrainType::Plain:
        default:                    return plain;
        }
    }

    const Terrain& TerrainCatalog::FromSaveId(int id){
        switch(id){
        case TERRAIN_ROAD_ID:     return Get(TerrainType::Road);
        case TERRAIN_FOREST_ID:   return Get(TerrainType::Forest);
        case TERRAIN_WATER_ID:    return Get(TerrainType::Water);
        case TERRAIN_MOUNTAIN_ID: return Get(TerrainType::Mountain);
        case TERRAIN_PLAIN_ID:
        default:                  return Get(TerrainType::Plain);
        }
    }

    TerrainType TerrainCatalog::TypeFromSaveId(int id)          { return FromSaveId(id).Kind(); }
    int TerrainCatalog::SaveId(TerrainType terrain)             { return Get(terrain).SaveId(); }
    const char* TerrainCatalog::Name(TerrainType terrain)       { return Get(terrain).Name(); }
    double TerrainCatalog::CostDelta(TerrainType terrain)       { return Get(terrain).CostDelta(); }
    bool TerrainCatalog::BlocksMovement(TerrainType terrain)    { return Get(terrain).BlocksMovement(); }

    GridMap::GridMap(int rows, int cols){ Resize(rows, cols); }

    void GridMap::Resize(int newRows, int newCols){
        rows = std::clamp(newRows, MAP_MIN_SIZE, MAP_MAX_ROWS);
        cols = std::clamp(newCols, MAP_MIN_SIZE, MAP_MAX_COLS);
        cells.assign(static_cast<size_t>(Count()), Cell{});
        start = Index(MiddleStart(rows, cols).row, MiddleStart(rows, cols).col);
        goal  = Index(MiddleGoal(rows, cols).row,  MiddleGoal(rows, cols).col);
        EnsureEndpointsWalkable();
    }

    void GridMap::ClearAll(){
        std::fill(cells.begin(), cells.end(), Cell{});
        start = Index(MiddleStart(rows, cols).row, MiddleStart(rows, cols).col);
        goal  = Index(MiddleGoal(rows, cols).row,  MiddleGoal(rows, cols).col);
        EnsureEndpointsWalkable();
    }

    void GridMap::ClearObstacles(){
        for(auto& cell : cells){
            cell.type = CellType::Empty;
            if(TerrainCatalog::BlocksMovement(cell.terrain)) cell.terrain = TerrainType::Plain;
        }
        EnsureEndpointsWalkable();
    }

    void GridMap::ResetTerrain(){
        for(auto& cell : cells){
            TerrainCatalog::Get(TerrainType::Plain).Apply(cell);
            cell.height = MIN_CELL_HEIGHT;
        }
        EnsureEndpointsWalkable();
    }

    void GridMap::GenerateRandom(float obstacleProbability, unsigned seed){
        constexpr float PROBABILITY_SCALE = 100.0f;
        const float maxProbability = static_cast<float>(OBSTACLE_PROBABILITY_MAX_X100) / PROBABILITY_SCALE;
        const float minProbability = static_cast<float>(OBSTACLE_PROBABILITY_MIN_X100) / PROBABILITY_SCALE;
        obstacleProbability = std::clamp(obstacleProbability, minProbability, maxProbability);

        std::mt19937 rng(seed);
        std::bernoulli_distribution obstacleDist(obstacleProbability);
        std::uniform_int_distribution<int> terrainDist(ROLL_MIN_VALUE, ROLL_MAX_VALUE);
        std::uniform_int_distribution<int> heightDist(MIN_CELL_HEIGHT, MAX_CELL_HEIGHT);

        for(auto& cell : cells){
            cell = Cell{};
            TerrainCatalog::Get(RandomMapTerrain(terrainDist(rng))).Apply(cell);
            cell.height = heightDist(rng);
            cell.type   = obstacleDist(rng) ? CellType::Obstacle : CellType::Empty;
        }

        start = PickRandomFreeCell(rng);
        goal  = PickRandomFreeCell(rng);

        for(int attempt = 0; attempt < FAR_GOAL_RETRY_LIMIT; ++attempt){
            const int candidate = PickRandomFreeCell(rng);
            const GridCoord s   = Coord(start);
            const GridCoord g   = Coord(candidate);
            const int manhattan = std::abs(s.row - g.row) + std::abs(s.col - g.col);
            if(manhattan > (rows + cols) / FAR_GOAL_DISTANCE_DIVISOR){ goal = candidate; break; }
        }
        EnsureEndpointsWalkable();
    }

    void GridMap::GenerateTerrain(unsigned seed){
        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> terrainDist(ROLL_MIN_VALUE, ROLL_MAX_VALUE);
        std::uniform_int_distribution<int> heightDist(MIN_CELL_HEIGHT, MAX_CELL_HEIGHT);

        for(auto& cell : cells){
            cell.type = CellType::Empty;
            TerrainCatalog::Get(RichMapTerrain(terrainDist(rng))).Apply(cell);
            cell.height = heightDist(rng);
        }
        start = Index(MiddleStart(rows, cols).row, MiddleStart(rows, cols).col);
        goal  = Index(MiddleGoal(rows, cols).row,  MiddleGoal(rows, cols).col);
        EnsureEndpointsWalkable();
    }

    void GridMap::GenerateMaze(unsigned seed){
        // Maze is still a generator, but terrain behavior is no longer hard-coded here.
        std::mt19937 rng(seed);
        for(auto& cell : cells){
            cell = Cell{};
            cell.type = CellType::Obstacle;
        }

        auto carve = [&](int row, int col){
            if(!InBounds(row, col)) return;
            const int idx = Index(row, col);
            Cell& cell = cells[static_cast<size_t>(idx)];
            cell.type = CellType::Empty;
            TerrainCatalog::Get(TerrainType::Plain).Apply(cell);
            cell.height = MIN_CELL_HEIGHT;
        };

        constexpr std::array<GridCoord, MAZE_DIRECTION_COUNT> DIRECTIONS = {{
            {-MAZE_STEP, 0}, {MAZE_STEP, 0}, {0, -MAZE_STEP}, {0, MAZE_STEP}
        }};

        const GridCoord first{ MAZE_FIRST_CELL, MAZE_FIRST_CELL };
        carve(first.row, first.col);

        std::vector<GridCoord> stack;
        stack.push_back(first);

        while(!stack.empty()){
            const GridCoord current = stack.back();
            std::vector<GridCoord> candidates;
            for(const GridCoord d : DIRECTIONS){
                const int nr = current.row + d.row;
                const int nc = current.col + d.col;
                if(InBounds(nr, nc) && IsObstacle(Index(nr, nc))) candidates.push_back({ nr, nc });
            }
            if(candidates.empty()){ stack.pop_back(); continue; }

            std::uniform_int_distribution<int> pick(0, static_cast<int>(candidates.size()) - 1);
            const GridCoord next = candidates[static_cast<size_t>(pick(rng))];
            carve((current.row + next.row) / MAZE_STEP, (current.col + next.col) / MAZE_STEP);
            carve(next.row, next.col);
            stack.push_back(next);
        }

        start = Index(MAZE_FIRST_CELL, MAZE_FIRST_CELL);
        const int goalRow = (rows % MAZE_STEP == 0) ? rows - MAZE_EVEN_BORDER_BACKOFF : rows - MAZE_ODD_BORDER_BACKOFF;
        const int goalCol = (cols % MAZE_STEP == 0) ? cols - MAZE_EVEN_BORDER_BACKOFF : cols - MAZE_ODD_BORDER_BACKOFF;
        goal = Index(std::max(MAZE_FIRST_CELL, goalRow), std::max(MAZE_FIRST_CELL, goalCol));
        EnsureEndpointsWalkable();
    }

    bool GridMap::SaveToFile(const std::string& path) const{
        try{
            std::filesystem::path filePath(path);
            if(filePath.has_parent_path()) std::filesystem::create_directories(filePath.parent_path());
            std::ofstream out(path);
            if(!out) return false;
            out << MAP_FILE_MAGIC << '\n';
            out << rows << ' ' << cols << '\n';
            out << start << ' ' << goal << '\n';
            for(const auto& cell : cells){
                out << ToInt(cell.type) << ' ' << TerrainCatalog::SaveId(cell.terrain) << ' ' << cell.height << '\n';
            }
            return true;
        }catch(...){ return false; }
    }

    bool GridMap::LoadFromFile(const std::string& path, std::string* errorMessage){
        std::ifstream in(path);
        if(!in){
            if(errorMessage != nullptr) *errorMessage = "Cannot open file: " + path;
            return false;
        }

        std::string magic;
        std::getline(in, magic);
        if(magic != MAP_FILE_MAGIC){
            if(errorMessage != nullptr) *errorMessage = "Unsupported map file format.";
            return false;
        }

        int loadedRows  = 0;
        int loadedCols  = 0;
        int loadedStart = 0;
        int loadedGoal  = 0;
        if(!(in >> loadedRows >> loadedCols >> loadedStart >> loadedGoal)){
            if(errorMessage != nullptr) *errorMessage = "Corrupted map header.";
            return false;
        }

        loadedRows = std::clamp(loadedRows, MAP_MIN_SIZE, MAP_MAX_ROWS);
        loadedCols = std::clamp(loadedCols, MAP_MIN_SIZE, MAP_MAX_COLS);
        std::vector<Cell> loadedCells(static_cast<size_t>(loadedRows * loadedCols));

        for(int i = 0; i < loadedRows * loadedCols; ++i){
            int typeId = CELL_TYPE_EMPTY_ID, terrainId = TERRAIN_PLAIN_ID, height = MIN_CELL_HEIGHT;
            if(!(in >> typeId >> terrainId >> height)){
                if(errorMessage != nullptr) *errorMessage = "Corrupted cell data.";
                return false;
            }
            Cell& cell = loadedCells[static_cast<size_t>(i)];
            cell.type = typeId == CELL_TYPE_OBSTACLE_ID ? CellType::Obstacle : CellType::Empty;
            cell.terrain = TerrainCatalog::TypeFromSaveId(terrainId);
            cell.height = ClampHeight(height);
            if(TerrainCatalog::BlocksMovement(cell.terrain)) cell.type = CellType::Obstacle;
        }

        rows   = loadedRows;
        cols   = loadedCols;
        cells  = std::move(loadedCells);
        start = std::clamp(loadedStart, 0, Count() - 1);
        goal  = std::clamp(loadedGoal,  0, Count() - 1);
        EnsureEndpointsWalkable();
        return true;
    }

    bool GridMap::InBounds(int row, int col) const{ return row >= 0 && row < rows && col >= 0 && col < cols; }

    const Cell& GridMap::GetCell(int index) const{
        if(index < 0 || index >= Count()) throw std::out_of_range("Cell index out of range");
        return cells[static_cast<size_t>(index)];
    }

    Cell& GridMap::GetCell(int index){
        if(index < 0 || index >= Count()) throw std::out_of_range("Cell index out of range");
        return cells[static_cast<size_t>(index)];
    }

    bool GridMap::IsObstacle(int index) const{
        if(index < 0 || index >= Count()) return true;
        return cells[static_cast<size_t>(index)].type == CellType::Obstacle;
    }

    bool GridMap::IsWater(int index) const{
        if(index < 0 || index >= Count()) return false;
        return cells[static_cast<size_t>(index)].terrain == TerrainType::Water;
    }

    bool GridMap::IsWalkable(int index) const{
        if(index < 0 || index >= Count()) return false;
        const Cell& cell = cells[static_cast<size_t>(index)];
        return cell.type != CellType::Obstacle && !TerrainCatalog::BlocksMovement(cell.terrain);
    }

    void GridMap::SetObstacle(int index, bool obstacle){
        if(index < 0 || index >= Count()) return;
        if(index == start || index == goal) return;
        Cell& cell = cells[static_cast<size_t>(index)];
        cell.type = obstacle ? CellType::Obstacle : CellType::Empty;
        if(!obstacle && TerrainCatalog::BlocksMovement(cell.terrain)) cell.terrain = TerrainType::Plain;
    }

    void GridMap::SetTerrain(int index, TerrainType terrain){
        if(index < 0 || index >= Count()) return;
        TerrainCatalog::Get(terrain).Apply(cells[static_cast<size_t>(index)]);
        if(index == start || index == goal) EnsureEndpointsWalkable();
    }

    void GridMap::SetHeight(int index, int height){
        if(index < 0 || index >= Count()) return;
        cells[static_cast<size_t>(index)].height = ClampHeight(height);
    }

    void GridMap::AddHeight(int index, int delta){
        if(index < 0 || index >= Count()) return;
        Cell& cell = cells[static_cast<size_t>(index)];
        cell.height = ClampHeight(cell.height + delta);
    }

    void GridMap::SetStart(int index){ if(IsWalkable(index) && index != goal) start  = index; }
    void GridMap::SetGoal (int index){ if(IsWalkable(index) && index != start) goal  = index; }

    std::vector<int> GridMap::Neighbors(int index, bool allowDiagonal) const{
        // Corner cutting makes diagonal A* look smart and be wrong. Don't let it cheat.
        constexpr std::array<GridCoord, DIAGONAL_DIRECTION_COUNT> DIRECTIONS = {{
            {-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {-1, 1}, {1, -1}, {1, 1}
        }};
        const int directionCount = allowDiagonal ? DIAGONAL_DIRECTION_COUNT : ORTHOGONAL_DIRECTION_COUNT;

        std::vector<int> result;
        const GridCoord c = Coord(index);
        for(int k = 0; k < directionCount; ++k){
            const int nr = c.row + DIRECTIONS[static_cast<size_t>(k)].row;
            const int nc = c.col + DIRECTIONS[static_cast<size_t>(k)].col;
            if(!InBounds(nr, nc)) continue;

            const int next = Index(nr, nc);
            if(!IsWalkable(next)) continue;

            if(k >= ORTHOGONAL_DIRECTION_COUNT){
                const int sideA = Index(c.row, nc);
                const int sideB = Index(nr, c.col);
                if(!IsWalkable(sideA) || !IsWalkable(sideB)) continue;
            }
            result.push_back(next);
        }
        return result;
    }

    double GridMap::BaseMoveCost(int from, int to) const{
        const GridCoord a = Coord(from);
        const GridCoord b = Coord(to);
        const int dr = std::abs(a.row - b.row);
        const int dc = std::abs(a.col - b.col);
        return (dr == 1 && dc == 1) ? DIAGONAL_MOVE_COST : STRAIGHT_MOVE_COST;
    }

    double GridMap::TerrainCost(int index) const{
        if(index < 0 || index >= Count()) return BLOCKED_COST;
        return TerrainCatalog::CostDelta(cells[static_cast<size_t>(index)].terrain);
    }

    double GridMap::StepCost(int from, int to, double terrainWeight, double heightWeight) const{
        if(!IsWalkable(from) || !IsWalkable(to)) return BLOCKED_COST;
        const double terrain = std::max(0.0, terrainWeight * TerrainCost(to));
        const int h1 = cells[static_cast<size_t>(from)].height;
        const int h2 = cells[static_cast<size_t>(to)].height;
        const double height = std::abs(h1 - h2) * heightWeight * HEIGHT_COST_UNIT;
        return BaseMoveCost(from, to) + terrain + height;
    }

    int GridMap::PickRandomFreeCell(std::mt19937& rng) const{
        std::uniform_int_distribution<int> dist(0, Count() - 1);
        for(int attempt = 0; attempt < FREE_CELL_RETRY_LIMIT; ++attempt){
            const int candidate = dist(rng);
            if(IsWalkable(candidate)) return candidate;
        }
        for(int i = 0; i < Count(); ++i) if(IsWalkable(i)) return i;
        return 0;
    }

    void GridMap::EnsureEndpointsWalkable(){
        if(cells.empty()) return;
        if(start < 0 || start >= Count()) start = 0;
        if(goal  < 0 || goal  >= Count()) goal  = Count() - 1;

        auto makeWalkable = [&](int index){
            Cell& cell = cells[static_cast<size_t>(index)];
            cell.type = CellType::Empty;
            if(TerrainCatalog::BlocksMovement(cell.terrain)) cell.terrain = TerrainType::Plain;
        };
        makeWalkable(start);
        makeWalkable(goal);
        if(start == goal){
            const GridCoord fallback = MiddleGoal(rows, cols);
            goal = std::clamp(Index(fallback.row, fallback.col), 0, Count() - 1);
            makeWalkable(goal);
        }
    }

}
