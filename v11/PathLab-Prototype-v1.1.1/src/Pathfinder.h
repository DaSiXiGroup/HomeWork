#pragma once

#include "GridMap.h"

#include <string>
#include <vector>

namespace pathlab{

enum class AlgKind{
    BFS,
    Dijkstra,
    AStar,
    Greedy,
    WAStar
};

struct AlgOpt{
    bool    diag   = false;
    double  hW = 1.4;
    double  terrW   = 1.0;
    double  hgtW    = 0.25;
    MoveProfile prof = MoveProfile::Pedestrian;
};

struct SearchFrm{
    int step    = 0;
    int cur = -1;
    std::vector<int> open;
    std::vector<int> closed;
    std::vector<int> path;
    bool finished   = false;
    bool found      = false;
};

struct SearchRes{
    AlgKind alg = AlgKind::Dijkstra;
    std::string algName;
    std::vector<SearchFrm> frames;
    std::vector<int> finalPath;
    bool    found           = false;
    double  pathLen      = 0.0;
    int     expNodes   = 0;
    int     genNodes  = 0;
    double  timeMs       = 0.0;
};

class Alg{
public:
    virtual ~Alg() = default;
    [[nodiscard]] virtual AlgKind Type() const = 0;
    [[nodiscard]] virtual const char*   Name() const = 0;
    [[nodiscard]] virtual SearchRes  Run(const GridMap& map, const AlgOpt& opt) const = 0;
};

class Pathfinder{
public:
    static SearchRes Run(const GridMap& map, AlgKind alg, const AlgOpt& opt = {});
    static std::string  AlgorithmName(AlgKind alg);

private:
    // Keep the registry hidden; the UI only needs names and results, not the backstage drama.
    static const Alg& __Algorithm(AlgKind alg);
};

}
