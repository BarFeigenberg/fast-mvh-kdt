// include/bench/nd_inc_frontier_t.hpp
//
// Incremental ND-tree frontier. The ND-tree (Jaszkiewicz & Lust 2018) was
// designed for incremental Pareto-archive maintenance, but NDTreeFrontierT
// rebuilds the whole tree on every Update. This backend updates in place,
// mirroring the incremental k-d tree (kd_inc_frontier_t.hpp):
//
//   * Insert: descend to the leaf whose ideal corner is nearest the new point
//     (L2), folding the point into each node's ideal/nadir corners along the
//     path, then append it; a leaf that overflows leaf_cap is split by the same
//     farthest-seed rule used for bulk construction. O(tree depth + leaf_cap).
//   * Delete: a nadir-pruned traversal removes every stored point the new
//     vector dominates. A subtree is skipped when the new vector does not
//     weakly dominate the subtree's nadir corner U(n): every point p there has
//     p <= U(n), so if z does not dominate U(n) it dominates no p. Removed
//     points are erased from their (small) leaf; the node corners are left
//     stale, which is conservative -- a not-yet-shrunk ideal only dominates
//     more queries (never wrongly prunes) and a not-yet-shrunk nadir is only
//     more likely to be visited. Corners are refreshed at the next rebuild.
//   * Rebuild: bulk reconstruction from the live points only when the live
//     count exceeds 2x or falls below 1/2 of the live count at the previous
//     build, bounding both insertion-driven imbalance and post-deletion
//     sparsity. Amortized O(log n) per update.
//
// Check (dominance query) is unchanged: prune a subtree when its ideal corner
// cannot weakly dominate the query. Correctness vs. a linear oracle and the
// Size() invariant follow the same argument as the incremental k-d tree.
#ifndef RZQ_BENCH_ND_INC_FRONTIER_T_H_
#define RZQ_BENCH_ND_INC_FRONTIER_T_H_
#include <stdexcept>
#include <algorithm>
#include <vector>
#include "bench/ifrontier.hpp"
#include "bench/cost_store.hpp"
#include "bench/dom_cmp.hpp"
namespace rzq { namespace bench {
template<typename Store, typename Cmp>
class NDIncFrontierT : public IFrontier {
  typedef typename Store::Point Point;
  struct Node {
    bool leaf; Point ideal; Point nadir; // per-axis min / max corners of the subtree
    std::vector<Point> pts;              // leaf payload (all live)
    std::vector<Node*> kids;             // internal children
    Node() : leaf(true) {}
  };
  static const int REBUILD_FACTOR = 2;
  static const int REBUILD_SLACK = 8;
public:
  explicit NDIncFrontierT(size_t dim, size_t leaf_cap = 8)
    : _dim(dim), _leaf_cap(leaf_cap) {
    if (dim == 0) throw std::invalid_argument("NDIncFrontierT: dim must be >= 1");
  }
  NDIncFrontierT(const NDIncFrontierT&) = delete;
  NDIncFrontierT& operator=(const NDIncFrontierT&) = delete;
  ~NDIncFrontierT() { _free(_root); }
  bool Check(const CostVec& g) override {
    if (g.empty()) return false;
    _d = g.size();
    Point gp = Store::convert(g);
    return _dominatedBy(_root, gp);
  }
  void Update(const CostVec& g) override {
    _d = g.size();
    Point gp = Store::convert(g);
    if (_root) _removeDominated(_root, gp); // drop points the new vector dominates
    _insert(gp);                            // then add the new vector
    if (_live > (size_t)REBUILD_FACTOR * _built + REBUILD_SLACK ||
        _built > (size_t)REBUILD_FACTOR * _live + REBUILD_SLACK) _rebuild();
  }
  size_t Size() const override { return _live; }
private:
  size_t _dim, _leaf_cap, _live = 0, _built = 0, _d = 0;
  Node* _root = NULL;
  void _free(Node* n) {
    if (!n) return;
    for (size_t i = 0; i < n->kids.size(); ++i) _free(n->kids[i]);
    delete n;
  }
  void _foldIn(Node* n, const Point& z) const {
    for (size_t d = 0; d < _dim; ++d) {
      if (z[d] < n->ideal[d]) n->ideal[d] = z[d];
      if (z[d] > n->nadir[d]) n->nadir[d] = z[d];
    }
  }
  void _recomputeLU(Node* n) const {
    if (n->leaf) { n->ideal = n->pts[0]; n->nadir = n->pts[0];
      for (size_t i = 1; i < n->pts.size(); ++i) _foldIn(n, n->pts[i]);
    } else { n->ideal = n->kids[0]->ideal; n->nadir = n->kids[0]->nadir;
      for (size_t k = 1; k < n->kids.size(); ++k)
        for (size_t d = 0; d < _dim; ++d) {
          if (n->kids[k]->ideal[d] < n->ideal[d]) n->ideal[d] = n->kids[k]->ideal[d];
          if (n->kids[k]->nadir[d] > n->nadir[d]) n->nadir[d] = n->kids[k]->nadir[d];
        }
    }
  }
  double _sqdist(const Point& a, const Point& b) const {
    double sq = 0.0;
    for (size_t d = 0; d < _dim; ++d) { double t = double(a[d]) - double(b[d]); sq += t*t; }
    return sq;
  }
  Node* _makeLeaf(std::vector<Point>& g) const {
    Node* n = new Node(); n->leaf = true; n->pts.swap(g); _recomputeLU(n); return n;
  }
  // Split an over-capacity leaf into two by the farthest-seed rule, recursing
  // while a child still overflows. If all points coincide the split is skipped.
  void _splitLeaf(Node* n) {
    std::vector<Point> pts; pts.swap(n->pts);
    size_t s0 = 0, s1 = 0; double best = -1.0;
    for (size_t i = 1; i < pts.size(); ++i) {
      double dd = _sqdist(pts[i], pts[s0]);
      if (dd > best) { best = dd; s1 = i; }
    }
    std::vector<Point> g0, g1;
    for (size_t i = 0; i < pts.size(); ++i)
      (_sqdist(pts[i], pts[s1]) < _sqdist(pts[i], pts[s0]) ? g1 : g0).push_back(pts[i]);
    if (g0.empty() || g1.empty()) { n->pts.swap(pts); return; } // identical points: keep oversized leaf
    n->leaf = false; n->kids.clear();
    Node* c0 = _makeLeaf(g0); Node* c1 = _makeLeaf(g1);
    n->kids.push_back(c0); n->kids.push_back(c1);
    _recomputeLU(n);
    if (c0->pts.size() > _leaf_cap) _splitLeaf(c0);
    if (c1->pts.size() > _leaf_cap) _splitLeaf(c1);
  }
  void _insert(const Point& z) {
    if (!_root) { _root = new Node(); _root->leaf = true; _root->pts.push_back(z);
      _root->ideal = z; _root->nadir = z; ++_live; return; }
    Node* n = _root;
    for (;;) {
      _foldIn(n, z);
      if (n->leaf) { n->pts.push_back(z); ++_live;
        if (n->pts.size() > _leaf_cap) _splitLeaf(n); break; }
      size_t best = 0; double bd = _sqdist(z, n->kids[0]->ideal);
      for (size_t k = 1; k < n->kids.size(); ++k) {
        double dd = _sqdist(z, n->kids[k]->ideal);
        if (dd < bd) { bd = dd; best = k; }
      }
      n = n->kids[best];
    }
  }
  // True iff some stored point weakly dominates g; prune subtrees whose ideal
  // corner cannot dominate g.
  bool _dominatedBy(Node* n, const Point& g) const {
    if (!n) return false;
    if (!Cmp::dominates(n->ideal, g, _d)) return false;
    if (n->leaf) {
      for (size_t i = 0; i < n->pts.size(); ++i)
        if (Cmp::dominates(n->pts[i], g, _d)) return true;
      return false;
    }
    for (size_t k = 0; k < n->kids.size(); ++k)
      if (_dominatedBy(n->kids[k], g)) return true;
    return false;
  }
  // Remove every stored point that z weakly dominates; prune subtrees whose
  // nadir corner z cannot dominate. Corners are left stale (conservative).
  void _removeDominated(Node* n, const Point& z) {
    if (!n) return;
    if (!Cmp::dominates(z, n->nadir, _d)) return;
    if (n->leaf) {
      size_t w = 0;
      for (size_t i = 0; i < n->pts.size(); ++i) {
        if (Cmp::dominates(z, n->pts[i], _d)) { --_live; }
        else { if (w != i) n->pts[w] = n->pts[i]; ++w; }
      }
      n->pts.resize(w);
      return;
    }
    for (size_t k = 0; k < n->kids.size(); ++k) _removeDominated(n->kids[k], z);
  }
  void _collect(Node* n, std::vector<Point>* out) const {
    if (!n) return;
    if (n->leaf) { for (size_t i = 0; i < n->pts.size(); ++i) out->push_back(n->pts[i]); }
    else { for (size_t k = 0; k < n->kids.size(); ++k) _collect(n->kids[k], out); }
  }
  // Bulk build from live points (farthest-seed binary split), setting both corners.
  Node* _bulk(std::vector<Point>& pts) const {
    Node* n = new Node();
    if (pts.size() <= _leaf_cap) { n->leaf = true; n->pts = pts; _recomputeLU(n); return n; }
    n->leaf = false;
    size_t s0 = 0, s1 = 0; double best = -1.0;
    for (size_t i = 1; i < pts.size(); ++i) {
      double dd = _sqdist(pts[i], pts[s0]);
      if (dd > best) { best = dd; s1 = i; }
    }
    std::vector<Point> g0, g1;
    for (size_t i = 0; i < pts.size(); ++i)
      (_sqdist(pts[i], pts[s1]) < _sqdist(pts[i], pts[s0]) ? g1 : g0).push_back(pts[i]);
    if (g0.empty() || g1.empty()) { delete n; Node* leaf = new Node();
      leaf->leaf = true; leaf->pts = pts; _recomputeLU(leaf); return leaf; }
    n->kids.push_back(_bulk(g0));
    n->kids.push_back(_bulk(g1));
    _recomputeLU(n);
    return n;
  }
  void _rebuild() {
    std::vector<Point> live; live.reserve(_live);
    _collect(_root, &live);
    _free(_root);
    _root = live.empty() ? NULL : _bulk(live);
    _built = live.size(); _live = live.size();
  }
};
} }
#endif
