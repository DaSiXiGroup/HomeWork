#pragma once

#include <random>
#include <string>
#include <vector>

namespace pathlab{

enum class CellType{
    Empty,
    Obstacle
};

enum class TerrainType{
    Plain,
    Road,
    Forest,
    Water,
    Mountain
};

enum class MovementProfile{
    Pedestrian,
    Car,
    Offroad,
    Drone
};

struct GridCoord{
    int row = 0;
    int col = 0;
};

struct Cell{
    CellType type = CellType::Empty;
    TerrainType terrain = TerrainType::Plain;
    int height = 0;
};

class GridMap{
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

    // I don't like [[nodiscard]] after all, but Visual Studio advise me to do so
    [[nodiscard]] int Rows()    const{ return __rows; }
    [[nodiscard]] int Cols()    const{ return __cols; }
    [[nodiscard]] int Count()   const{ return __rows * __cols; }

    [[nodiscard]] bool      InBounds(int row, int col)  const;
    [[nodiscard]] int       Index(int row, int col)     const{ return row * __cols + col; }
    [[nodiscard]] GridCoord Coord(int index)            const{ return { index / __cols, index % __cols }; }

    [[nodiscard]] const Cell&   GetCell(int index) const;
    [[nodiscard]] Cell&         GetCell(int index);

    [[nodiscard]] bool IsObstacle(int index)                            const;
    [[nodiscard]] bool IsWater(int index)                               const;
    [[nodiscard]] bool IsWalkable(int index)                            const;
    [[nodiscard]] bool IsWalkable(int index, MovementProfile profile)   const;

    void SetObstacle(int index, bool obstacle);
    void SetTerrain(int index, TerrainType terrain);
    void SetHeight(int index, int height);
    void AddHeight(int index, int delta);

    [[nodiscard]] int Start() const{ return __start; }
    [[nodiscard]] int Goal() const{ return __goal; }
    void SetStart(int index);
    void SetGoal(int index);

    [[nodiscard]] std::vector<int> Neighbors(int index, bool allowDiagonal)                                             const;
    [[nodiscard]] std::vector<int> Neighbors(int index, bool allowDiagonal, MovementProfile profile)                    const;
    [[nodiscard]] double BaseMoveCost(int from, int to)                                                                 const;
    [[nodiscard]] double TerrainCost(int index)                                                                         const;
    [[nodiscard]] double StepCost(int from, int to, double terrainWeight, double heightWeight)                          const;
    [[nodiscard]] double StepCost(int from, int to, double terrainWeight, double heightWeight, MovementProfile profile) const;

private:
    int __rows = 35;
    int __cols = 55;
    std::vector<Cell> __cells;
    int __start = 0;
    int __goal = 0;

    [[nodiscard]] bool __ValidIndex(int index) const;
    [[nodiscard]] int __DefaultStart() const;
    [[nodiscard]] int __DefaultGoal() const;
    [[nodiscard]] int __PickRandomFreeCell(std::mt19937& rng) const;
    void __ResetEndpoints();
    void __EnsureEndpointsWalkable();
};

}
