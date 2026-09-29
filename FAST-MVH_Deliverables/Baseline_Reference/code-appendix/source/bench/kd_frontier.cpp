// source/bench/kd_frontier.cpp
#include "bench/kd_frontier.hpp"
#include "vec_type.hpp"
#include <algorithm>
namespace rzq { namespace bench {
void KDFrontier::_free(Node* n) { if (!n) return; _free(n->l); _free(n->r); delete n; }

KDFrontier::Node* KDFrontier::_build(std::vector<int>& ids, int depth) {
  if (ids.empty()) return NULL;
  int axis = depth % (int)_dim;
  std::sort(ids.begin(), ids.end(),
            [&](int a, int b){ return _pts[a][axis] < _pts[b][axis]; });
  size_t mid = ids.size() / 2;
  Node* n = new Node();
  n->idx = ids[mid]; n->axis = axis;
  std::vector<int> left(ids.begin(), ids.begin() + mid);
  std::vector<int> right(ids.begin() + mid + 1, ids.end());
  n->l = _build(left, depth + 1);
  n->r = _build(right, depth + 1);
  // subtree min-corner
  n->lo = _pts[n->idx];
  if (n->l) for (size_t d = 0; d < _dim; ++d) n->lo[d] = std::min(n->lo[d], n->l->lo[d]);
  if (n->r) for (size_t d = 0; d < _dim; ++d) n->lo[d] = std::min(n->lo[d], n->r->lo[d]);
  return n;
}
void KDFrontier::_rebuild() {
  _free(_root); _root = NULL;
  std::vector<int> ids(_pts.size());
  for (size_t i = 0; i < _pts.size(); ++i) ids[i] = (int)i;
  _root = _build(ids, 0);
}
bool KDFrontier::_query(Node* n, const CostVec& g) const {
  if (!n) return false;
  // prune: if min-corner already exceeds g on any axis, no dominator below.
  for (size_t d = 0; d < _dim; ++d) { if (n->lo[d] > g[d]) return false; }
  if (basic::EpsDom(_pts[n->idx], g)) return true;
  if (_query(n->l, g)) return true;
  return _query(n->r, g);
}
bool KDFrontier::Check(const CostVec& g) { return _query(_root, g); }
void KDFrontier::Update(const CostVec& g) {
  std::vector<CostVec> kept;
  for (size_t i = 0; i < _pts.size(); ++i)
    if (!basic::EpsDom(g, _pts[i])) kept.push_back(_pts[i]);
  kept.push_back(g);
  _pts.swap(kept);
  _rebuild();
}
} }
