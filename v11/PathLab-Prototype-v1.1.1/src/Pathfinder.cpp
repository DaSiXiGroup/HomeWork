#include "Pathfinder.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <utility>

namespace pathlab{

namespace{

using Clock = std::chrono::steady_clock;

const int    NO_NODE                = -1;
const double INF_DIST               = std::numeric_limits<double>::infinity();
const double BFS_EDGE_COST          = 1.0;
const double SQRT2                  = 1.4142135623730950488;
const double MIN_HEURISTIC_WEIGHT   = 1.0;

// zero addition, totally pure
// handmade Heap :)
struct HeapNode{
    double  score   = 0.0;
    double  dist    = 0.0;
    int     node    = NO_NODE;
    
    bool operator>(const HeapNode& other) const{
        return this->score != other.score ? this->score > other.score : this->node > other.node;
    }
};

// Convert open/closed marker array into a node list for visualization
std::vector<int> PickMarked(const std::vector<uint8_t>& mark){
    std::vector<int> nodes;
    nodes.reserve(mark.size());
    for(int i = 0, n = static_cast<int>(mark.size()); i < n; ++i){
        if(mark[static_cast<size_t>(i)]) nodes.push_back(i);
    }
    return nodes;
}

// Estimate grid distance to goal
// Manhattan for 4-neighbor, octile for diagonal movement
double Heuristic(const GridMap& map, int a, int b, bool diag){
    const GPos ca = map.Coord(a);
    const GPos cb = map.Coord(b);
    const double    dx = static_cast<double>(std::abs(ca.col - cb.col));
    const double    dy = static_cast<double>(std::abs(ca.row - cb.row));

    if(!diag) return dx + dy;
    const double diagonal = std::min(dx, dy);
    const double straight = std::max(dx, dy) - diagonal;
    return diagonal * SQRT2 + straight; // a trick from your dad SDLTF
}

// Rebuild final path by walking parent pointers backward
std::vector<int> MakePath(const std::vector<int>& parent, int start, int goal){
    std::vector<int> path;
    for(int u = goal; u != NO_NODE; u = parent[static_cast<size_t>(u)]){
        path.push_back(u);
        if(u == start) break;
    }
    if(path.empty() || path.back() != start) return {};
    std::reverse(path.begin(), path.end());
    return path;
}

// Recalculate final path cost for stats display
double PathCost(const GridMap& map, const std::vector<int>& path, const AlgOpt& opt, bool useBaseCost){
    if(path.size() < 2) return 0.0;

    double cost = 0.0;
    for(size_t i = 1; i < path.size(); ++i){
        const int u = path[i - 1], v = path[i];
        cost += useBaseCost ? 
                              map.BaseMoveCost(u, v)
                            : map.StepCost(u, v, opt.terrW, opt.hgtW, opt.prof);
    }
    return cost;
}

// SearchState keeps all temporary arrays and the frames we need for animation
struct SearchState{
    explicit SearchState(int nodeCount)
        : dist(static_cast<size_t>(nodeCount), INF_DIST),
          parent(static_cast<size_t>(nodeCount), NO_NODE),
          open(static_cast<size_t>(nodeCount), 0),
          closed(static_cast<size_t>(nodeCount), 0){}

    std::vector<double> dist;
    std::vector<int> parent;
    std::vector<uint8_t> open, closed;
    int step = 0;

    // Save one visualization frm
    // without this, animation has nothing to replay
    // memroibility come to support!
    void SaveFrame(SearchRes& result, int cur, bool finished, bool found, const std::vector<int>& path = {}) const{
        SearchFrm frm;
        frm.step      = step;
        frm.cur   = cur;
        frm.open      = PickMarked(open);
        frm.closed    = PickMarked(closed);
        frm.path      = path;
        frm.finished  = finished;
        frm.found     = found;
        result.frames.push_back(std::move(frm));
    }
};

// Create a result object with alg metadata filled in
SearchRes EmptyResult(const Alg& alg){
    SearchRes result;
    result.alg        = alg.Type();
    result.algName    = alg.Name();
    return result;
}

// BFS is the baseline search: cheap, honest, and completely ignores terrain cost
class BfsAlgorithm final : public Alg{
public:
    [[nodiscard]] AlgKind Type() const override{ return AlgKind::BFS; }
    [[nodiscard]] const char* Name() const override{ return "BFS"; }

