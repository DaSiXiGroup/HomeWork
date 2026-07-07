#include "Pathfinder.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>

namespace pathlab {

    namespace {

        using Clock = std::chrono::steady_clock;

        constexpr int    NO_NODE                = -1;
        constexpr double INF_DIST               = std::numeric_limits<double>::infinity();
        constexpr double BFS_EDGE_COST          = 1.0;
        constexpr double SQRT2_COST             = 1.4142135623730950488;
        constexpr double MIN_HEURISTIC_WEIGHT   = 1.0;

        struct HeapNode {
            double score = 0.0;
            int node = NO_NODE;

            bool operator>(const HeapNode& other) const {
                return score == other.score ? node > other.node : score > other.score;
            }
        };

        std::vector<int> PickMarked(const std::vector<char>& mark) {
            std::vector<int> nodes;
            nodes.reserve(mark.size());
            for (int i = 0, n = static_cast<int>(mark.size()); i < n; ++i) {
                if (mark[static_cast<size_t>(i)]) nodes.push_back(i);
            }
            return nodes;
        }

        double Heuristic(const GridMap& map, int a, int b, bool allowDiagonal) {
            const GridCoord ca = map.Coord(a);
            const GridCoord cb = map.Coord(b);
            const double dx = static_cast<double>(std::abs(ca.col - cb.col));
            const double dy = static_cast<double>(std::abs(ca.row - cb.row));

            if (!allowDiagonal) return dx + dy;
            const double diagonal = std::min(dx, dy);
            const double straight = std::max(dx, dy) - diagonal;
            return diagonal * SQRT2_COST + straight;
        }

        std::vector<int> MakePath(const std::vector<int>& parent, int start, int goal) {
            std::vector<int> path;
            for (int u = goal; u != NO_NODE; u = parent[static_cast<size_t>(u)]) {
                path.push_back(u);
                if (u == start) break;
            }
            if (path.empty() || path.back() != start) return {};
            std::reverse(path.begin(), path.end());
            return path;
        }

        double PathCost(const GridMap& map, const std::vector<int>& path, const AlgorithmOptions& options, bool useBaseCost) {
            constexpr size_t MIN_PATH_NODES = 2;
            if (path.size() < MIN_PATH_NODES) return 0.0;

            double cost = 0.0;
            for (size_t i = 1; i < path.size(); ++i) {
                const int u = path[i - 1], v = path[i];
                cost += useBaseCost ? map.BaseMoveCost(u, v)
                    : map.StepCost(u, v, options.terrainWeight, options.heightWeight);
            }
            return cost;
        }

        struct SearchState {
            explicit SearchState(int nodeCount)
                : dist(static_cast<size_t>(nodeCount), INF_DIST),
                parent(static_cast<size_t>(nodeCount), NO_NODE),
                open(static_cast<size_t>(nodeCount), 0),
                closed(static_cast<size_t>(nodeCount), 0) {
            }

            std::vector<double> dist;
            std::vector<int> parent;
            std::vector<char> open, closed;
            int step = 0;

            void SaveFrame(SearchResult& result, int current, bool finished, bool found, const std::vector<int>& path = {}) const {
                SearchFrame frame;
                frame.step = step;
                frame.current = current;
                frame.open = PickMarked(open);
                frame.closed = PickMarked(closed);
                frame.path = path;
                frame.finished = finished;
                frame.found = found;
                result.frames.push_back(std::move(frame));
            }
        };

        SearchResult EmptyResult(const SearchAlgorithm& algorithm) {
            SearchResult result;
            result.algorithm = algorithm.Kind();
            result.algorithmName = algorithm.Name();
            return result;
        }

        class BfsAlgorithm final : public SearchAlgorithm {
        public:
            [[nodiscard]] AlgorithmKind Kind() const override   { return AlgorithmKind::BFS; }
            [[nodiscard]] const char* Name() const override     { return "BFS"; }

            [[nodiscard]] SearchResult Run(const GridMap& map, const AlgorithmOptions& options) const override {
                const auto begin = Clock::now();
                SearchResult result = EmptyResult(*this);
                SearchState state(map.Count());

                const int start = map.Start(), goal = map.Goal();
                if (!map.IsWalkable(start) || !map.IsWalkable(goal)) {
                    result.elapsedMs = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
                    state.SaveFrame(result, NO_NODE, true, false);
                    return result;
                }

                std::queue<int> q;
                q.push(start);
                state.dist[static_cast<size_t>(start)] = 0.0;
                state.open[static_cast<size_t>(start)] = 1;
                result.generatedNodes = 1;
                state.SaveFrame(result, start, false, false);

                while (!q.empty()) {
                    const int u = q.front(); q.pop();
                    if (state.closed[static_cast<size_t>(u)]) continue;

                    state.open[static_cast<size_t>(u)] = 0;
                    state.closed[static_cast<size_t>(u)] = 1;
                    ++state.step;
                    ++result.expandedNodes;

                    if (u == goal) break;

                    for (const int v : map.Neighbors(u, options.allowDiagonal)) {
                        if (state.open[static_cast<size_t>(v)] || state.closed[static_cast<size_t>(v)]) continue;
                        state.parent[static_cast<size_t>(v)] = u;
                        state.dist[static_cast<size_t>(v)] = state.dist[static_cast<size_t>(u)] + BFS_EDGE_COST;
                        state.open[static_cast<size_t>(v)] = 1;
                        q.push(v);
                        ++result.generatedNodes;
                    }
                    state.SaveFrame(result, u, false, false);
                }

                result.finalPath = MakePath(state.parent, start, goal);
                result.found = !result.finalPath.empty();
                if (result.found) result.pathLength = PathCost(map, result.finalPath, options, true);

                result.elapsedMs = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
                ++state.step;
                state.SaveFrame(result, result.found ? goal : NO_NODE, true, result.found, result.finalPath);
                return result;
            }
        };

