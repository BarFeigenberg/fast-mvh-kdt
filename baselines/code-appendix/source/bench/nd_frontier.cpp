// source/bench/nd_frontier.cpp
#include "bench/nd_frontier.hpp"
#include "vec_type.hpp"
#include <algorithm>
#include <limits>
namespace rzq { namespace bench {
using rzq::basic::operator-;
void NDTreeFrontier::_free(Node* n) {
  if (!n) return;
  for (size_t i = 0; i < n->kids.size(); ++i) _free(n->kids[i]);
  delete n;
}
void NDTreeFrontier::_recompute(Node* n) {
  // Only the ideal (per-axis min) corner is needed for dominance-query pruning:
  // if the ideal corner cannot weakly dominate g, no contained point can.
  n->ideal.assign(_dim, std::numeric_limits<double>::infinity());
  if (n->leaf) {
    for (size_t i = 0; i < n->pts.size(); ++i)
      for (size_t d = 0; d < _dim; ++d)
        if (n->pts[i][d] < n->ideal[d]) n->ideal[d] = n->pts[i][d];
  } else {
    for (size_t k = 0; k < n->kids.size(); ++k)
      for (size_t d = 0; d < _dim; ++d)
        if (n->kids[k]->ideal[d] < n->ideal[d]) n->ideal[d] = n->kids[k]->ideal[d];
  }
}
bool NDTreeFrontier::_dominatedBy(Node* n, const CostVec& g) const {
  if (!n) return false;
  // prune: ideal corner must weakly dominate g, else no point here can.
  if (!basic::EpsDom(n->ideal, g)) return false;
  if (n->leaf) {
    for (size_t i = 0; i < n->pts.size(); ++i)
      if (basic::EpsDom(n->pts[i], g)) return true;
    return false;
  }
  for (size_t k = 0; k < n->kids.size(); ++k)
    if (_dominatedBy(n->kids[k], g)) return true;
  return false;
}
bool NDTreeFrontier::Check(const CostVec& g) { return _dominatedBy(_root, g); }

void NDTreeFrontier::_collect(Node* n, std::vector<CostVec>* out) const {
  if (!n) return;
  if (n->leaf) { for (size_t i=0;i<n->pts.size();++i) out->push_back(n->pts[i]); }
  else { for (size_t k=0;k<n->kids.size();++k) _collect(n->kids[k], out); }
}
// Farthest-point split: seed 0 = first point, seed 1 = farthest from seed 0,
// assign each point to nearest seed (L2), recurse if a child exceeds capacity.
NDTreeFrontier::Node* NDTreeFrontier::_bulk(std::vector<CostVec>& pts) {
  Node* n = new Node();
  if (pts.size() <= _leaf_cap) {
    n->leaf = true; n->pts = pts; _recompute(n); return n;
  }
  n->leaf = false;
  size_t s0 = 0, s1 = 0; double best = -1.0;
  for (size_t i = 1; i < pts.size(); ++i) {
    double dd = basic::NormL2(pts[i] - pts[s0]);
    if (dd > best) { best = dd; s1 = i; }
  }
  std::vector<std::vector<CostVec> > groups(2);
  for (size_t i = 0; i < pts.size(); ++i) {
    double d0 = basic::NormL2(pts[i] - pts[s0]);
    double d1 = basic::NormL2(pts[i] - pts[s1]);
    groups[(d1 < d0) ? 1 : 0].push_back(pts[i]);
  }
  if (groups[0].empty() || groups[1].empty()) { // degenerate: force a leaf
    delete n; Node* leaf = new Node(); leaf->leaf = true; leaf->pts = pts;
    _recompute(leaf); return leaf;
  }
  n->kids.push_back(_bulk(groups[0]));
  n->kids.push_back(_bulk(groups[1]));
  _recompute(n);
  return n;
}
void NDTreeFrontier::Update(const CostVec& g) {
  // Single rebuild: collect once, drop points dominated by g, append g, bulk-load.
  std::vector<CostVec> all; _collect(_root, &all);
  std::vector<CostVec> kept;
  kept.reserve(all.size() + 1);
  for (size_t i = 0; i < all.size(); ++i)
    if (!basic::EpsDom(g, all[i])) kept.push_back(all[i]);
  kept.push_back(g);
  _free(_root); _root = _bulk(kept); _size = kept.size();
}
} }