    // Run BFS and record every useful step for animation
    [[nodiscard]] SearchRes Run(const GridMap& map, const AlgOpt& opt) const override{
        const auto begin    = Clock::now();
        SearchRes result = EmptyResult(*this);
        result.frames.reserve(static_cast<size_t>(map.Count()) + 1);
        SearchState state(map.Count());

        const int start = map.Start(), goal = map.Goal();
        if(!map.IsWalkable(start, opt.prof) || !map.IsWalkable(goal, opt.prof)){
            result.timeMs = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
            state.SaveFrame(result, NO_NODE, true, false);
            return result;
        }

        std::queue<int> q;
        q.push(start);
        state.dist[static_cast<size_t>(start)] = 0.0;
        state.open[static_cast<size_t>(start)] = 1;
        result.genNodes = 1;
        state.SaveFrame(result, start, false, false);

        while(!q.empty()){
            const int u = q.front(); q.pop();
            if(state.closed[static_cast<size_t>(u)]) continue;

            state.open[static_cast<size_t>(u)]      = 0;
            state.closed[static_cast<size_t>(u)]    = 1;
            ++state.step;
            ++result.expNodes;

            if(u == goal) break;

            for(const int v : map.Neighbors(u, opt.diag, opt.prof)){
                if(state.open[static_cast<size_t>(v)] || state.closed[static_cast<size_t>(v)]) continue;
                state.parent[static_cast<size_t>(v)]    = u;
                state.dist[static_cast<size_t>(v)]      = state.dist[static_cast<size_t>(u)] + BFS_EDGE_COST;
                state.open[static_cast<size_t>(v)]      = 1;
                q.push(v);
                ++result.genNodes;
            }
            state.SaveFrame(result, u, false, false);
        }

        result.finalPath    = MakePath(state.parent, start, goal);
        result.found        = !result.finalPath.empty();
        if(result.found) result.pathLen = PathCost(map, result.finalPath, opt, true);

        result.timeMs = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
        ++state.step;
        state.SaveFrame(result, result.found ? goal : NO_NODE, true, result.found, result.finalPath);
        return result;
    }
};

// Shared heap-search skeleton for Dijkstra, A* and greedy
class PrioAlg : public Alg{
public:
    // Run the shared priority-search loop and record visualization frames
    [[nodiscard]] SearchRes Run(const GridMap& map, const AlgOpt& opt) const override{
        const auto begin    = Clock::now();
        SearchRes result = EmptyResult(*this);
        result.frames.reserve(static_cast<size_t>(map.Count()) + 1);
        SearchState state(map.Count());

        const int start = map.Start(), goal = map.Goal();
        if(!map.IsWalkable(start, opt.prof) || !map.IsWalkable(goal, opt.prof)){
            result.timeMs = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
            state.SaveFrame(result, NO_NODE, true, false);
            return result;
        }

        std::priority_queue<HeapNode, std::vector<HeapNode>, std::greater<HeapNode>> heap;
        heap.push({ __Priority(0.0, Heuristic(map, start, goal, opt.diag), opt), 0.0, start });
        state.dist[static_cast<size_t>(start)] = 0.0;
        state.open[static_cast<size_t>(start)] = 1;
        result.genNodes = 1;
        state.SaveFrame(result, start, false, false);

        while(!heap.empty()){
            const HeapNode top = heap.top(); heap.pop();
            const int u = top.node;
            if(top.dist != state.dist[static_cast<size_t>(u)]) continue; // old heap node; lazy deletion saves us from writing decrease-key
            if(state.closed[static_cast<size_t>(u)]) continue;

            state.open[static_cast<size_t>(u)]      = 0;
            state.closed[static_cast<size_t>(u)]    = 1;
            ++state.step;
            ++result.expNodes;

            if(u == goal) break;

            for(const int v : map.Neighbors(u, opt.diag, opt.prof)){
                if(state.closed[static_cast<size_t>(v)]) continue;

                const double newDist = state.dist[static_cast<size_t>(u)]
                                     + map.StepCost(u, v, opt.terrW, opt.hgtW, opt.prof);
                if(newDist >= state.dist[static_cast<size_t>(v)]) continue;

                state.dist[static_cast<size_t>(v)]      = newDist;
                state.parent[static_cast<size_t>(v)]    = u;
                heap.push({ __Priority(newDist, Heuristic(map, v, goal, opt.diag), opt), newDist, v });

                if(!state.open[static_cast<size_t>(v)]){
                    state.open[static_cast<size_t>(v)] = 1;
                    ++result.genNodes;
                }
            }
            state.SaveFrame(result, u, false, false);
        }

        result.finalPath    = MakePath(state.parent, start, goal);
        result.found        = !result.finalPath.empty();
        if(result.found) result.pathLen = PathCost(map, result.finalPath, opt, false);

        result.timeMs = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
        ++state.step;
        state.SaveFrame(result, result.found ? goal : NO_NODE, true, result.found, result.finalPath);
        return result;
    }

private:
    // same heap, different f(x)
    // another trick from your dad SDLTF
    [[nodiscard]] virtual double __Priority(double dist, double heuristic, const AlgOpt& opt) const = 0;
};

// Dijkstra
class DijkstraAlg final : public PrioAlg{
public:
    [[nodiscard]] AlgKind Type() const override{ return AlgKind::Dijkstra; }
    [[nodiscard]] const char*   Name() const override{ return "Dijkstra"; }

private:
    [[nodiscard]] double __Priority(double dist, double, const AlgOpt&) const override{ return dist; }
};

// A*
class AStarAlg final : public PrioAlg{
public:
    [[nodiscard]] AlgKind Type() const override{ return AlgKind::AStar; }
    [[nodiscard]] const char*   Name() const override{ return "A*"; }

private:
    [[nodiscard]] double __Priority(double dist, double heuristic, const AlgOpt&) const override{ return dist + heuristic; }
};

// Weighted A*
class WAStarAlg final : public PrioAlg{
public:
    [[nodiscard]] AlgKind Type() const override{ return AlgKind::WAStar; }
    [[nodiscard]] const char*   Name() const override{ return "Weighted A*"; }

private:
    // Compute heap priority f(x)
    // each alg gets its own flavor.
    [[nodiscard]] double __Priority(double dist, double heuristic, const AlgOpt& opt) const override{
        return dist + std::max(MIN_HEURISTIC_WEIGHT, opt.hW) * heuristic;
    }
};

// Greedy best-first chases the goal and hopes the map is nice
// em, greedy is greedy
class GreedyAlg final : public PrioAlg{
public:
    [[nodiscard]] AlgKind Type() const override{ return AlgKind::Greedy; }
    [[nodiscard]] const char*   Name() const override{ return "Greedy best-first"; }

private:
    [[nodiscard]] double __Priority(double, double heuristic, const AlgOpt&) const override{ return heuristic; }
};

}

// Return the singleton alg object for the selected enum
const Alg& Pathfinder::__Algorithm(AlgKind alg){
    static const BfsAlgorithm bfs;
    static const DijkstraAlg dijkstra;
    static const AStarAlg astar;
    static const WAStarAlg weightedAstar;
    static const GreedyAlg greedy;

    switch(alg){
        case AlgKind::BFS:             return bfs;
        case AlgKind::AStar:           return astar;
        case AlgKind::WAStar:   return weightedAstar;
        case AlgKind::Greedy: return greedy;
        case AlgKind::Dijkstra:
        default:                             return dijkstra;
    }
}

// Public entry: run one alg on one map with the given opt.
SearchRes Pathfinder::Run(const GridMap& map, AlgKind alg, const AlgOpt& opt){
    return __Algorithm(alg).Run(map, opt);
}

// Public helper for UI text: enum in, alg name out.
std::string Pathfinder::AlgorithmName(AlgKind alg){
    return __Algorithm(alg).Name();
}

}
