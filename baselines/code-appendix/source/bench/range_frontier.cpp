// source/bench/range_frontier.cpp
#include "bench/range_frontier.hpp"
#include "vec_type.hpp"
#include <algorithm>
namespace rzq { namespace bench {
void RangeTreeFrontier::_free(Node* n) {
  if (!n) return;
  _free(n->left); _free(n->right); _free(n->assoc);
  delete n;
}
// ids must be sorted ascending by _pts[id][axis].
RangeTreeFrontier::Node* RangeTreeFrontier::_build(std::vector<int> ids, int axis) {
  if (ids.empty()) return NULL;
  Node* n = new Node();
  n->axis = axis;
  n->maxv = _pts[ids.back()][axis];
  if (axis == (int)_dim - 1) {
    double mn = _pts[ids[0]][axis];
    for (size_t i = 1; i < ids.size(); ++i)
      if (_pts[ids[i]][axis] < mn) mn = _pts[ids[i]][axis];
    n->submin_last = mn;
    n->mid = ids[ids.size() / 2];
    return n; // last axis: no children/assoc
  }
  size_t m = ids.size() / 2;
  n->mid = ids[m];
  std::vector<int> leftids(ids.begin(), ids.begin() + m);
  std::vector<int> rightids(ids.begin() + m + 1, ids.end());
  n->left = _build(leftids, axis);    // same axis (BST over axis values)
  n->right = _build(rightids, axis);
  std::vector<int> a1 = ids;          // assoc: same point set, next axis
  int na = axis + 1;
  std::sort(a1.begin(), a1.end(),
            [&](int x, int y) { return _pts[x][na] < _pts[y][na]; });
  n->assoc = _build(a1, na);
  return n;
}
// Contract: n's subtree points already satisfy axes [0..n->axis-1] <= g.
// Returns true iff some such point also has axes [n->axis..dim-1] <= g.
bool RangeTreeFrontier::_query(Node* n, const CostVec& g) const {
  if (!n) return false;
  int a = n->axis;
  if (a == (int)_dim - 1) {
    return n->submin_last <= g[a];
  }
  if (n->maxv <= g[a]) {
    // whole subtree qualifies on axis a; check remaining axes over all of it
    return _query(n->assoc, g);
  }
  double midv = _pts[n->mid][a];
  if (midv <= g[a]) {
    if (basic::EpsDom(_pts[n->mid], g)) return true;        // mid dominates fully
    if (n->left && _query(n->left->assoc, g)) return true;  // left fully-qualifies on axis a
    return _query(n->right, g);                              // right: partial on axis a
  }
  return _query(n->left, g);                                 // mid,right exceed g[a]
}
void RangeTreeFrontier::_rebuild() {
  _free(_root); _root = NULL;
  if (_pts.empty()) return;
  std::vector<int> ids(_pts.size());
  for (size_t i = 0; i < _pts.size(); ++i) ids[i] = (int)i;
  std::sort(ids.begin(), ids.end(),
            [&](int x, int y) { return _pts[x][0] < _pts[y][0]; });
  _root = _build(ids, 0);
}
bool RangeTreeFrontier::Check(const CostVec& g) { return _query(_root, g); }
void RangeTreeFrontier::Update(const CostVec& g) {
  std::vector<CostVec> kept;
  for (size_t i = 0; i < _pts.size(); ++i)
    if (!basic::EpsDom(g, _pts[i])) kept.push_back(_pts[i]);
  kept.push_back(g);
  _pts.swap(kept);
  _rebuild();
}
} }
