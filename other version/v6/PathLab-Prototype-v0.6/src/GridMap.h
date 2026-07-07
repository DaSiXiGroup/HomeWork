#pragma once

#include <random>
#include <string>
#include <vector>

namespace pathlab {

enum class CellType {
    Empty,
    Obstacle
};

enum class TerrainType {
    Plain,
    Road,
    Forest,
    Water,
    Mountain
};

struct GridCoord {
    int row = 0;
    int col = 0;
};

struct Cell {
    CellType type = CellType::Empty;
    TerrainType terrain = TerrainType::Plain;
    int height = 0; // 0 - 9. Elevation is independent of terrain category.
};

class GridMap {
public:
    GridMap(int rows = 35, int cols = 55);

    void Resize(int rows, int cols);
    void ClearAll();
    void ClearObstacles();
    void ResetTerrain();

    void GenerateRandom(float obstacleProbability, unsigned seed = std::random_device{}());
    void GenerateTerrain(unsigned seed = std::random_device{}());
    void GenerateMaze(unsigned seed = std::random_device{}());

    bool SaveToFile(const std::string& path) const;
    bool LoadFromFile(const std::string& path, std::string* errorMessage = nullptr);

    [[nodiscard]] int Rows() const { return rows_; }
    [[nodiscard]] int Cols() const { return cols_; }
    [[nodiscard]] int Count() const { return rows_ * cols_; }

    [[nodiscard]] bool InBounds(int row, int col) const;
    [[nodiscard]] int Index(int row, int col) const { return row * cols_ + col; }
    [[nodiscard]] GridCoord Coord(int index) const { return { index / cols_, index % cols_ }; }

    [[nodiscard]] const Cell& GetCell(int index) const;
    [[nodiscard]] Cell& GetCell(int index);

    [[nodiscard]] bool IsObstacle(int index) const;
    [[nodiscard]] bool IsWater(int index) const;
    [[nodiscard]] bool IsWalkable(int index) const;

    void SetObstacle(int index, bool obstacle);
    void SetTerrain(int index, TerrainType terrain);
    void SetHeight(int index, int height);
    void AddHeight(int index, int delta);

    [[nodiscard]] int Start() const { return start_; }
    [[nodiscard]] int Goal() const { return goal_; }
    void SetStart(int index);
    void SetGoal(int index);

    [[nodiscard]] std::vector<int> Neighbors(int index, bool allowDiagonal) const;
    [[nodiscard]] double BaseMoveCost(int from, int to) const;
    [[nodiscard]] double TerrainCost(int index) const;
    [[nodiscard]] double StepCost(int from, int to, double terrainWeight, double heightWeight) const;

private:
    int rows_ = 35;
    int cols_ = 55;
    std::vector<Cell> cells_;
    int start_ = 0;
    int goal_ = 0;

    int PickRandomFreeCell(std::mt19937& rng) const;
    void EnsureEndpointsWalkable();
};

} // namespace pathlab
