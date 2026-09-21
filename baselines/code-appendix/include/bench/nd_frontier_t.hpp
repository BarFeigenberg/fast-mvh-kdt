// include/bench/nd_frontier_t.hpp
#ifndef RZQ_BENCH_ND_FRONTIER_T_H_
#define RZQ_BENCH_ND_FRONTIER_T_H_
#include <stdexcept>
#include <algorithm>
#include <limits>
#include <vector>
#include "bench/ifrontier.hpp"
#include "bench/cost_store.hpp"
#include "bench/dom_cmp.hpp"
namespace rzq { namespace bench {
template<typename Store, typename Cmp>
class NDTreeFrontierT : public IFrontier {
  typedef typename Store::Point Point;
  struct Node {
    bool leaf; Point ideal;          // ideal = per-axis min corner (for query pruning)
    std::vector<Point> pts;          // leaf payload
    std::vector<Node*> kids;         // internal children
    Node() : leaf(true) {}
  };
public:
  explicit NDTreeFrontierT(size_t dim, size_t leaf_cap = 8, size_t max_children = 4)
    : _dim(dim), _leaf_cap(leaf_cap), _max_children(max_children) {
    if (dim == 0) throw std::invalid_argument("NDTreeFrontierT: dim must be >= 1");
  }
  NDTreeFrontierT(const NDTreeFrontierT&) = delete;
  NDTreeFrontierT& operator=(const NDTreeFrontierT&) = delete;
  ~NDTreeFrontierT() { _free(_root); }
  bool Check(const CostVec& g) override {
    if (g.empty()) return false;
    _d = g.size();
    Point gp = Store::convert(g); return _dominatedBy(_root, gp);
  }
  void Update(const CostVec& g) override {
    _d = g.size(); Point gp = Store::convert(g);
    std::vector<Point> all; _collect(_root, &all);
    std::vector<Point> kept; kept.reserve(all.size() + 1);
    for (size_t i = 0; i < all.size(); ++i)
      if (!Cmp::dominates(gp, all[i], _d)) kept.push_back(all[i]);
    kept.push_back(gp);
    _free(_root); _root = _bulk(kept); _size = kept.size();
  }
  size_t Size() const override { return _size; }
private:
  size_t _dim, _leaf_cap, _max_children, _size = 0, _d = 0;
  Node* _root = NULL;
  void _free(Node* n) {
    if (!n) return;
    for (size_t i = 0; i < n->kids.size(); ++i) _free(n->kids[i]);
    delete n;
  }
  void _recompute(Node* n) {
    // Only the ideal (per-axis min) corner is needed for dominance-query pruning:
    // if the ideal corner cannot weakly dominate g, no contained point can.
    // Seed n->ideal from an existing same-shaped Point so std::vector<Point>
    // gets sized to _dim (a no-op for fixed-size std::array Points), then
    // overwrite every element with the max sentinel before folding in minima.
    typedef typename Point::value_type Elem;
    if (n->leaf) { n->ideal = n->pts[0]; } else { n->ideal = n->kids[0]->ideal; }
    for (size_t d = 0; d < _dim; ++d) n->ideal[d] = std::numeric_limits<Elem>::max();
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
  bool _dominatedBy(Node* n, const Point& g) const {
    if (!n) return false;
    // prune: ideal corner must weakly dominate g, else no point here can.
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
  void _collect(Node* n, std::vector<Point>* out) const {
    if (!n) return;
    if (n->leaf) { for (size_t i=0;i<n->pts.size();++i) out->push_back(n->pts[i]); }
    else { for (size_t k=0;k<n->kids.size();++k) _collect(n->kids[k], out); }
  }
  // squared L2 distance over _dim components of two Points
  double _sqdist(const Point& a, const Point& b) const {
    double sq = 0.0;
    for (size_t d = 0; d < _dim; ++d) { double t = double(a[d]) - double(b[d]); sq += t*t; }
    return sq;
  }
  // Farthest-point split: seed 0 = first point, seed 1 = farthest from seed 0,
  // assign each point to nearest seed (L2), recurse if a child exceeds capacity.
  Node* _bulk(std::vector<Point>& pts) {
    Node* n = new Node();
    if (pts.size() <= _leaf_cap) {
      n->leaf = true; n->pts = pts; _recompute(n); return n;
    }
    n->leaf = false;
    size_t s0 = 0, s1 = 0; double best = -1.0;
    for (size_t i = 1; i < pts.size(); ++i) {
      double dd = _sqdist(pts[i], pts[s0]);
      if (dd > best) { best = dd; s1 = i; }
    }
    std::vector<std::vector<Point> > groups(2);
    for (size_t i = 0; i < pts.size(); ++i) {
      double d0 = _sqdist(pts[i], pts[s0]);
      double d1 = _sqdist(pts[i], pts[s1]);
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
};
} }
#endif
