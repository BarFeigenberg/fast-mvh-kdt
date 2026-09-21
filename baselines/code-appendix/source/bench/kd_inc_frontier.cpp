// source/bench/kd_inc_frontier.cpp
#include "bench/kd_inc_frontier.hpp"
#include "vec_type.hpp"
#include <algorithm>
namespace rzq { namespace bench {

void KDIncFrontier::_free(Node* n) { if (!n) return; _free(n->l); _free(n->r); delete n; }

void KDIncFrontier::_initLeaf(Node* n, const CostVec& gp, int axis) {
  n->axis = axis; n->dead = false; n->pt = gp; n->lo = gp; n->hi = gp;
  n->l = NULL; n->r = NULL;
}
void KDIncFrontier::_insert(const CostVec& gp) {
  if (!_root) { _root = new Node(); _initLeaf(_root, gp, 0); ++_total; ++_live; return; }
  Node* n = _root;
  for (;;) {
    for (size_t d = 0; d < _dim; ++d) {
      if (gp[d] < n->lo[d]) n->lo[d] = gp[d];
      if (gp[d] > n->hi[d]) n->hi[d] = gp[d];
    }
    int a = n->axis;
    Node** child = (gp[a] < n->pt[a]) ? &n->l : &n->r;
    if (*child == NULL) { *child = new Node(); _initLeaf(*child, gp, (a + 1) % (int)_dim); break; }
    n = *child;
  }
  ++_total; ++_live;
}
bool KDIncFrontier::_query(Node* n, const CostVec& g) const {
  if (!n) return false;
  for (size_t d = 0; d < _dim; ++d) { RZQ_COORD_TICK(); if (n->lo[d] > g[d]) return false; }
  if (!n->dead && basic::EpsDom(n->pt, g)) return true;
  if (_query(n->l, g)) return true;
  return _query(n->r, g);
}
void KDIncFrontier::_markDominated(Node* n, const CostVec& gp) {
  if (!n) return;
  for (size_t d = 0; d < _dim; ++d) { RZQ_COORD_TICK(); if (n->hi[d] < gp[d]) return; }
  if (!n->dead && basic::EpsDom(gp, n->pt)) { n->dead = true; --_live; }
  _markDominated(n->l, gp);
  _markDominated(n->r, gp);
}
void KDIncFrontier::_collectLive(Node* n, std::vector<CostVec>& out) const {
  if (!n) return;
  if (!n->dead) out.push_back(n->pt);
  _collectLive(n->l, out);
  _collectLive(n->r, out);
}
KDIncFrontier::Node* KDIncFrontier::_build(std::vector<CostVec>& pts, size_t lo, size_t hi, int depth) {
  if (lo >= hi) return NULL;
  int axis = depth % (int)_dim;
  size_t mid = lo + (hi - lo) / 2;
  std::nth_element(pts.begin() + lo, pts.begin() + mid, pts.begin() + hi,
                   [&](const CostVec& a, const CostVec& b){ return a[axis] < b[axis]; });
  Node* n = new Node();
  n->axis = axis; n->dead = false; n->pt = pts[mid];
  n->l = _build(pts, lo, mid, depth + 1);
  n->r = _build(pts, mid + 1, hi, depth + 1);
  n->lo = n->pt; n->hi = n->pt;
  if (n->l) for (size_t d = 0; d < _dim; ++d) { n->lo[d] = std::min(n->lo[d], n->l->lo[d]); n->hi[d] = std::max(n->hi[d], n->l->hi[d]); }
  if (n->r) for (size_t d = 0; d < _dim; ++d) { n->lo[d] = std::min(n->lo[d], n->r->lo[d]); n->hi[d] = std::max(n->hi[d], n->r->hi[d]); }
  return n;
}
void KDIncFrontier::_rebuild() {
  std::vector<CostVec> live; live.reserve(_live);
  _collectLive(_root, live);
  _free(_root);
  _root = _build(live, 0, live.size(), 0);
  _total = live.size(); _live = live.size(); _built = _live;
}
bool KDIncFrontier::Check(const CostVec& g) {
  if (g.empty()) return false;
  return _query(_root, g);
}
void KDIncFrontier::Update(const CostVec& g) {
  if (_root) _markDominated(_root, g);
  _insert(g);
  if (_total > (size_t)REBUILD_FACTOR * _built + REBUILD_SLACK) _rebuild();
}
} }
