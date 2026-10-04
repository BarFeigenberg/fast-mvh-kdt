// source/bench/linear_frontier.cpp
#include "bench/linear_frontier.hpp"
#include "vec_type.hpp"
namespace rzq { namespace bench {
bool LinearFrontier::Check(const CostVec& g) {
  for (size_t i = 0; i < _pts.size(); ++i) {
    if (basic::EpsDom(_pts[i], g)) { return true; }
  }
  return false;
}
void LinearFrontier::Update(const CostVec& g) {
  std::vector<CostVec> kept;
  kept.reserve(_pts.size() + 1);
  for (size_t i = 0; i < _pts.size(); ++i) {
    if (!basic::EpsDom(g, _pts[i])) { kept.push_back(_pts[i]); }
  }
  kept.push_back(g);
  _pts.swap(kept);
}
} }
