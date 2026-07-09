#pragma once

#include <random>
#include <string>
#include <vector>

namespace pathlab{

enum class CellKind{
    Empty,
    Obstacle
};

enum class Terrain{
    Plain,
    Road,
    Forest,
    Water,
    Mountain
};

enum class MoveProfile{
    Pedestrian,
    Car,
    Offroad,
    Drone
};

// Old objs may still ask for this name after the rename. VS linker is a fossil dig site sometimes.
enum class MovementProfile{
    Pedestrian,
    Car,
    Offroad,
    Drone
};

[[nodiscard]] constexpr MoveProfile ToMoveProfile(MovementProfile prof){
    switch(prof){
        case MovementProfile::Car:     return MoveProfile::Car;
        case MovementProfile::Offroad: return MoveProfile::Offroad;
        case MovementProfile::Drone:   return MoveProfile::Drone;
        case MovementProfile::Pedestrian:
        default:                       return MoveProfile::Pedestrian;
    }
}

struct GPos{
    int row = 0;
    int col = 0;
};

struct Cell{
    CellKind type = CellKind::Empty;
    Terrain terrain = Terrain::Plain;
    int height = 0;
};

class GridMap{
public:
    GridMap(int rows = 35, int cols = 55);

    void Resize(int rows, int cols);
    void ClearAll();
    void ClearObstacles();
    void ResetTerrain();

    void GenerateRandom(float obsProb, unsigned seed = std::random_device{}());
    void GenerateTerrain(unsigned seed = std::random_device{}());
    void GenerateMaze(unsigned seed = std::random_device{}());

    bool SaveToFile(const std::string& path) const;
    bool LoadFromFile(const std::string& path, std::string* errMsg = nullptr);

    // I don't like [[nodiscard]] after all, but Visual Studio advise me to do so
    [[nodiscard]] int Rows()    const{ return __row; }
    [[nodiscard]] int Cols()    const{ return __col; }
    [[nodiscard]] int Count()   const{ return __row * __col; }

    [[nodiscard]] bool      InBounds(int row, int col)  const;
    [[nodiscard]] int       Index(int row, int col)     const{ return row * __col + col; }
    [[nodiscard]] GPos Coord(int index)            const{ return { index / __col, index % __col }; }

    [[nodiscard]] const Cell&   GetCell(int index) const;
    [[nodiscard]] Cell&         GetCell(int index);

    [[nodiscard]] bool IsObstacle(int index)                            const;
    [[nodiscard]] bool IsWater(int index)                               const;
    [[nodiscard]] bool IsWalkable(int index)                            const;
    [[nodiscard]] bool IsWalkable(int index, MoveProfile prof)   const;
    [[nodiscard]] bool IsWalkable(int index, MovementProfile prof) const;

    void SetObstacle(int index, bool obstacle);
    void SetTerrain(int index, Terrain terrain);
    void SetHeight(int index, int height);
    void AddHeight(int index, int delta);

    [[nodiscard]] int Start() const{ return __start; }
    [[nodiscard]] int Goal() const{ return __goal; }
    void SetStart(int index);
    void SetGoal(int index);

    [[nodiscard]] std::vector<int> Neighbors(int index, bool diag)                                             const;
    [[nodiscard]] std::vector<int> Neighbors(int index, bool diag, MoveProfile prof)                    const;
    [[nodiscard]] std::vector<int> Neighbors(int index, bool diag, MovementProfile prof)                const;
    [[nodiscard]] double BaseMoveCost(int from, int to)                                                                 const;
    [[nodiscard]] double TerrainCost(int index)                                                                         const;
    [[nodiscard]] double StepCost(int from, int to, double terrW, double hgtW)                          const;
    [[nodiscard]] double StepCost(int from, int to, double terrW, double hgtW, MoveProfile prof) const;
    [[nodiscard]] double StepCost(int from, int to, double terrW, double hgtW, MovementProfile prof) const;

private:
    int __row = 35;
    int __col = 55;
    std::vector<Cell> __cell;
    int __start = 0;
    int __goal = 0;

    [[nodiscard]] bool __ValidIndex(int index) const;
    [[nodiscard]] int __DefStart() const;
    [[nodiscard]] int __DefGoal() const;
    [[nodiscard]] int __PickFree(std::mt19937& rng) const;
    void __ResetEnd();
    void __FixEnd();
};

}
