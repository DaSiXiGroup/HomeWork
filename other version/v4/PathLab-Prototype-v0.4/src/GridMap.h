#pragma once

#include <random>
#include <string>
#include <vector>

namespace pathlab{

constexpr int       DEFAULT_MAP_ROWS        = 35;
constexpr int       DEFAULT_MAP_COLS        = 55;
constexpr int       MIN_CELL_HEIGHT         = 0;
constexpr int       MAX_CELL_HEIGHT         = 9;
constexpr double    DEFAULT_PLAIN_COST      = 0.0;
enum class CellType{ Empty, Obstacle };
enum class TerrainType{ Plain, Road, Forest, Water, Mountain };

struct Cell;

class Terrain{
public:
    virtual ~Terrain() = default;

    [[nodiscard]] virtual TerrainType  Kind()          const = 0;
    [[nodiscard]] virtual const char*  Name()          const = 0;
    [[nodiscard]] virtual int          SaveId()        const = 0;
    [[nodiscard]] virtual double       CostDelta()     const = 0;
    [[nodiscard]] virtual bool         BlocksMovement()const = 0;

    void Apply(Cell& cell) const;
};

class TerrainCatalog{
public:
    [[nodiscard]] static const Terrain& Get(TerrainType terrain);
    [[nodiscard]] static const Terrain& FromSaveId(int id);
    [[nodiscard]] static TerrainType    TypeFromSaveId(int id);
    [[nodiscard]] static int            SaveId(TerrainType terrain);
    [[nodiscard]] static const char*    Name(TerrainType terrain);
    [[nodiscard]] static double         CostDelta(TerrainType terrain);
    [[nodiscard]] static bool           BlocksMovement(TerrainType terrain);
};

struct GridCoord{ int row = 0, col = 0; };

struct Cell{
    CellType    type    = CellType::Empty;
    TerrainType terrain = TerrainType::Plain;
    int         height  = MIN_CELL_HEIGHT; // simple 2.5D elevation; not real GIS, just useful.
};

class GridMap{
public:
    GridMap(int rows = DEFAULT_MAP_ROWS, int cols = DEFAULT_MAP_COLS);

    void Resize(int rows, int cols);
    void ClearAll();
    void ClearObstacles();
    void ResetTerrain();

    void GenerateRandom(float obstacleProbability, unsigned seed = std::random_device{}());
    void GenerateTerrain(unsigned seed = std::random_device{}());
    void GenerateMaze(unsigned seed = std::random_device{}());

    bool SaveToFile(const std::string& path) const;
    bool LoadFromFile(const std::string& path, std::string* errorMessage = nullptr);

    [[nodiscard]] int Rows()  const { return rows; }
    [[nodiscard]] int Cols()  const { return cols; }
    [[nodiscard]] int Count() const { return rows * cols; }

    [[nodiscard]] bool      InBounds(int row, int col) const;
    [[nodiscard]] int       Index(int row, int col) const { return row * cols + col; }
    [[nodiscard]] GridCoord Coord(int index) const { return { index / cols, index % cols }; }

    [[nodiscard]] const Cell& GetCell(int index) const;
    [[nodiscard]] Cell&       GetCell(int index);

    [[nodiscard]] bool IsObstacle(int index) const;
    [[nodiscard]] bool IsWater(int index) const;
    [[nodiscard]] bool IsWalkable(int index) const;

    void SetObstacle(int index, bool obstacle);
    void SetTerrain(int index, TerrainType terrain);
    void SetHeight(int index, int height);
    void AddHeight(int index, int delta);

    [[nodiscard]] int Start() const { return start; }
    [[nodiscard]] int Goal()  const { return goal; }
    void SetStart(int index);
    void SetGoal(int index);

    [[nodiscard]] std::vector<int> Neighbors(int index, bool allowDiagonal) const;
    [[nodiscard]] double BaseMoveCost(int from, int to) const;
    [[nodiscard]] double TerrainCost(int index) const;
    [[nodiscard]] double StepCost(int from, int to, double terrainWeight, double heightWeight) const;

private:
    int rows = DEFAULT_MAP_ROWS;
    int cols = DEFAULT_MAP_COLS;
    std::vector<Cell> cells;
    int start = 0;
    int goal  = 0;

    [[nodiscard]] int  PickRandomFreeCell(std::mt19937& rng) const;
    void EnsureEndpointsWalkable();
};

}
