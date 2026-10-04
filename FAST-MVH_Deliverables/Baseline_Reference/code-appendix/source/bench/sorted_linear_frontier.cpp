// source/bench/sorted_linear_frontier.cpp
#include "bench/sorted_linear_frontier.hpp"
#include "vec_type.hpp"
#include <algorithm>
namespace rzq { namespace bench {

// Check: scan in lexicographic (ascending) order; stop as soon as a point's
// leading coordinate exceeds the query's, since every later point has a leading
// coordinate at least as large and so cannot dominate g. Points with leading
// coordinate <= g[0] are full-tested (each tick is one dominance check).
bool SortedLinearFrontier::Check(const CostVec& g) {
  for (size_t i = 0; i < _pts.size(); ++i) {
    if (_pts[i][0] > g[0]) break;                 // early termination on primary axis
    if (basic::EpsDom(_pts[i], g)) return true;
  }
  return false;
}

// Update: g has already passed Check (it is not dominated). Remove the points g
// dominates and insert g at its lexicographic position, keeping the antichain
// sorted. A point p can be dominated by g only if p[0] >= g[0], so the prefix
// with p[0] < g[0] is untouched (no dominance checks spent there) -- the
// symmetric early-start to Check's early-termination.
void SortedLinearFrontier::Update(const CostVec& g) {
  std::vector<CostVec> kept;
  kept.reserve(_pts.size() + 1);
  bool inserted = false;
  for (size_t i = 0; i < _pts.size(); ++i) {
    // insert g at the first position where g precedes the current point (lex)
    if (!inserted && basic::LexCompare(g, _pts[i]) < 0) {
      kept.push_back(g);
      inserted = true;
    }
    if (_pts[i][0] < g[0]) {                       // cannot be dominated by g
      kept.push_back(_pts[i]);
      continue;
    }
    if (!basic::EpsDom(g, _pts[i])) {              // survives (not dominated by g)
      kept.push_back(_pts[i]);
    }
  }
  if (!inserted) kept.push_back(g);
  _pts.swap(kept);
}

} }