        class PrioritySearchAlgorithm : public SearchAlgorithm {
        public:
            [[nodiscard]] SearchResult Run(const GridMap& map, const AlgorithmOptions& options) const override {
                const auto begin = Clock::now();
                SearchResult result = EmptyResult(*this);
                SearchState state(map.Count());

                const int start = map.Start(), goal = map.Goal();
                if (!map.IsWalkable(start) || !map.IsWalkable(goal)) {
                    result.elapsedMs = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
                    state.SaveFrame(result, NO_NODE, true, false);
                    return result;
                }

                std::priority_queue<HeapNode, std::vector<HeapNode>, std::greater<HeapNode>> heap;
                heap.push({ Priority(0.0, Heuristic(map, start, goal, options.allowDiagonal), options), start });
                state.dist[static_cast<size_t>(start)] = 0.0;
                state.open[static_cast<size_t>(start)] = 1;
                result.generatedNodes = 1;
                state.SaveFrame(result, start, false, false);

                while (!heap.empty()) {
                    const int u = heap.top().node; heap.pop();
                    if (state.closed[static_cast<size_t>(u)]) continue;

                    state.open[static_cast<size_t>(u)] = 0;
                    state.closed[static_cast<size_t>(u)] = 1;
                    ++state.step;
                    ++result.expandedNodes;

                    if (u == goal) break;

                    for (const int v : map.Neighbors(u, options.allowDiagonal)) {
                        if (state.closed[static_cast<size_t>(v)]) continue;

                        const double newDist = state.dist[static_cast<size_t>(u)]
                            + map.StepCost(u, v, options.terrainWeight, options.heightWeight);
                        if (newDist >= state.dist[static_cast<size_t>(v)]) continue;

                        state.dist[static_cast<size_t>(v)] = newDist;
                        state.parent[static_cast<size_t>(v)] = u;
                        heap.push({ Priority(newDist, Heuristic(map, v, goal, options.allowDiagonal), options), v });

                        if (!state.open[static_cast<size_t>(v)]) {
                            state.open[static_cast<size_t>(v)] = 1;
                            ++result.generatedNodes;
                        }
                    }
                    state.SaveFrame(result, u, false, false);
                }

                result.finalPath = MakePath(state.parent, start, goal);
                result.found = !result.finalPath.empty();
                if (result.found) result.pathLength = PathCost(map, result.finalPath, options, false);

                result.elapsedMs = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
                ++state.step;
                state.SaveFrame(result, result.found ? goal : NO_NODE, true, result.found, result.finalPath);
                return result;
            }

        private:
            [[nodiscard]] virtual double Priority(double dist, double heuristic, const AlgorithmOptions& options) const = 0;
        };

        class DijkstraAlgorithm final : public PrioritySearchAlgorithm {
        public:
            [[nodiscard]] AlgorithmKind Kind() const override { return AlgorithmKind::Dijkstra; }
            [[nodiscard]] const char* Name() const override { return "Dijkstra"; }

        private:
            [[nodiscard]] double Priority(double dist, double, const AlgorithmOptions&) const override { return dist; }
        };

        class AStarAlgorithm final : public PrioritySearchAlgorithm {
        public:
            [[nodiscard]] AlgorithmKind Kind() const override { return AlgorithmKind::AStar; }
            [[nodiscard]] const char* Name() const override { return "A*"; }

        private:
            [[nodiscard]] double Priority(double dist, double heuristic, const AlgorithmOptions&) const override {
                return dist + heuristic;
            }
        };

        class WeightedAStarAlgorithm final : public PrioritySearchAlgorithm {
        public:
            [[nodiscard]] AlgorithmKind Kind() const override { return AlgorithmKind::WeightedAStar; }
            [[nodiscard]] const char* Name() const override { return "Weighted A*"; }

        private:
            [[nodiscard]] double Priority(double dist, double heuristic, const AlgorithmOptions& options) const override {
                return dist + std::max(MIN_HEURISTIC_WEIGHT, options.heuristicWeight) * heuristic;
            }
        };

        class GreedyBestFirstAlgorithm final : public PrioritySearchAlgorithm {
        public:
            [[nodiscard]] AlgorithmKind Kind() const override { return AlgorithmKind::GreedyBestFirst; }
            [[nodiscard]] const char* Name() const override { return "Greedy best-first"; }

        private:
            [[nodiscard]] double Priority(double, double heuristic, const AlgorithmOptions&) const override { return heuristic; }
        };

    }

    const SearchAlgorithm& Pathfinder::Algorithm(AlgorithmKind algorithm) {
        static const BfsAlgorithm bfs;
        static const DijkstraAlgorithm dijkstra;
        static const AStarAlgorithm astar;
        static const WeightedAStarAlgorithm weightedAstar;
        static const GreedyBestFirstAlgorithm greedy;

        switch (algorithm) {
        case AlgorithmKind::BFS:             return bfs;
        case AlgorithmKind::AStar:           return astar;
        case AlgorithmKind::WeightedAStar:   return weightedAstar;
        case AlgorithmKind::GreedyBestFirst: return greedy;
        case AlgorithmKind::Dijkstra:
        default:                             return dijkstra;
        }
    }

    SearchResult Pathfinder::Run(const GridMap& map, AlgorithmKind algorithm, const AlgorithmOptions& options) {
        return Algorithm(algorithm).Run(map, options);
    }

    std::string Pathfinder::AlgorithmName(AlgorithmKind algorithm) {
        return Algorithm(algorithm).Name();
    }

}