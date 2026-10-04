// include/bench/range_frontier_t.hpp
#ifndef RZQ_BENCH_RANGE_FRONTIER_T_H_
#define RZQ_BENCH_RANGE_FRONTIER_T_H_
#include <stdexcept>
#include <algorithm>
#include <vector>
#include "bench/ifrontier.hpp"
#include "bench/cost_store.hpp"
#include "bench/dom_cmp.hpp"
namespace rzq { namespace bench {
// Layered range tree for dominance-emptiness, rebuilt on batch.
template<typename Store, typename Cmp>
class RangeTreeFrontierT : public IFrontier {
  typedef typename Store::Point Point;
  struct Node {
    int axis;
    int mid;            // index into _pts of this node's median point
    double maxv;        // max axis-`axis` value over subtree
    double submin_last; // (last axis only) min axis-`axis` value over subtree
    Node* left; Node* right; Node* assoc;
    Node() : axis(0), mid(-1), maxv(0.0), submin_last(0.0),
             left(NULL), right(NULL), assoc(NULL) {}
  };
public:
  explicit RangeTreeFrontierT(size_t dim) : _dim(dim), _root(NULL) {
    if (dim == 0) throw std::invalid_argument("RangeTreeFrontierT: dim must be >= 1");
  }
  RangeTreeFrontierT(const RangeTreeFrontierT&) = delete;
  RangeTreeFrontierT& operator=(const RangeTreeFrontierT&) = delete;
  ~RangeTreeFrontierT() { _free(_root); }
  bool Check(const CostVec& g) override {
    if (g.empty()) return false;
    _d = g.size();
    Point gp = Store::convert(g); return _query(_root, gp);
  }
  void Update(const CostVec& g) override {
    _d = g.size(); Point gp = Store::convert(g);
    std::vector<Point> kept; kept.reserve(_pts.size() + 1);
    for (size_t i = 0; i < _pts.size(); ++i)
      if (!Cmp::dominates(gp, _pts[i], _d)) kept.push_back(_pts[i]);
    kept.push_back(gp); _pts.swap(kept); _rebuild();
  }
  size_t Size() const override { return _pts.size(); }
private:
  size_t _dim, _d = 0;
  std::vector<Point> _pts; // current non-dominated set
  Node* _root;
  void _free(Node* n) {
    if (!n) return;
    _free(n->left); _free(n->right); _free(n->assoc);
    delete n;
  }
  // ids must be sorted ascending by _pts[id][axis].
  Node* _build(std::vector<int> ids, int axis) {
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
  bool _query(Node* n, const Point& g) const {
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
      if (Cmp::dominates(_pts[n->mid], g, _d)) return true;    // mid dominates fully
      if (n->left && _query(n->left->assoc, g)) return true;   // left fully-qualifies on axis a
      return _query(n->right, g);                                // right: partial on axis a
    }
    return _query(n->left, g);                                   // mid,right exceed g[a]
  }
  void _rebuild() {
    _free(_root); _root = NULL;
    if (_pts.empty()) return;
    std::vector<int> ids(_pts.size());
    for (size_t i = 0; i < _pts.size(); ++i) ids[i] = (int)i;
    std::sort(ids.begin(), ids.end(),
              [&](int x, int y) { return _pts[x][0] < _pts[y][0]; });
    _root = _build(ids, 0);
  }
};
} }
#endif
