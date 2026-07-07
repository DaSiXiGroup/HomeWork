#pragma once

#include "GridMap.h"

#include <string>
#include <vector>

namespace pathlab {

enum class AlgorithmKind {
    BFS,
    Dijkstra,
    AStar,
    GreedyBestFirst,
    WeightedAStar
};

struct AlgorithmOptions {
    bool allowDiagonal = false;
    double heuristicWeight = 1.4;
    double terrainWeight = 1.0;
    double heightWeight = 0.25;
};

struct SearchFrame {
    int step = 0;
    int current = -1;
    std::vector<int> open;
    std::vector<int> closed;
    std::vector<int> path;
    bool finished = false;
    bool found = false;
};

struct SearchResult {
    AlgorithmKind algorithm = AlgorithmKind::Dijkstra;
    std::string algorithmName;
    std::vector<SearchFrame> frames;
    std::vector<int> finalPath;
    bool found = false;
    double pathLength = 0.0;
    int expandedNodes = 0;
    int generatedNodes = 0;
    double elapsedMs = 0.0;
};

class Pathfinder {
public:
    static SearchResult Run(const GridMap& map, AlgorithmKind algorithm, const AlgorithmOptions& options = {});
    static std::string AlgorithmName(AlgorithmKind algorithm);

private:
    static double Heuristic(const GridMap& map, int a, int b, bool allowDiagonal);
    static double ComputePathCost(const GridMap& map, const std::vector<int>& path, const AlgorithmOptions& options, bool forceUnitCost);
    static std::vector<int> ReconstructPath(const std::vector<int>& parent, int start, int goal);
};

} // namespace pathlab
