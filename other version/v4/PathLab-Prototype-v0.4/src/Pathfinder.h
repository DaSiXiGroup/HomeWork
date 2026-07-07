#pragma once

#include "GridMap.h"

#include <string>
#include <vector>

namespace pathlab{

constexpr double DEFAULT_HEURISTIC_WEIGHT = 1.4;
constexpr double DEFAULT_TERRAIN_WEIGHT   = 1.0;
constexpr double DEFAULT_HEIGHT_WEIGHT    = 0.25;

enum class AlgorithmKind{ BFS, Dijkstra, AStar, GreedyBestFirst, WeightedAStar };

struct AlgorithmOptions{
    bool   allowDiagonal   = false;
    double heuristicWeight = DEFAULT_HEURISTIC_WEIGHT;
    double terrainWeight   = DEFAULT_TERRAIN_WEIGHT;
    double heightWeight    = DEFAULT_HEIGHT_WEIGHT;
};

struct SearchFrame{
    int step    = 0;
    int current = -1;

    std::vector<int> open;
    std::vector<int> closed;
    std::vector<int> path;

    bool finished = false;
    bool found    = false;
};

struct SearchResult{
    AlgorithmKind algorithm = AlgorithmKind::Dijkstra;
    std::string algorithmName;

    std::vector<SearchFrame> frames;
    std::vector<int> finalPath;

    bool   found          = false;
    double pathLength     = 0.0;
    int    expandedNodes  = 0;
    int    generatedNodes = 0;
    double elapsedMs      = 0.0;
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
    [[nodiscard]] static SearchResult           Run(const GridMap& map, AlgorithmKind algorithm, const AlgorithmOptions& options = {});
    [[nodiscard]] static std::string            AlgorithmName(AlgorithmKind algorithm);
    [[nodiscard]] static const SearchAlgorithm& Algorithm(AlgorithmKind algorithm);
};

}
