#pragma once

#include "GridMap.h"

#include <string>
#include <vector>

namespace pathlab{

enum class AlgorithmKind{
    BFS,
    Dijkstra,
    AStar,
    GreedyBestFirst,
    WeightedAStar
};

struct AlgorithmOptions{
    bool    allowDiagonal   = false;
    double  heuristicWeight = 1.4;
    double  terrainWeight   = 1.0;
    double  heightWeight    = 0.25;
    MovementProfile profile = MovementProfile::Pedestrian;
};

struct SearchFrame{
    int step    = 0;
    int current = -1;
    std::vector<int> open;
    std::vector<int> closed;
    std::vector<int> path;
    bool finished   = false;
    bool found      = false;
};

struct SearchResult{
    AlgorithmKind algorithm = AlgorithmKind::Dijkstra;
    std::string algorithmName;
    std::vector<SearchFrame> frames;
    std::vector<int> finalPath;
    bool    found           = false;
    double  pathLength      = 0.0;
    int     expandedNodes   = 0;
    int     generatedNodes  = 0;
    double  elapsedMs       = 0.0;
};

class SearchAlgorithm{
public:
    virtual ~SearchAlgorithm() = default;
    [[nodiscard]] virtual AlgorithmKind Kind() const = 0;
    [[nodiscard]] virtual const char*   Name() const = 0;
    [[nodiscard]] virtual SearchResult  Run(const GridMap& map, const AlgorithmOptions& options) const = 0;
};

class Pathfinder{
public:
    static SearchResult Run(const GridMap& map, AlgorithmKind algorithm, const AlgorithmOptions& options = {});
    static std::string  AlgorithmName(AlgorithmKind algorithm);

private:
    // Keep the registry hidden; the UI only needs names and results, not the backstage drama.
    static const SearchAlgorithm& __Algorithm(AlgorithmKind algorithm);
};

}
