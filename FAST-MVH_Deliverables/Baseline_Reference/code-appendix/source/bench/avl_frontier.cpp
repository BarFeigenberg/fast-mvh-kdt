// source/bench/avl_frontier.cpp
#include "bench/avl_frontier.hpp"
namespace rzq { namespace bench {
CostVec AVLFrontier::_lift(const CostVec& g) {
  CostVec out;
  out.reserve(g.size() + 1);
  out.push_back(0.0);
  for (size_t i = 0; i < g.size(); ++i) { out.push_back(g[i]); }
  return out;
}
bool AVLFrontier::Check(const CostVec& g) {
  return _f.Check(_lift(g)); // Frontier::Check applies _p, recovering g
}
void AVLFrontier::Update(const CostVec& g) {
  rzq::search::Label l;
  l.id = _id++;
  l.g = _lift(g);
  _f.Update(l); // Frontier::Update applies _p, then Filter+Add
}
} }
