#include "Pathfinder.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>

namespace pathlab {

namespace {
constexpr double INF = std::numeric_limits<double>::infinity();

struct QueueItem {
    double priority = 0.0;
    int node = -1;

    bool operator>(const QueueItem& other) const {
        if (priority == other.priority) return node > other.node;
        return priority > other.priority;
    }
};

std::vector<int> CollectIndices(const std::vector<unsigned char>& flags) {
    std::vector<int> result;
    result.reserve(flags.size());
    for (int i = 0; i < static_cast<int>(flags.size()); ++i) {
        if (flags[static_cast<size_t>(i)] != 0) result.push_back(i);
    }
    return result;
}

bool UsesUnitCost(AlgorithmKind algorithm) {
    return algorithm == AlgorithmKind::BFS;
}

bool UsesGreedyPriority(AlgorithmKind algorithm) {
    return algorithm == AlgorithmKind::GreedyBestFirst;
}

bool UsesWeightedAStar(AlgorithmKind algorithm) {
    return algorithm == AlgorithmKind::WeightedAStar;
}
} // namespace

SearchResult Pathfinder::Run(const GridMap& map, AlgorithmKind algorithm, const AlgorithmOptions& options) {
    auto t0 = std::chrono::steady_clock::now();

    const int n = map.Count();
    const int start = map.Start();
    const int goal = map.Goal();
    const bool forceUnitCost = UsesUnitCost(algorithm);

    SearchResult result;
    result.algorithm = algorithm;
    result.algorithmName = AlgorithmName(algorithm);

    std::vector<double> dist(static_cast<size_t>(n), INF);
    std::vector<int> parent(static_cast<size_t>(n), -1);
    std::vector<unsigned char> open(static_cast<size_t>(n), 0);
    std::vector<unsigned char> closed(static_cast<size_t>(n), 0);

    auto pushFrame = [&](int step, int current, bool finished, bool found, const std::vector<int>& path) {
        SearchFrame frame;
        frame.step = step;
        frame.current = current;
        frame.open = CollectIndices(open);
        frame.closed = CollectIndices(closed);
        frame.path = path;
        frame.finished = finished;
        frame.found = found;
        result.frames.push_back(std::move(frame));
    };

    if (!map.IsWalkable(start, options.profile) || !map.IsWalkable(goal, options.profile)) {
        result.elapsedMs = 0.0;
        pushFrame(0, -1, true, false, {});
        return result;
    }

    dist[static_cast<size_t>(start)] = 0.0;
    open[static_cast<size_t>(start)] = 1;
    result.generatedNodes = 1;
    pushFrame(0, start, false, false, {});

    int step = 0;
    bool found = false;

    if (algorithm == AlgorithmKind::BFS) {
        std::queue<int> q;
        q.push(start);

        while (!q.empty()) {
            int current = q.front();
            q.pop();

            if (closed[static_cast<size_t>(current)] != 0) continue;
            open[static_cast<size_t>(current)] = 0;
            closed[static_cast<size_t>(current)] = 1;
            ++result.expandedNodes;
            ++step;

            if (current == goal) {
                found = true;
                break;
            }

            for (int next : map.Neighbors(current, options.allowDiagonal, options.profile)) {
                if (closed[static_cast<size_t>(next)] != 0 || open[static_cast<size_t>(next)] != 0) continue;
                parent[static_cast<size_t>(next)] = current;
                dist[static_cast<size_t>(next)] = dist[static_cast<size_t>(current)] + 1.0;
                q.push(next);
                open[static_cast<size_t>(next)] = 1;
                ++result.generatedNodes;
            }
            pushFrame(step, current, false, false, {});
        }
    } else {
        std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<QueueItem>> pq;
        double startPriority = 0.0;
        if (algorithm == AlgorithmKind::AStar || algorithm == AlgorithmKind::GreedyBestFirst || algorithm == AlgorithmKind::WeightedAStar) {
            startPriority = Heuristic(map, start, goal, options.allowDiagonal);
            if (UsesWeightedAStar(algorithm)) startPriority *= std::max(1.0, options.heuristicWeight);
        }
        pq.push({ startPriority, start });

        while (!pq.empty()) {
            QueueItem item = pq.top();
            pq.pop();
            int current = item.node;

            if (current < 0 || current >= n) continue;
            if (closed[static_cast<size_t>(current)] != 0) continue;

            open[static_cast<size_t>(current)] = 0;
            closed[static_cast<size_t>(current)] = 1;
            ++result.expandedNodes;
            ++step;

            if (current == goal) {
                found = true;
                break;
            }

            for (int next : map.Neighbors(current, options.allowDiagonal, options.profile)) {
                if (closed[static_cast<size_t>(next)] != 0) continue;

                double stepCost = map.StepCost(current, next, options.terrainWeight, options.heightWeight, options.profile);
                double tentative = dist[static_cast<size_t>(current)] + stepCost;
                if (tentative < dist[static_cast<size_t>(next)]) {
                    dist[static_cast<size_t>(next)] = tentative;
                    parent[static_cast<size_t>(next)] = current;

                    double h = Heuristic(map, next, goal, options.allowDiagonal);
                    double priority = tentative;
                    if (algorithm == AlgorithmKind::AStar) {
                        priority = tentative + h;
                    } else if (UsesWeightedAStar(algorithm)) {
                        priority = tentative + std::max(1.0, options.heuristicWeight) * h;
                    } else if (UsesGreedyPriority(algorithm)) {
                        priority = h;
                    }

                    pq.push({ priority, next });
                    if (open[static_cast<size_t>(next)] == 0) {
                        open[static_cast<size_t>(next)] = 1;
                        ++result.generatedNodes;
                    }
                }
            }
            pushFrame(step, current, false, false, {});
        }
    }

    result.found = found;
    if (found) {
        result.finalPath = ReconstructPath(parent, start, goal);
        result.pathLength = ComputePathCost(map, result.finalPath, options, forceUnitCost);
    }

    auto t1 = std::chrono::steady_clock::now();
    result.elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

    pushFrame(step + 1, found ? goal : -1, true, found, result.finalPath);
    return result;
}

std::string Pathfinder::AlgorithmName(AlgorithmKind algorithm) {
    switch (algorithm) {
        case AlgorithmKind::BFS: return "BFS";
        case AlgorithmKind::Dijkstra: return "Dijkstra";
        case AlgorithmKind::AStar: return "A*";
        case AlgorithmKind::GreedyBestFirst: return "Greedy best-first";
        case AlgorithmKind::WeightedAStar: return "Weighted A*";
        default: return "Unknown";
    }
}

double Pathfinder::Heuristic(const GridMap& map, int a, int b, bool allowDiagonal) {
    GridCoord ca = map.Coord(a);
    GridCoord cb = map.Coord(b);
    const double dx = static_cast<double>(std::abs(ca.col - cb.col));
    const double dy = static_cast<double>(std::abs(ca.row - cb.row));
    if (!allowDiagonal) return dx + dy;

    // Octile distance for 8-neighbor grids.
    const double diagonal = std::min(dx, dy);
    const double straight = std::max(dx, dy) - diagonal;
    return std::sqrt(2.0) * diagonal + straight;
}

double Pathfinder::ComputePathCost(const GridMap& map, const std::vector<int>& path, const AlgorithmOptions& options, bool forceUnitCost) {
    if (path.size() < 2) return 0.0;
    double total = 0.0;
    for (size_t i = 1; i < path.size(); ++i) {
        if (forceUnitCost) {
            total += map.BaseMoveCost(path[i - 1], path[i]);
        } else {
            total += map.StepCost(path[i - 1], path[i], options.terrainWeight, options.heightWeight, options.profile);
        }
    }
    return total;
}

std::vector<int> Pathfinder::ReconstructPath(const std::vector<int>& parent, int start, int goal) {
    std::vector<int> path;
    int current = goal;
    while (current != -1) {
        path.push_back(current);
        if (current == start) break;
        current = parent[static_cast<size_t>(current)];
    }
    if (path.empty() || path.back() != start) return {};
    std::reverse(path.begin(), path.end());
    return path;
}

} // namespace pathlab
