#include "GridMap.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <queue>
#include <sstream>
#include <stdexcept>

namespace pathlab {

namespace {
constexpr int kMinSize = 5;
constexpr int kMaxRows = 120;
constexpr int kMaxCols = 180;

int ClampHeight(int value) {
    return std::clamp(value, 0, 9);
}

int ToInt(CellType type) {
    return type == CellType::Obstacle ? 1 : 0;
}

int ToInt(TerrainType terrain) {
    switch (terrain) {
        case TerrainType::Road: return 1;
        case TerrainType::Forest: return 2;
        case TerrainType::Water: return 3;
        case TerrainType::Mountain: return 4;
        case TerrainType::Plain:
        default: return 0;
    }
}

TerrainType TerrainFromInt(int value) {
    switch (value) {
        case 1: return TerrainType::Road;
        case 2: return TerrainType::Forest;
        case 3: return TerrainType::Water;
        case 4: return TerrainType::Mountain;
        case 0:
        default: return TerrainType::Plain;
    }
}
} // namespace

GridMap::GridMap(int rows, int cols) {
    Resize(rows, cols);
}

void GridMap::Resize(int rows, int cols) {
    rows_ = std::clamp(rows, kMinSize, kMaxRows);
    cols_ = std::clamp(cols, kMinSize, kMaxCols);
    cells_.assign(static_cast<size_t>(rows_ * cols_), Cell{});
    start_ = Index(rows_ / 2, cols_ / 5);
    goal_ = Index(rows_ / 2, cols_ * 4 / 5);
    EnsureEndpointsWalkable();
}

void GridMap::ClearAll() {
    std::fill(cells_.begin(), cells_.end(), Cell{});
    start_ = Index(rows_ / 2, cols_ / 5);
    goal_ = Index(rows_ / 2, cols_ * 4 / 5);
    EnsureEndpointsWalkable();
}

void GridMap::ClearObstacles() {
    for (auto& cell : cells_) {
        cell.type = CellType::Empty;
        if (cell.terrain == TerrainType::Water) {
            cell.terrain = TerrainType::Plain;
        }
    }
    EnsureEndpointsWalkable();
}

void GridMap::ResetTerrain() {
    for (auto& cell : cells_) {
        cell.terrain = TerrainType::Plain;
        cell.height = 0;
    }
    EnsureEndpointsWalkable();
}

void GridMap::GenerateRandom(float obstacleProbability, unsigned seed) {
    obstacleProbability = std::clamp(obstacleProbability, 0.0f, 0.75f);
    std::mt19937 rng(seed);
    std::bernoulli_distribution obstacleDist(obstacleProbability);
    std::uniform_int_distribution<int> terrainDist(0, 99);
    std::uniform_int_distribution<int> heightDist(0, 9);

    for (auto& cell : cells_) {
        cell = Cell{};
        int terrainRoll = terrainDist(rng);
        if (terrainRoll < 10) cell.terrain = TerrainType::Road;
        else if (terrainRoll < 24) cell.terrain = TerrainType::Forest;
        else if (terrainRoll < 31) cell.terrain = TerrainType::Mountain;
        else cell.terrain = TerrainType::Plain;

        cell.height = heightDist(rng);
        cell.type = obstacleDist(rng) ? CellType::Obstacle : CellType::Empty;
    }

    start_ = PickRandomFreeCell(rng);
    goal_ = PickRandomFreeCell(rng);

    for (int attempt = 0; attempt < 500; ++attempt) {
        int candidate = PickRandomFreeCell(rng);
        GridCoord s = Coord(start_);
        GridCoord g = Coord(candidate);
        int manhattan = std::abs(s.row - g.row) + std::abs(s.col - g.col);
        if (manhattan > (rows_ + cols_) / 3) {
            goal_ = candidate;
            break;
        }
    }
    EnsureEndpointsWalkable();
}

void GridMap::GenerateTerrain(unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> terrainDist(0, 99);
    std::uniform_int_distribution<int> heightDist(0, 9);

    for (auto& cell : cells_) {
        cell.type = CellType::Empty;
        int roll = terrainDist(rng);
        if (roll < 14) {
            cell.terrain = TerrainType::Road;
        } else if (roll < 36) {
            cell.terrain = TerrainType::Forest;
        } else if (roll < 47) {
            cell.terrain = TerrainType::Mountain;
        } else if (roll < 54) {
            cell.terrain = TerrainType::Water;
        } else {
            cell.terrain = TerrainType::Plain;
        }
        cell.height = heightDist(rng);
        if (cell.terrain == TerrainType::Water) cell.type = CellType::Obstacle;
    }
    start_ = Index(rows_ / 2, cols_ / 5);
    goal_ = Index(rows_ / 2, cols_ * 4 / 5);
    EnsureEndpointsWalkable();
}

void GridMap::GenerateMaze(unsigned seed) {
    // A compact recursive-backtracking maze generator on an odd-cell lattice.
    // Cells on even coordinates become walls; odd coordinates are carved as paths.
    std::mt19937 rng(seed);
    for (auto& cell : cells_) {
        cell = Cell{};
        cell.type = CellType::Obstacle;
    }

    auto carve = [&](int row, int col) {
        if (InBounds(row, col)) {
            int idx = Index(row, col);
            cells_[static_cast<size_t>(idx)].type = CellType::Empty;
            cells_[static_cast<size_t>(idx)].terrain = TerrainType::Plain;
            cells_[static_cast<size_t>(idx)].height = 0;
        }
    };

    int startRow = 1;
    int startCol = 1;
    carve(startRow, startCol);

    std::vector<GridCoord> stack;
    stack.push_back({ startRow, startCol });

    while (!stack.empty()) {
        GridCoord current = stack.back();
        std::vector<GridCoord> candidates;
        const int dr[4] = { -2, 2, 0, 0 };
        const int dc[4] = { 0, 0, -2, 2 };
        for (int k = 0; k < 4; ++k) {
            int nr = current.row + dr[k];
            int nc = current.col + dc[k];
            if (InBounds(nr, nc) && IsObstacle(Index(nr, nc))) {
                candidates.push_back({ nr, nc });
            }
        }
        if (candidates.empty()) {
            stack.pop_back();
            continue;
        }
        std::uniform_int_distribution<int> pick(0, static_cast<int>(candidates.size()) - 1);
        GridCoord next = candidates[static_cast<size_t>(pick(rng))];
        carve((current.row + next.row) / 2, (current.col + next.col) / 2);
        carve(next.row, next.col);
        stack.push_back(next);
    }

    start_ = Index(1, 1);
    int goalRow = (rows_ % 2 == 0) ? rows_ - 3 : rows_ - 2;
    int goalCol = (cols_ % 2 == 0) ? cols_ - 3 : cols_ - 2;
    goalRow = std::max(1, goalRow);
    goalCol = std::max(1, goalCol);
    goal_ = Index(goalRow, goalCol);
    EnsureEndpointsWalkable();
}

bool GridMap::SaveToFile(const std::string& path) const {
    try {
        std::filesystem::path filePath(path);
        if (filePath.has_parent_path()) {
            std::filesystem::create_directories(filePath.parent_path());
        }
        std::ofstream out(path);
        if (!out) return false;
        out << "PATHLAB_MAP_V1\n";
        out << rows_ << ' ' << cols_ << '\n';
        out << start_ << ' ' << goal_ << '\n';
        for (const auto& cell : cells_) {
            out << ToInt(cell.type) << ' ' << ToInt(cell.terrain) << ' ' << cell.height << '\n';
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool GridMap::LoadFromFile(const std::string& path, std::string* errorMessage) {
    std::ifstream in(path);
    if (!in) {
        if (errorMessage != nullptr) *errorMessage = "Cannot open file: " + path;
        return false;
    }

    std::string magic;
    std::getline(in, magic);
    if (magic != "PATHLAB_MAP_V1") {
        if (errorMessage != nullptr) *errorMessage = "Unsupported map file format.";
        return false;
    }

    int rows = 0;
    int cols = 0;
    int start = 0;
    int goal = 0;
    if (!(in >> rows >> cols >> start >> goal)) {
        if (errorMessage != nullptr) *errorMessage = "Corrupted map header.";
        return false;
    }

    rows = std::clamp(rows, kMinSize, kMaxRows);
    cols = std::clamp(cols, kMinSize, kMaxCols);
    std::vector<Cell> loaded(static_cast<size_t>(rows * cols));

    for (int i = 0; i < rows * cols; ++i) {
        int type = 0;
        int terrain = 0;
        int height = 0;
        if (!(in >> type >> terrain >> height)) {
            if (errorMessage != nullptr) *errorMessage = "Corrupted cell data.";
            return false;
        }
        loaded[static_cast<size_t>(i)].type = type == 1 ? CellType::Obstacle : CellType::Empty;
        loaded[static_cast<size_t>(i)].terrain = TerrainFromInt(terrain);
        loaded[static_cast<size_t>(i)].height = ClampHeight(height);
    }

    rows_ = rows;
    cols_ = cols;
    cells_ = std::move(loaded);
    start_ = std::clamp(start, 0, Count() - 1);
    goal_ = std::clamp(goal, 0, Count() - 1);
    EnsureEndpointsWalkable();
    return true;
}

bool GridMap::InBounds(int row, int col) const {
    return row >= 0 && row < rows_ && col >= 0 && col < cols_;
}

const Cell& GridMap::GetCell(int index) const {
    if (index < 0 || index >= Count()) throw std::out_of_range("Cell index out of range");
    return cells_[static_cast<size_t>(index)];
}

Cell& GridMap::GetCell(int index) {
    if (index < 0 || index >= Count()) throw std::out_of_range("Cell index out of range");
    return cells_[static_cast<size_t>(index)];
}

bool GridMap::IsObstacle(int index) const {
    if (index < 0 || index >= Count()) return true;
    return cells_[static_cast<size_t>(index)].type == CellType::Obstacle;
}

bool GridMap::IsWater(int index) const {
    if (index < 0 || index >= Count()) return false;
    return cells_[static_cast<size_t>(index)].terrain == TerrainType::Water;
}

bool GridMap::IsWalkable(int index) const {
    return index >= 0 && index < Count() && !IsObstacle(index) && !IsWater(index);
}

void GridMap::SetObstacle(int index, bool obstacle) {
    if (index < 0 || index >= Count()) return;
    if (index == start_ || index == goal_) return;
    cells_[static_cast<size_t>(index)].type = obstacle ? CellType::Obstacle : CellType::Empty;
    if (!obstacle && cells_[static_cast<size_t>(index)].terrain == TerrainType::Water) {
        cells_[static_cast<size_t>(index)].terrain = TerrainType::Plain;
    }
}

void GridMap::SetTerrain(int index, TerrainType terrain) {
    if (index < 0 || index >= Count()) return;
    cells_[static_cast<size_t>(index)].terrain = terrain;
    if (terrain == TerrainType::Water) cells_[static_cast<size_t>(index)].type = CellType::Obstacle;
    if (index == start_ || index == goal_) EnsureEndpointsWalkable();
}

void GridMap::SetHeight(int index, int height) {
    if (index < 0 || index >= Count()) return;
    cells_[static_cast<size_t>(index)].height = ClampHeight(height);
}

void GridMap::AddHeight(int index, int delta) {
    if (index < 0 || index >= Count()) return;
    cells_[static_cast<size_t>(index)].height = ClampHeight(cells_[static_cast<size_t>(index)].height + delta);
}

void GridMap::SetStart(int index) {
    if (!IsWalkable(index) || index == goal_) return;
    start_ = index;
}

void GridMap::SetGoal(int index) {
    if (!IsWalkable(index) || index == start_) return;
    goal_ = index;
}

std::vector<int> GridMap::Neighbors(int index, bool allowDiagonal) const {
    std::vector<int> result;
    GridCoord c = Coord(index);
    const int dr8[8] = { -1, 1, 0, 0, -1, -1, 1, 1 };
    const int dc8[8] = { 0, 0, -1, 1, -1, 1, -1, 1 };
    int count = allowDiagonal ? 8 : 4;

    for (int k = 0; k < count; ++k) {
        int nr = c.row + dr8[k];
        int nc = c.col + dc8[k];
        if (InBounds(nr, nc)) {
            int next = Index(nr, nc);
            if (!IsWalkable(next)) continue;

            if (k >= 4) {
                // Avoid cutting through two diagonal walls.
                int sideA = Index(c.row, nc);
                int sideB = Index(nr, c.col);
                if (!IsWalkable(sideA) || !IsWalkable(sideB)) continue;
            }
            result.push_back(next);
        }
    }
    return result;
}

double GridMap::BaseMoveCost(int from, int to) const {
    GridCoord a = Coord(from);
    GridCoord b = Coord(to);
    int dr = std::abs(a.row - b.row);
    int dc = std::abs(a.col - b.col);
    return (dr == 1 && dc == 1) ? std::sqrt(2.0) : 1.0;
}

double GridMap::TerrainCost(int index) const {
    if (index < 0 || index >= Count()) return 1000000.0;
    switch (cells_[static_cast<size_t>(index)].terrain) {
        case TerrainType::Road: return -0.25;
        case TerrainType::Forest: return 0.8;
        case TerrainType::Mountain: return 1.7;
        case TerrainType::Water: return 1000000.0;
        case TerrainType::Plain:
        default: return 0.0;
    }
}

double GridMap::StepCost(int from, int to, double terrainWeight, double heightWeight) const {
    if (!IsWalkable(from) || !IsWalkable(to)) return 1000000.0;
    double base = BaseMoveCost(from, to);
    double terrain = std::max(0.0, terrainWeight * TerrainCost(to));
    int h1 = cells_[static_cast<size_t>(from)].height;
    int h2 = cells_[static_cast<size_t>(to)].height;
    double height = std::abs(h1 - h2) * heightWeight;
    return base + terrain + height;
}

int GridMap::PickRandomFreeCell(std::mt19937& rng) const {
    std::uniform_int_distribution<int> dist(0, Count() - 1);
    for (int attempt = 0; attempt < 10000; ++attempt) {
        int candidate = dist(rng);
        if (IsWalkable(candidate)) return candidate;
    }
    for (int i = 0; i < Count(); ++i) {
        if (IsWalkable(i)) return i;
    }
    return 0;
}

void GridMap::EnsureEndpointsWalkable() {
    if (start_ < 0 || start_ >= Count()) start_ = 0;
    if (goal_ < 0 || goal_ >= Count()) goal_ = Count() - 1;
    auto makeWalkable = [&](int index) {
        cells_[static_cast<size_t>(index)].type = CellType::Empty;
        if (cells_[static_cast<size_t>(index)].terrain == TerrainType::Water) {
            cells_[static_cast<size_t>(index)].terrain = TerrainType::Plain;
        }
    };
    makeWalkable(start_);
    makeWalkable(goal_);
    if (start_ == goal_) {
        goal_ = std::clamp(Index(rows_ / 2, cols_ * 4 / 5), 0, Count() - 1);
        makeWalkable(goal_);
    }
}

} // namespace pathlab
